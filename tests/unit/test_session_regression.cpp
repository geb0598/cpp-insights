// Loads .trace files saved by earlier builds and compares every summary the viewer
// draws from them against committed text snapshots.
//
// Fixtures live in tests/fixtures/, one per FILE_VERSION (server.cpp), named
// <source>_v<FILE_VERSION>.trace. Stub timings vary between runs, so a fixture is
// recorded once and never regenerated. After a format change, record a new fixture
// and keep the old ones, so files saved by earlier builds stay covered.
// game_stub_v1.trace holds the CPU track of `game-stub --frames 125`, recorded by a
// Server that started the session as soon as the stub connected.
//
// Snapshots are <fixture>.<summary>.txt next to the fixture. After an intended change
// to the analysis, rewrite them, then review the diff before committing:
//   set INSIGHTS_UPDATE_SNAPSHOTS=1
//   build\debug\Debug\insights-test.exe --gtest_filter=*SessionRegressionTest*

#define NOMINMAX
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "insights/reporter.h"
#include "insights/server.h"
#include "test_support.h"

namespace {

// -------------------------------------------------
// Printing
// -------------------------------------------------
// Every value goes through one stream at a fixed precision of 1 ns, so snapshots do
// not depend on shortest-round-trip float formatting. Names stand in for IDs.
std::ostringstream MakeStream() {
    std::ostringstream os;
    os << std::fixed << std::setprecision(6);
    return os;
}

std::string DescName(insights::Descriptor::Id id) {
    if (id == insights::Descriptor::INVALID_ID) {
        return "-";
    }
    auto* desc = insights::Registry::GetInstance().FindDescriptor(id);
    return desc ? desc->GetName() : "?" + std::to_string(id);
}

std::string GroupName(insights::Group::Id id) {
    auto* group = insights::Registry::GetInstance().FindGroup(id);
    return group ? group->GetName() : "?" + std::to_string(id);
}

void PrintTiming(std::ostream& os, const insights::TimingSummary& timing) {
    os << "avg " << timing.avg_ms << " min " << timing.min_ms << " max " << timing.max_ms;
    os << " p95 ";
    if (timing.p95_ms) { os << *timing.p95_ms; } else { os << "-"; }
    os << " p99 ";
    if (timing.p99_ms) { os << *timing.p99_ms; } else { os << "-"; }
}

std::vector<uint32_t> SortedTrackIds() {
    auto ids = insights::Reporter::GetInstance().GetTrackIds();
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::string PrintGroups() {
    auto& reporter = insights::Reporter::GetInstance();
    auto  os       = MakeStream();
    for (uint32_t track_id : SortedTrackIds()) {
        size_t frame_count = reporter.Size(track_id);
        os << "track " << track_id << " frames " << frame_count << "\n";

        auto groups = reporter.SummarizeByGroup(frame_count, track_id);
        std::sort(groups.begin(), groups.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.group_id < rhs.group_id;
        });
        for (const auto& group : groups) {
            os << "  " << GroupName(group.group_id) << "\n";
            for (const auto& stat : group.stats) {
                os << "    " << DescName(stat.id) << " ";
                PrintTiming(os, stat.timing);
                os << "\n";
            }
        }
    }
    return os.str();
}

std::string PrintStacks() {
    auto& reporter = insights::Reporter::GetInstance();
    auto  os       = MakeStream();
    for (uint32_t track_id : SortedTrackIds()) {
        size_t frame_count = reporter.Size(track_id);
        os << "track " << track_id << " frames " << frame_count << "\n";

        auto stacks = reporter.SummarizeByStack(0, frame_count, track_id);
        std::sort(stacks.begin(), stacks.end(), [](const auto& lhs, const auto& rhs) {
            return std::tie(lhs.depth, lhs.parent_id, lhs.id) < std::tie(rhs.depth, rhs.parent_id, rhs.id);
        });
        for (const auto& stack : stacks) {
            os << "  " << DescName(stack.id) << " <- " << DescName(stack.parent_id)
               << " depth " << stack.depth << " count " << stack.count << "\n";
            os << "    inclusive ";
            PrintTiming(os, stack.inclusive);
            os << "\n    exclusive ";
            PrintTiming(os, stack.exclusive);
            os << "\n";
        }
    }
    return os.str();
}

std::string PrintTimeline() {
    auto& reporter = insights::Reporter::GetInstance();
    auto  os       = MakeStream();
    for (uint32_t track_id : SortedTrackIds()) {
        auto timeline = reporter.GetTimelineSummary(track_id);

        std::vector<insights::Descriptor::Id> ids;
        for (const auto& [id, values] : timeline.tracks) {
            ids.push_back(id);
        }
        std::sort(ids.begin(), ids.end());

        os << "track " << track_id << " frames " << timeline.total_frame_ms.size() << "\n";
        for (size_t i = 0; i < timeline.total_frame_ms.size(); ++i) {
            os << "  " << i << " total " << timeline.total_frame_ms[i]
               << " unaccounted " << timeline.unaccounted_ms[i];
            for (auto id : ids) {
                os << " " << DescName(id) << " " << timeline.tracks.at(id)[i];
            }
            os << "\n";
        }
    }
    return os.str();
}

std::string PrintFlame() {
    auto flame = insights::Reporter::GetInstance().GetFlameSummary();
    auto os    = MakeStream();
    os << "total_ms " << flame.total_ms << "\n";
    for (const auto& track : flame.tracks) {
        os << "track " << track.track_id << " " << track.label << " max_depth " << track.max_depth
           << " scopes " << track.scopes.size() << "\n";
        for (const auto& scope : track.scopes) {
            os << "  " << scope.frame_index << " " << scope.depth << " " << DescName(scope.id)
               << " " << scope.start_ms << " " << scope.end_ms << "\n";
        }
    }
    return os.str();
}

// -------------------------------------------------
// Snapshots
// -------------------------------------------------
// git may check the snapshots out with CRLF, so lines are compared without '\r'.
std::vector<std::string> SplitLines(std::istream& is) {
    std::vector<std::string> lines;
    std::string              line;
    while (std::getline(is, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    return lines;
}

bool IsUpdatingSnapshots() {
    const char* value = std::getenv("INSIGHTS_UPDATE_SNAPSHOTS");
    return value && std::string(value) == "1";
}

// -------------------------------------------------
// SessionRegressionTest
// -------------------------------------------------
// LoadSession replaces the Registry; InsightsTest restores it after each test.
class SessionRegressionTest : public insights::test::InsightsTest,
                              public ::testing::WithParamInterface<const char*> {
protected:
    void SetUp() override {
        InsightsTest::SetUp();
        ASSERT_NO_THROW(insights::Server::GetInstance().LoadSession(FixturePath(".trace")));
    }

    std::filesystem::path FixturePath(const std::string& suffix) const {
        return std::filesystem::path(INSIGHTS_FIXTURE_DIR) / (GetParam() + suffix);
    }

    void ExpectMatchesSnapshot(const std::string& summary, const std::string& actual) const {
        auto path = FixturePath("." + summary + ".txt");
        if (IsUpdatingSnapshots()) {
            std::ofstream(path, std::ios::binary) << actual;
            return;
        }

        std::ifstream file(path, std::ios::binary);
        ASSERT_TRUE(file) << "missing snapshot " << path << "; see the top of this file";
        std::istringstream actual_stream(actual);
        auto expected_lines = SplitLines(file);
        auto actual_lines   = SplitLines(actual_stream);

        size_t common = std::min(expected_lines.size(), actual_lines.size());
        for (size_t i = 0; i < common; ++i) {
            if (actual_lines[i] != expected_lines[i]) {
                ADD_FAILURE() << path.filename() << ":" << i + 1 << " differs\n"
                              << "  expected: " << expected_lines[i] << "\n"
                              << "  actual:   " << actual_lines[i];
                return;
            }
        }
        EXPECT_EQ(actual_lines.size(), expected_lines.size()) << path.filename() << ": line count";
    }
};

TEST_P(SessionRegressionTest, SummarizeByGroup) {
    ExpectMatchesSnapshot("group", PrintGroups());
}

TEST_P(SessionRegressionTest, SummarizeByStack) {
    ExpectMatchesSnapshot("stack", PrintStacks());
}

TEST_P(SessionRegressionTest, GetTimelineSummary) {
    ExpectMatchesSnapshot("timeline", PrintTimeline());
}

TEST_P(SessionRegressionTest, GetFlameSummary) {
    ExpectMatchesSnapshot("flame", PrintFlame());
}

INSTANTIATE_TEST_SUITE_P(Fixtures, SessionRegressionTest, ::testing::Values("game_stub_v1"),
                         [](const auto& info) { return std::string(info.param); });

} // namespace
