#pragma once

#include <array>
#include <d3d11.h>

#include "insights/gpu/gpu_profiler_backend.h"

namespace insights {

// -------------------------------------------------
// D3D11GpuProfilerBackend
// -------------------------------------------------
class D3D11GpuProfilerBackend : public IGpuProfilerBackend {
public:
    D3D11GpuProfilerBackend(ID3D11Device* device, ID3D11DeviceContext* context);
    ~D3D11GpuProfilerBackend() override;

    D3D11GpuProfilerBackend(const D3D11GpuProfilerBackend&)            = delete;
    D3D11GpuProfilerBackend& operator=(const D3D11GpuProfilerBackend&) = delete;

    void BeginRecording()                    override;
    void BeginFrame()                        override;
    void EndFrame()                          override;
    int  BeginScope(const Descriptor& desc)  override;
    void EndScope(int handle)                override;

    std::vector<ScopeRecord> CollectFrame()  override;

private:
    static constexpr int FRAME_LATENCY        = 5;
    static constexpr int MAX_SCOPES_PER_FRAME = 256;

    struct ScopeEntry {
        Descriptor::Id id      = {};
        int            depth   = 0;
        ID3D11Query*   begin_q = nullptr;
        ID3D11Query*   end_q   = nullptr;
    };

    struct FrameSlot {
        ID3D11Query*                                 disjoint_q  = nullptr;
        std::array<ScopeEntry, MAX_SCOPES_PER_FRAME> scopes;
        int                                          scope_count = 0;
        int                                          scope_depth = 0;
        bool                                         active      = false;
    };

    ID3D11Device*        device_         = nullptr;
    ID3D11DeviceContext* context_        = nullptr;
    uint32_t             track_id_       = 0;
    FrameSlot            slots_[FRAME_LATENCY];
    int                  write_idx_      = 0;
    int                  frame_num_      = 0;

    UINT64               base_gpu_tick_  = 0;
};

} // namespace insights
