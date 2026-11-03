#include <chrono>
#include <cstring>
#include <functional>
#include <thread>
#include <unordered_map>
#include <vector>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include <gtest/gtest.h>

#include "insights/insights_d3d11.h"
#include "forwarding_context.h"
#include "test_support.h"

namespace {

using Microsoft::WRL::ComPtr;

INSIGHTS_DECLARE_STATGROUP("GpuTest", STATGROUP_GPU_TEST);

INSIGHTS_DECLARE_STAT("Pass",  STAT_PASS,  STATGROUP_GPU_TEST);
INSIGHTS_DECLARE_STAT("Clear", STAT_CLEAR, STATGROUP_GPU_TEST);
INSIGHTS_DECLARE_STAT("Draw",  STAT_DRAW,  STATGROUP_GPU_TEST);

// Mirrors the private D3D11GpuProfilerBackend::FRAME_LATENCY; change both together.
constexpr size_t  FRAME_LATENCY      = 5;
// Each frame closes three GPU scopes: Pass, and Clear and Draw inside it.
constexpr size_t  SCOPES_PER_FRAME   = 3;
// Slack for rounding and clock skew when ticks are converted to ns.
constexpr int64_t CLOCK_TOLERANCE_NS = 1'000'000;
// The calibration tick is written before the backend sees it, and recording start is
// taken after that, so GPU times run late by that wait. WARP has taken up to 15 ms
// (one scheduler tick); the bound only has to catch gross errors.
constexpr int64_t CALIBRATION_LAG_NS = 100'000'000;

// A full-screen triangle from SV_VertexID, so the draw needs no buffers.
const char* SHADER_CODE = R"(
float4 VSMain(uint id : SV_VertexID) : SV_Position {
    float2 uv = float2((id << 1) & 2, id & 2);
    return float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
}
float4 PSMain() : SV_Target {
    return float4(1, 0, 0, 1);
}
)";

// -------------------------------------------------
// FaultContext
// -------------------------------------------------
// Rewrites query results the way a misbehaving GPU clock would, so calibration paths
// that WARP never takes can run. It relies on the backend calling Begin() only on
// disjoint queries and reading timestamps as UINT64.
class FaultContext : public insights::test::ForwardingContext {
public:
    using ForwardingContext::ForwardingContext;

    // Every timestamp read from now on is shifted by `ticks`.
    void SetTimestampShift(UINT64 ticks) {
        shift_ = ticks;
    }

    // The next disjoint query to begin reports Disjoint.
    void FlagNextDisjoint() {
        flag_next_ = true;
    }

    // From `now_tick` on the counter runs `factor` times faster and disjoint queries
    // begun after it report that frequency. The disjoint query open across the change
    // reports Disjoint, as the hardware would.
    void SpeedUpCounter(UINT64 factor, UINT64 now_tick) {
        if (open_disjoint_) {
            disjoints_[open_disjoint_].flagged = true;
        }
        factor_      = factor;
        change_tick_ = now_tick;
    }

    void STDMETHODCALLTYPE Begin(ID3D11Asynchronous* async) override {
        disjoints_[async] = { factor_, flag_next_ };
        flag_next_        = false;
        open_disjoint_    = async;
        inner_->Begin(async);
    }

    void STDMETHODCALLTYPE End(ID3D11Asynchronous* async) override {
        if (async == open_disjoint_) {
            open_disjoint_ = nullptr;
        }
        inner_->End(async);
    }

    HRESULT STDMETHODCALLTYPE GetData(
        ID3D11Asynchronous* async, void* data, UINT data_size, UINT get_data_flags) override {
        HRESULT hr = inner_->GetData(async, data, data_size, get_data_flags);
        if (hr != S_OK || !data) {
            return hr;
        }

        if (data_size == sizeof(D3D11_QUERY_DATA_TIMESTAMP_DISJOINT)) {
            auto it = disjoints_.find(async);
            if (it != disjoints_.end()) {
                auto* disjoint       = static_cast<D3D11_QUERY_DATA_TIMESTAMP_DISJOINT*>(data);
                disjoint->Frequency *= it->second.factor;
                if (it->second.flagged) {
                    disjoint->Disjoint = TRUE;
                }
            }
        } else if (data_size == sizeof(UINT64)) {
            auto* tick = static_cast<UINT64*>(data);
            if (*tick > change_tick_) {
                *tick = change_tick_ + (*tick - change_tick_) * factor_;
            }
            *tick += shift_;
        }
        return hr;
    }

private:
    struct DisjointState {
        UINT64 factor  = 1;
        bool   flagged = false;
    };

    std::unordered_map<ID3D11Asynchronous*, DisjointState> disjoints_;
    ID3D11Asynchronous*                                    open_disjoint_ = nullptr;
    bool                                                   flag_next_     = false;
    UINT64                                                 shift_         = 0;
    UINT64                                                 factor_        = 1;
    UINT64                                                 change_tick_   = 0;
};

// -------------------------------------------------
// GpuProfilerTest
// -------------------------------------------------
// Drives GpuProfiler and a D3D11GpuProfilerBackend on a WARP device with no swap chain.
// WARP timing is not stable, so tests check bounds and ordering, never durations.
class GpuProfilerTest : public insights::test::InsightsTest {
protected:
    struct Frame {
        int64_t               submit_ns  = 0;   // CPU time just before the first GPU scope
        int64_t               collect_ns = 0;   // CPU time just after GpuProfiler::EndFrame()
        insights::FrameRecord records;          // collected at this frame's end
    };

    void SetUp() override {
        InsightsTest::SetUp();

        const D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;
        ASSERT_HRESULT_SUCCEEDED(D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, &feature_level, 1, D3D11_SDK_VERSION,
            device_.GetAddressOf(), nullptr, context_.GetAddressOf()));

        D3D11_TEXTURE2D_DESC target_desc = {};
        target_desc.Width                = 64;
        target_desc.Height               = 64;
        target_desc.MipLevels            = 1;
        target_desc.ArraySize            = 1;
        target_desc.Format               = DXGI_FORMAT_R8G8B8A8_UNORM;
        target_desc.SampleDesc.Count     = 1;
        target_desc.Usage                = D3D11_USAGE_DEFAULT;
        target_desc.BindFlags            = D3D11_BIND_RENDER_TARGET;
        ComPtr<ID3D11Texture2D> target;
        ASSERT_HRESULT_SUCCEEDED(device_->CreateTexture2D(&target_desc, nullptr, target.GetAddressOf()));
        ASSERT_HRESULT_SUCCEEDED(device_->CreateRenderTargetView(target.Get(), nullptr, rtv_.GetAddressOf()));

        ComPtr<ID3DBlob> vs_blob;
        ComPtr<ID3DBlob> ps_blob;
        ASSERT_HRESULT_SUCCEEDED(D3DCompile(SHADER_CODE, std::strlen(SHADER_CODE), nullptr, nullptr, nullptr,
                                            "VSMain", "vs_5_0", 0, 0, vs_blob.GetAddressOf(), nullptr));
        ASSERT_HRESULT_SUCCEEDED(D3DCompile(SHADER_CODE, std::strlen(SHADER_CODE), nullptr, nullptr, nullptr,
                                            "PSMain", "ps_5_0", 0, 0, ps_blob.GetAddressOf(), nullptr));
        ASSERT_HRESULT_SUCCEEDED(device_->CreateVertexShader(
            vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), nullptr, vertex_shader_.GetAddressOf()));
        ASSERT_HRESULT_SUCCEEDED(device_->CreatePixelShader(
            ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), nullptr, pixel_shader_.GetAddressOf()));

        // The same hooks INSIGHTS_INITIALIZE() installs, without connecting a client.
        insights::ScopeProfiler::GetInstance().SetOnBeginRecording([]() {
            insights::GpuProfiler::GetInstance().BeginRecording();
        });
        insights::ScopeProfiler::GetInstance().SetOnEndRecording([]() {
            insights::GpuProfiler::GetInstance().EndRecording();
        });
    }

    void TearDown() override {
        // The backend releases its queries, so it must go before the device does.
        insights::GpuProfiler::GetInstance().EndRecording();
        insights::GpuProfiler::GetInstance().Init(nullptr);
        insights::ScopeProfiler::GetInstance().SetOnBeginRecording(nullptr);
        insights::ScopeProfiler::GetInstance().SetOnEndRecording(nullptr);

        InsightsTest::TearDown();
    }

    // Passing a FaultContext here lets a test alter what the backend reads.
    void InitBackend(ID3D11DeviceContext* context) {
        INSIGHTS_GPU_INIT_D3D11(device_.Get(), context);
    }

    void InitBackend() {
        InitBackend(context_.Get());
    }

    static void BeginRecording() {
        insights::ScopeProfiler::GetInstance().BeginRecording();
    }

    static int64_t NowNs() {
        auto start = insights::ScopeProfiler::GetInstance().GetRecordingStart();
        return insights::PlatformTime::Elapsed(start, insights::PlatformTime::Now()).count();
    }

    // Runs one frame the way INSIGHTS_FRAME_BEGIN/END do, with a clear and a draw inside
    // GPU scopes; `during` runs inside the frame after the GPU work. The frame then waits
    // for the GPU to go idle, as a vsync-bound game would, so the backend never drops a
    // frame for being late and a test sees the same frames on every run.
    void RunFrame(const std::function<void()>& during = {}) {
        Frame frame;
        INSIGHTS_FRAME_BEGIN();
        frame.submit_ns = NowNs();
        {
            INSIGHTS_GPU_SCOPE(STAT_PASS);
            {
                INSIGHTS_GPU_SCOPE(STAT_CLEAR);
                const float color[] = { 0.0f, 0.0f, 0.0f, 1.0f };
                context_->ClearRenderTargetView(rtv_.Get(), color);
            }
            {
                INSIGHTS_GPU_SCOPE(STAT_DRAW);
                D3D11_VIEWPORT viewport = { 0.0f, 0.0f, 64.0f, 64.0f, 0.0f, 1.0f };
                context_->RSSetViewports(1, &viewport);
                context_->OMSetRenderTargets(1, rtv_.GetAddressOf(), nullptr);
                context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                context_->VSSetShader(vertex_shader_.Get(), nullptr, 0);
                context_->PSSetShader(pixel_shader_.Get(), nullptr, 0);
                context_->Draw(3, 0);
            }
        }
        if (during) {
            during();
        }
        insights::ScopeProfiler::GetInstance().EndFrame();
        frame.records    = insights::GpuProfiler::GetInstance().EndFrame();
        frame.collect_ns = NowNs();
        WaitForGpu();
        frames_.push_back(std::move(frame));
    }

    void RunFrames(size_t count) {
        for (size_t i = 0; i < count; ++i) {
            RunFrame();
        }
    }

    // Records collected at frame `index` were recorded FRAME_LATENCY - 1 frames earlier.
    const Frame& SourceOf(size_t index) const {
        return frames_[index - (FRAME_LATENCY - 1)];
    }

    // GPU work cannot start before the CPU submitted it, nor end after its result was
    // read (give or take the calibration lag). `shift_ns` moves that window for a test
    // that skews the calibration tick.
    void ExpectWithinWindow(size_t index, int64_t shift_ns = 0) const {
        const auto& source = SourceOf(index);
        for (const auto& record : frames_[index].records) {
            EXPECT_GE(record.start_ns, source.submit_ns - shift_ns - CLOCK_TOLERANCE_NS)
                << "frame " << index;
            EXPECT_LE(record.end_ns, frames_[index].collect_ns - shift_ns + CALIBRATION_LAG_NS)
                << "frame " << index;
        }
    }

    UINT64 ReadGpuTick() {
        D3D11_QUERY_DESC     desc = { D3D11_QUERY_TIMESTAMP, 0 };
        ComPtr<ID3D11Query>  query;
        UINT64               tick = 0;
        EXPECT_HRESULT_SUCCEEDED(device_->CreateQuery(&desc, query.GetAddressOf()));
        context_->End(query.Get());
        while (context_->GetData(query.Get(), &tick, sizeof(tick), 0) == S_FALSE) {
            std::this_thread::yield();
        }
        return tick;
    }

    UINT64 ReadTickFrequency() {
        D3D11_QUERY_DESC                    desc     = { D3D11_QUERY_TIMESTAMP_DISJOINT, 0 };
        ComPtr<ID3D11Query>                 query;
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint = {};
        EXPECT_HRESULT_SUCCEEDED(device_->CreateQuery(&desc, query.GetAddressOf()));
        context_->Begin(query.Get());
        context_->End(query.Get());
        while (context_->GetData(query.Get(), &disjoint, sizeof(disjoint), 0) == S_FALSE) {
            std::this_thread::yield();
        }
        return disjoint.Frequency;
    }

    void WaitForGpu() {
        D3D11_QUERY_DESC    desc = { D3D11_QUERY_EVENT, 0 };
        ComPtr<ID3D11Query> query;
        BOOL                done = FALSE;
        ASSERT_HRESULT_SUCCEEDED(device_->CreateQuery(&desc, query.GetAddressOf()));
        context_->End(query.Get());
        while (context_->GetData(query.Get(), &done, sizeof(done), 0) == S_FALSE) {
            std::this_thread::yield();
        }
    }

    ComPtr<ID3D11Device>           device_;
    ComPtr<ID3D11DeviceContext>    context_;
    ComPtr<ID3D11RenderTargetView> rtv_;
    ComPtr<ID3D11VertexShader>     vertex_shader_;
    ComPtr<ID3D11PixelShader>      pixel_shader_;
    std::vector<Frame>             frames_;
};

// -------------------------------------------------
// Invariants
// -------------------------------------------------
TEST_F(GpuProfilerTest, RecordsAppearAfterFrameLatency) {
    InitBackend();
    BeginRecording();
    RunFrames(2 * FRAME_LATENCY);

    for (size_t i = 0; i < frames_.size(); ++i) {
        size_t expected = i < FRAME_LATENCY - 1 ? 0 : SCOPES_PER_FRAME;
        EXPECT_EQ(frames_[i].records.size(), expected) << "frame " << i;
    }
}

TEST_F(GpuProfilerTest, ScopesEndAfterTheyStart) {
    InitBackend();
    BeginRecording();
    RunFrames(2 * FRAME_LATENCY);

    for (const auto& frame : frames_) {
        for (const auto& record : frame.records) {
            EXPECT_GE(record.end_ns, record.start_ns);
        }
    }
}

TEST_F(GpuProfilerTest, ChildScopesLieWithinParent) {
    InitBackend();
    BeginRecording();
    RunFrames(2 * FRAME_LATENCY);

    for (size_t i = FRAME_LATENCY - 1; i < frames_.size(); ++i) {
        const auto& records = frames_[i].records;
        auto find = [&](const insights::Descriptor& desc) -> const insights::ScopeRecord* {
            for (const auto& record : records) {
                if (record.id == desc.GetId()) {
                    return &record;
                }
            }
            return nullptr;
        };
        const auto* pass  = find(STAT_PASS);
        const auto* clear = find(STAT_CLEAR);
        const auto* draw  = find(STAT_DRAW);
        ASSERT_NE(pass,  nullptr) << "frame " << i;
        ASSERT_NE(clear, nullptr) << "frame " << i;
        ASSERT_NE(draw,  nullptr) << "frame " << i;

        EXPECT_EQ(pass->depth,  0);
        EXPECT_EQ(clear->depth, 1);
        EXPECT_EQ(draw->depth,  1);
        for (const auto* child : { clear, draw }) {
            EXPECT_GE(child->start_ns, pass->start_ns) << "frame " << i;
            EXPECT_LE(child->end_ns,   pass->end_ns)   << "frame " << i;
        }
        EXPECT_LE(clear->end_ns, draw->start_ns) << "frame " << i;
    }
}

TEST_F(GpuProfilerTest, RecordsUseGpuTrack) {
    InitBackend();
    BeginRecording();
    RunFrames(2 * FRAME_LATENCY);

    for (const auto& frame : frames_) {
        for (const auto& record : frame.records) {
            EXPECT_GE(record.track_id, insights::TrackId::GPU_BASE);
        }
    }
}

TEST_F(GpuProfilerTest, StartFallsWithinWindowAfterRecordingStart) {
    InitBackend();
    BeginRecording();
    RunFrames(2 * FRAME_LATENCY);

    for (size_t i = FRAME_LATENCY - 1; i < frames_.size(); ++i) {
        ASSERT_FALSE(frames_[i].records.empty()) << "frame " << i;
        ExpectWithinWindow(i);
    }
}

// The calibration-defect tests below go through FaultContext; this shows that it
// changes nothing on its own, so their failures come from the fault they inject.
TEST_F(GpuProfilerTest, FaultContextWithoutFaultsIsTransparent) {
    FaultContext fault(context_.Get());
    InitBackend(&fault);
    BeginRecording();
    RunFrames(2 * FRAME_LATENCY);

    for (size_t i = FRAME_LATENCY - 1; i < frames_.size(); ++i) {
        ASSERT_EQ(frames_[i].records.size(), SCOPES_PER_FRAME) << "frame " << i;
        ExpectWithinWindow(i);
    }
}

// -------------------------------------------------
// Calibration defects
// -------------------------------------------------
// Each test below fails until its Bug issue is fixed; the fix removes the DISABLED_
// prefix. Run them with --gtest_also_run_disabled_tests.

// Bug: (draft, not filed yet) GPU base tick is not paired with a CPU instant.
// The backend takes its calibration tick as recording start. Here the session command
// lands between the GPU and CPU halves of INSIGHTS_FRAME_BEGIN, so the GPU calibrates
// one frame late and every GPU scope is drawn that much too early.
TEST_F(GpuProfilerTest, DISABLED_CalibrationPairsGpuTickWithCpuInstant) {
    InitBackend();

    insights::GpuProfiler::GetInstance().BeginFrame();
    BeginRecording();
    insights::ScopeProfiler::GetInstance().BeginFrame();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    insights::ScopeProfiler::GetInstance().EndFrame();
    EXPECT_TRUE(insights::GpuProfiler::GetInstance().EndFrame().empty());

    RunFrames(2 * FRAME_LATENCY);

    for (size_t i = FRAME_LATENCY - 1; i < frames_.size(); ++i) {
        ASSERT_FALSE(frames_[i].records.empty()) << "frame " << i;
        ExpectWithinWindow(i);
    }
}

// Bug: (draft, not filed yet) A failed calibration does not suppress GPU records.
// BeginRecording() returns early without a base tick, and CollectFrame() goes on
// converting ticks against the stale one (0 on a new backend).
TEST_F(GpuProfilerTest, DISABLED_FailedCalibrationSuppressesRecords) {
    FaultContext fault(context_.Get());
    InitBackend(&fault);
    fault.FlagNextDisjoint();    // the calibration's disjoint query
    BeginRecording();
    RunFrames(2 * FRAME_LATENCY);

    for (size_t i = 0; i < frames_.size(); ++i) {
        EXPECT_TRUE(frames_[i].records.empty()) << "frame " << i;
    }
}

// Bug: (draft, not filed yet) GPU tick deltas are unsigned.
// A tick earlier than the calibration tick must convert to a negative time. The
// subtraction is done in UINT64, so it wraps and the double-to-int64 cast overflows.
TEST_F(GpuProfilerTest, DISABLED_TickBeforeCalibrationConvertsToNegativeTime) {
    constexpr int64_t SHIFT_NS = 1'000'000'000;

    FaultContext fault(context_.Get());
    InitBackend(&fault);
    BeginRecording();
    fault.SetTimestampShift(ReadTickFrequency());    // the calibration tick reads 1 s late
    RunFrame();
    fault.SetTimestampShift(0);
    RunFrames(2 * FRAME_LATENCY);

    for (size_t i = FRAME_LATENCY - 1; i < frames_.size(); ++i) {
        ASSERT_FALSE(frames_[i].records.empty()) << "frame " << i;
        ExpectWithinWindow(i, SHIFT_NS);
    }
}

// Bug: (draft, not filed yet) The base tick is reused across disjoint intervals.
// Frequency only holds inside its own disjoint query, but every frame converts
// (tick - base) with its own Frequency. After the counter changes rate, later frames
// are scaled by the new rate over time that passed at the old one.
TEST_F(GpuProfilerTest, DISABLED_RecordsStayInWindowAfterCounterChange) {
    FaultContext fault(context_.Get());
    InitBackend(&fault);
    BeginRecording();
    RunFrames(FRAME_LATENCY);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    RunFrame([&] { fault.SpeedUpCounter(2, ReadGpuTick()); });
    const size_t changed = frames_.size() - 1;
    RunFrames(3 * FRAME_LATENCY);

    // A fix may drop frames until it recalibrates, but what it emits must be right,
    // and records must come back.
    for (size_t i = changed + FRAME_LATENCY; i < frames_.size(); ++i) {
        ExpectWithinWindow(i);
    }
    EXPECT_FALSE(frames_.back().records.empty());
}

} // namespace
