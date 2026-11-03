#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <system_error>

#include <gtest/gtest.h>

#include "insights/insights.h"
#include "insights/reporter.h"
#include "insights/server.h"
#include "test_support.h"

namespace {

using insights::test::MakeRecord;

INSIGHTS_DECLARE_STATGROUP("Physics",   STATGROUP_PHYSICS);
INSIGHTS_DECLARE_STATGROUP("Rendering", STATGROUP_RENDERING);
INSIGHTS_DECLARE_STAT("Step", STAT_STEP, STATGROUP_PHYSICS);
INSIGHTS_DECLARE_STAT("Draw", STAT_DRAW, STATGROUP_RENDERING);

const insights::Descriptor& STAT_FRAME = insights::Descriptor::GetFrameDescriptor();

insights::FrameRecord MakeFrame(int64_t base_ms, uint32_t track_id = insights::TrackId::CPU_BASE) {
    return {
        MakeRecord(STAT_STEP,   base_ms + 1, base_ms + 3, 1, track_id),
        MakeRecord(STAT_DRAW,   base_ms + 3, base_ms + 4, 1, track_id),
        MakeRecord(STAT_FRAME,  base_ms,     base_ms + 5, 0, track_id),
    };
}

// Saves and loads through Server, the path the viewer uses for .trace files.
class SessionTest : public insights::test::InsightsTest {
protected:
    void SetUp() override {
        InsightsTest::SetUp();

        // Unique per process and test, so parallel ctest runs never share a file.
        std::string name = "insights-test-" + std::to_string(GetCurrentProcessId()) + "-" +
                           ::testing::UnitTest::GetInstance()->current_test_info()->name() + ".trace";
        path_ = std::filesystem::temp_directory_path() / name;
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
        std::filesystem::remove(std::filesystem::path(path_) += ".tmp", ec);

        InsightsTest::TearDown();
    }

    insights::Server&     server_   = insights::Server::GetInstance();
    insights::Registry&   registry_ = insights::Registry::GetInstance();
    insights::Reporter&   reporter_ = insights::Reporter::GetInstance();
    std::filesystem::path path_;
};

TEST_F(SessionTest, SaveAndLoadRestoresRegistry) {
    std::map<insights::Group::Id, std::pair<std::string, std::vector<insights::Descriptor::Id>>> groups;
    for (const auto& [id, group] : registry_.GetGroups()) {
        groups[id] = { group->GetName(), group->GetDescriptors() };
    }
    std::map<insights::Descriptor::Id, std::string> descs;
    for (const auto& [id, desc] : registry_.GetDescriptors()) {
        descs[id] = desc->GetName();
    }

    server_.SaveSession(path_);
    registry_.Clear();
    server_.LoadSession(path_);

    ASSERT_EQ(registry_.GetGroups().size(),      groups.size());
    ASSERT_EQ(registry_.GetDescriptors().size(), descs.size());
    for (const auto& [id, expected] : groups) {
        auto* group = registry_.FindGroup(id);
        ASSERT_NE(group, nullptr) << "group " << id;
        EXPECT_EQ(group->GetName(),        expected.first);
        EXPECT_EQ(group->GetDescriptors(), expected.second);
    }
    for (const auto& [id, name] : descs) {
        auto* desc = registry_.FindDescriptor(id);
        ASSERT_NE(desc, nullptr) << "descriptor " << id;
        EXPECT_EQ(desc->GetName(), name);
    }

    // Loaded entries are copies owned by the Server, not the static declarations.
    EXPECT_NE(registry_.FindGroup(STATGROUP_PHYSICS.GetId()), &STATGROUP_PHYSICS);
}

TEST_F(SessionTest, SaveAndLoadRestoresFrames) {
    const uint32_t gpu_track = insights::TrackId::GPU_BASE;
    std::vector<insights::FrameRecord> cpu_frames = { MakeFrame(0), MakeFrame(5) };
    std::vector<insights::FrameRecord> gpu_frames = { MakeFrame(1, gpu_track) };
    for (const auto& frame : cpu_frames) { reporter_.Submit(frame); }
    for (const auto& frame : gpu_frames) { reporter_.Submit(frame); }

    server_.SaveSession(path_);
    reporter_.Clear();
    server_.LoadSession(path_);

    ASSERT_EQ(reporter_.TotalSize(),                       3u);
    ASSERT_EQ(reporter_.Size(insights::TrackId::CPU_BASE), cpu_frames.size());
    ASSERT_EQ(reporter_.Size(gpu_track),                   gpu_frames.size());

    auto cpu_track = reporter_.GetTrack(insights::TrackId::CPU_BASE);
    for (size_t i = 0; i < cpu_frames.size(); ++i) {
        EXPECT_EQ(*cpu_track[i], cpu_frames[i]) << "cpu frame " << i;
    }
    EXPECT_EQ(*reporter_.GetTrack(gpu_track)[0], gpu_frames[0]);
}

TEST_F(SessionTest, LoadReplacesCurrentFrames) {
    reporter_.Submit(MakeFrame(0));
    server_.SaveSession(path_);
    reporter_.Submit(MakeFrame(5));
    reporter_.Submit(MakeFrame(10));

    server_.LoadSession(path_);

    EXPECT_EQ(reporter_.TotalSize(), 1u);
}

TEST_F(SessionTest, LoadRejectsBadMagic) {
    {
        std::ofstream file(path_, std::ios::binary);
        file << "not a trace file";
    }

    EXPECT_THROW(server_.LoadSession(path_), std::system_error);
    EXPECT_EQ(registry_.FindGroup(STATGROUP_PHYSICS.GetId()), &STATGROUP_PHYSICS);
}

TEST_F(SessionTest, LoadRejectsTruncatedFile) {
    reporter_.Submit(MakeFrame(0));
    server_.SaveSession(path_);
    std::filesystem::resize_file(path_, std::filesystem::file_size(path_) - 1);

    EXPECT_THROW(server_.LoadSession(path_), std::system_error);
}

TEST_F(SessionTest, LoadMissingFileThrows) {
    EXPECT_THROW(server_.LoadSession(path_), std::system_error);
}

} // namespace
