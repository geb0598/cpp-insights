#include <gtest/gtest.h>

#include "insights/insights.h"
#include "insights/reporter.h"
#include "test_support.h"

namespace {

using insights::test::FindGroup;
using insights::test::FindStack;
using insights::test::FindStat;
using insights::test::MakeRecord;

INSIGHTS_DECLARE_STATGROUP("Physics",   STATGROUP_PHYSICS);
INSIGHTS_DECLARE_STATGROUP("Rendering", STATGROUP_RENDERING);
INSIGHTS_DECLARE_STAT("Outer", STAT_OUTER, STATGROUP_PHYSICS);
INSIGHTS_DECLARE_STAT("Inner", STAT_INNER, STATGROUP_PHYSICS);
INSIGHTS_DECLARE_STAT("Draw",  STAT_DRAW,  STATGROUP_RENDERING);

const insights::Descriptor& STAT_FRAME = insights::Descriptor::GetFrameDescriptor();

// Records are listed in the order ScopeProfiler emits them: each scope after
// the scopes nested in it, the frame scope last.
//
//   Frame [0, 10] +- Outer [1, 7] -- Inner [2, 4]
//                 +- Draw  [7, 9]
//
// All times in ms, multiplied by scale and shifted by base_ms.
insights::FrameRecord MakeNestedFrame(int64_t base_ms, int64_t scale = 1,
                                      uint32_t track_id = insights::TrackId::CPU_BASE) {
    return {
        MakeRecord(STAT_INNER,  base_ms + 2 * scale, base_ms +  4 * scale, 2, track_id),
        MakeRecord(STAT_OUTER,  base_ms + 1 * scale, base_ms +  7 * scale, 1, track_id),
        MakeRecord(STAT_DRAW,   base_ms + 7 * scale, base_ms +  9 * scale, 1, track_id),
        MakeRecord(STAT_FRAME,  base_ms,             base_ms + 10 * scale, 0, track_id),
    };
}

// Frame [base, base + outer_ms + 1] -- Outer [base, base + outer_ms]
insights::FrameRecord MakeOuterFrame(int64_t base_ms, int64_t outer_ms) {
    return {
        MakeRecord(STAT_OUTER,  base_ms, base_ms + outer_ms,     1),
        MakeRecord(STAT_FRAME,  base_ms, base_ms + outer_ms + 1, 0),
    };
}

class ReporterTest : public insights::test::InsightsTest {
protected:
    insights::Reporter& reporter_ = insights::Reporter::GetInstance();

    void SubmitOuterFrames(const std::vector<int64_t>& outer_ms) {
        int64_t base_ms = 0;
        for (int64_t ms : outer_ms) {
            reporter_.Submit(MakeOuterFrame(base_ms, ms));
            base_ms += ms + 1;
        }
    }

    const insights::StatSummary* SummarizeOuter(size_t count) {
        groups_ = reporter_.SummarizeByGroup(count);
        auto* physics = FindGroup(groups_, STATGROUP_PHYSICS);
        return physics ? FindStat(*physics, STAT_OUTER) : nullptr;
    }

private:
    std::vector<insights::GroupSummary> groups_;
};

// -------------------------------------------------
// Submit
// -------------------------------------------------

TEST_F(ReporterTest, SubmitIncreasesSize) {
    reporter_.Submit(MakeNestedFrame(0));
    reporter_.Submit(MakeNestedFrame(10));

    EXPECT_EQ(reporter_.Size(),      2u);
    EXPECT_EQ(reporter_.TotalSize(), 2u);
}

TEST_F(ReporterTest, SubmitIgnoresEmptyFrame) {
    reporter_.Submit({});

    EXPECT_EQ(reporter_.TotalSize(), 0u);
    EXPECT_TRUE(reporter_.GetTrackIds().empty());
}

TEST_F(ReporterTest, SubmitSeparatesTracksByTrackId) {
    const uint32_t gpu_track = insights::TrackId::GPU_BASE;
    reporter_.Submit(MakeNestedFrame(0));
    reporter_.Submit(MakeNestedFrame(0, 1, gpu_track));
    reporter_.Submit(MakeNestedFrame(10, 1, gpu_track));

    EXPECT_EQ(reporter_.Size(),          1u);
    EXPECT_EQ(reporter_.Size(gpu_track), 2u);
    EXPECT_EQ(reporter_.TotalSize(),     3u);
    EXPECT_TRUE(reporter_.HasTrack(gpu_track));
    EXPECT_FALSE(reporter_.HasTrack(gpu_track + 1));
}

TEST_F(ReporterTest, ClearRemovesAllTracks) {
    reporter_.Submit(MakeNestedFrame(0));
    reporter_.Submit(MakeNestedFrame(0, 1, insights::TrackId::GPU_BASE));

    reporter_.Clear();

    EXPECT_EQ(reporter_.TotalSize(), 0u);
    EXPECT_TRUE(reporter_.GetTrackIds().empty());
}

// -------------------------------------------------
// SummarizeByGroup
// -------------------------------------------------

TEST_F(ReporterTest, SummarizeByGroupReturnsCorrectGroups) {
    reporter_.Submit(MakeNestedFrame(0));
    reporter_.Submit(MakeNestedFrame(10));

    auto groups = reporter_.SummarizeByGroup(2);
    ASSERT_EQ(groups.size(), 3u);

    auto* physics = FindGroup(groups, STATGROUP_PHYSICS);
    ASSERT_NE(physics, nullptr);
    ASSERT_EQ(physics->stats.size(), 2u);
    EXPECT_NE(FindStat(*physics, STAT_OUTER), nullptr);
    EXPECT_NE(FindStat(*physics, STAT_INNER), nullptr);

    auto* rendering = FindGroup(groups, STATGROUP_RENDERING);
    ASSERT_NE(rendering, nullptr);
    ASSERT_EQ(rendering->stats.size(), 1u);
    EXPECT_NE(FindStat(*rendering, STAT_DRAW), nullptr);

    auto* frame = FindGroup(groups, insights::Group::GetFrameGroup());
    ASSERT_NE(frame, nullptr);
    ASSERT_EQ(frame->stats.size(), 1u);
    EXPECT_NE(FindStat(*frame, STAT_FRAME), nullptr);
}

TEST_F(ReporterTest, SummarizeByGroupTimingIsExact) {
    SubmitOuterFrames({ 3, 1, 5, 2, 4 });

    auto* outer = SummarizeOuter(5);
    ASSERT_NE(outer, nullptr);
    EXPECT_DOUBLE_EQ(outer->timing.avg_ms, 3.0);
    EXPECT_DOUBLE_EQ(outer->timing.min_ms, 1.0);
    EXPECT_DOUBLE_EQ(outer->timing.max_ms, 5.0);
    EXPECT_FALSE(outer->timing.p95_ms.has_value());
    EXPECT_FALSE(outer->timing.p99_ms.has_value());
}

TEST_F(ReporterTest, SummarizeByGroupUsesLatestFrames) {
    SubmitOuterFrames({ 3, 1, 5, 2, 4 });

    auto* outer = SummarizeOuter(3);
    ASSERT_NE(outer, nullptr);
    EXPECT_DOUBLE_EQ(outer->timing.avg_ms, 11.0 / 3.0);
    EXPECT_DOUBLE_EQ(outer->timing.min_ms, 2.0);
    EXPECT_DOUBLE_EQ(outer->timing.max_ms, 5.0);
}

TEST_F(ReporterTest, SummarizeByGroupClampsCountToFrameCount) {
    SubmitOuterFrames({ 3, 1, 5, 2, 4 });

    auto* outer = SummarizeOuter(100);
    ASSERT_NE(outer, nullptr);
    EXPECT_DOUBLE_EQ(outer->timing.avg_ms, 3.0);
}

TEST_F(ReporterTest, PercentilesNeedTwentySamples) {
    SubmitOuterFrames(std::vector<int64_t>(19, 1));
    auto* outer = SummarizeOuter(19);
    ASSERT_NE(outer, nullptr);
    EXPECT_FALSE(outer->timing.p95_ms.has_value());
    EXPECT_FALSE(outer->timing.p99_ms.has_value());

    SubmitOuterFrames({ 1 });
    outer = SummarizeOuter(20);
    ASSERT_NE(outer, nullptr);
    EXPECT_TRUE(outer->timing.p95_ms.has_value());
    EXPECT_TRUE(outer->timing.p99_ms.has_value());
}

TEST_F(ReporterTest, PercentilesIndexSortedSamples) {
    // 30 ms down to 1 ms, so the summary has to sort before indexing.
    std::vector<int64_t> outer_ms;
    for (int64_t ms = 30; ms >= 1; --ms) {
        outer_ms.push_back(ms);
    }
    SubmitOuterFrames(outer_ms);

    auto* outer = SummarizeOuter(30);
    ASSERT_NE(outer, nullptr);
    EXPECT_DOUBLE_EQ(outer->timing.avg_ms, 15.5);
    EXPECT_DOUBLE_EQ(outer->timing.min_ms, 1.0);
    EXPECT_DOUBLE_EQ(outer->timing.max_ms, 30.0);
    // Index floor(30 * 0.95) = 28 and floor(30 * 0.99) = 29.
    ASSERT_TRUE(outer->timing.p95_ms.has_value());
    ASSERT_TRUE(outer->timing.p99_ms.has_value());
    EXPECT_DOUBLE_EQ(*outer->timing.p95_ms, 29.0);
    EXPECT_DOUBLE_EQ(*outer->timing.p99_ms, 30.0);
}

TEST_F(ReporterTest, SummarizeByGroupOnMissingTrackIsEmpty) {
    reporter_.Submit(MakeNestedFrame(0));

    EXPECT_TRUE(reporter_.SummarizeByGroup(1, insights::TrackId::GPU_BASE).empty());
}

// -------------------------------------------------
// SummarizeByStack
// -------------------------------------------------

TEST_F(ReporterTest, SummarizeByStackInclusiveAndExclusive) {
    reporter_.Submit(MakeNestedFrame(0,  1));
    reporter_.Submit(MakeNestedFrame(10, 2));

    auto stacks = reporter_.SummarizeByStack(0, 2);
    ASSERT_EQ(stacks.size(), 4u);

    auto* frame = FindStack(stacks, STAT_FRAME, insights::Descriptor::INVALID_ID);
    ASSERT_NE(frame, nullptr);
    EXPECT_EQ(frame->depth, 0);
    EXPECT_EQ(frame->count, 2u);
    EXPECT_DOUBLE_EQ(frame->inclusive.avg_ms, 15.0);
    EXPECT_DOUBLE_EQ(frame->inclusive.min_ms, 10.0);
    EXPECT_DOUBLE_EQ(frame->inclusive.max_ms, 20.0);
    EXPECT_DOUBLE_EQ(frame->exclusive.avg_ms,  3.0);
    EXPECT_DOUBLE_EQ(frame->exclusive.min_ms,  2.0);
    EXPECT_DOUBLE_EQ(frame->exclusive.max_ms,  4.0);

    auto* outer = FindStack(stacks, STAT_OUTER, STAT_FRAME.GetId());
    ASSERT_NE(outer, nullptr);
    EXPECT_EQ(outer->depth, 1);
    EXPECT_EQ(outer->count, 2u);
    EXPECT_DOUBLE_EQ(outer->inclusive.avg_ms,  9.0);
    EXPECT_DOUBLE_EQ(outer->inclusive.min_ms,  6.0);
    EXPECT_DOUBLE_EQ(outer->inclusive.max_ms, 12.0);
    EXPECT_DOUBLE_EQ(outer->exclusive.avg_ms,  6.0);
    EXPECT_DOUBLE_EQ(outer->exclusive.min_ms,  4.0);
    EXPECT_DOUBLE_EQ(outer->exclusive.max_ms,  8.0);

    auto* inner = FindStack(stacks, STAT_INNER, STAT_OUTER.GetId());
    ASSERT_NE(inner, nullptr);
    EXPECT_EQ(inner->depth, 2);
    EXPECT_EQ(inner->count, 2u);
    EXPECT_DOUBLE_EQ(inner->inclusive.avg_ms, 3.0);
    EXPECT_DOUBLE_EQ(inner->exclusive.avg_ms, 3.0);

    auto* draw = FindStack(stacks, STAT_DRAW, STAT_FRAME.GetId());
    ASSERT_NE(draw, nullptr);
    EXPECT_EQ(draw->depth, 1);
    EXPECT_EQ(draw->count, 2u);
    EXPECT_DOUBLE_EQ(draw->inclusive.avg_ms, 3.0);
    EXPECT_DOUBLE_EQ(draw->exclusive.avg_ms, 3.0);
}

TEST_F(ReporterTest, SummarizeByStackSeparatesParents) {
    //   Frame [0, 10] +- Outer [0, 4] -- Inner [1, 3]
    //                 +- Inner [5, 6]
    reporter_.Submit({
        MakeRecord(STAT_INNER,  1,  3, 2),
        MakeRecord(STAT_OUTER,  0,  4, 1),
        MakeRecord(STAT_INNER,  5,  6, 1),
        MakeRecord(STAT_FRAME,  0, 10, 0),
    });

    auto stacks = reporter_.SummarizeByStack(0, 1);
    ASSERT_EQ(stacks.size(), 4u);

    auto* nested = FindStack(stacks, STAT_INNER, STAT_OUTER.GetId());
    ASSERT_NE(nested, nullptr);
    EXPECT_EQ(nested->depth, 2);
    EXPECT_DOUBLE_EQ(nested->inclusive.avg_ms, 2.0);

    auto* top = FindStack(stacks, STAT_INNER, STAT_FRAME.GetId());
    ASSERT_NE(top, nullptr);
    EXPECT_EQ(top->depth, 1);
    EXPECT_DOUBLE_EQ(top->inclusive.avg_ms, 1.0);

    auto* frame = FindStack(stacks, STAT_FRAME, insights::Descriptor::INVALID_ID);
    ASSERT_NE(frame, nullptr);
    EXPECT_DOUBLE_EQ(frame->exclusive.avg_ms, 5.0);
}

TEST_F(ReporterTest, SummarizeByStackUsesFrameRange) {
    reporter_.Submit(MakeNestedFrame(0,  1));
    reporter_.Submit(MakeNestedFrame(10, 2));
    reporter_.Submit(MakeNestedFrame(30, 3));

    auto middle = reporter_.SummarizeByStack(1, 2);
    auto* outer = FindStack(middle, STAT_OUTER, STAT_FRAME.GetId());
    ASSERT_NE(outer, nullptr);
    EXPECT_EQ(outer->count, 1u);
    EXPECT_DOUBLE_EQ(outer->inclusive.avg_ms, 12.0);

    auto tail = reporter_.SummarizeByStack(1, 100);
    outer = FindStack(tail, STAT_OUTER, STAT_FRAME.GetId());
    ASSERT_NE(outer, nullptr);
    EXPECT_EQ(outer->count, 2u);
    EXPECT_DOUBLE_EQ(outer->inclusive.avg_ms, 15.0);

    EXPECT_TRUE(reporter_.SummarizeByStack(3, 4).empty());
}

// -------------------------------------------------
// GetTimelineSummary
// -------------------------------------------------

TEST_F(ReporterTest, TimelineSummaryTracksTopLevelScopes) {
    reporter_.Submit(MakeNestedFrame(0,  1));
    reporter_.Submit(MakeNestedFrame(10, 2));

    auto timeline = reporter_.GetTimelineSummary();

    EXPECT_EQ(timeline.total_frame_ms, (std::vector<float>{ 10.0f, 20.0f }));
    EXPECT_EQ(timeline.unaccounted_ms, (std::vector<float>{  2.0f,  4.0f }));

    // Only depth-1 scopes get a timeline track.
    ASSERT_EQ(timeline.tracks.size(), 2u);
    auto outer = timeline.tracks.find(STAT_OUTER.GetId());
    ASSERT_NE(outer, timeline.tracks.end());
    EXPECT_EQ(outer->second, (std::vector<float>{ 6.0f, 12.0f }));
    auto draw = timeline.tracks.find(STAT_DRAW.GetId());
    ASSERT_NE(draw, timeline.tracks.end());
    EXPECT_EQ(draw->second, (std::vector<float>{ 2.0f, 4.0f }));
}

// -------------------------------------------------
// GetFlameSummary
// -------------------------------------------------

TEST_F(ReporterTest, FlameSummaryKeepsAbsoluteTimesPerTrack) {
    const uint32_t gpu_track = insights::TrackId::GPU_BASE;
    reporter_.Submit(MakeNestedFrame(0, 1, gpu_track));
    reporter_.Submit(MakeNestedFrame(0));
    reporter_.Submit(MakeNestedFrame(10));

    auto flame = reporter_.GetFlameSummary();
    ASSERT_EQ(flame.tracks.size(), 2u);

    const auto& cpu = flame.tracks[0];
    EXPECT_EQ(cpu.track_id,      insights::TrackId::CPU_BASE);
    EXPECT_EQ(cpu.label,         "CPU #0");
    EXPECT_EQ(cpu.max_depth,     2);
    ASSERT_EQ(cpu.scopes.size(), 8u);

    // Second frame's Outer scope: [11, 17] ms, unshifted.
    const auto& outer = cpu.scopes[5];
    EXPECT_EQ(outer.id,          STAT_OUTER.GetId());
    EXPECT_EQ(outer.frame_index, 1u);
    EXPECT_EQ(outer.depth,       1);
    EXPECT_DOUBLE_EQ(outer.start_ms, 11.0);
    EXPECT_DOUBLE_EQ(outer.end_ms,   17.0);

    const auto& gpu = flame.tracks[1];
    EXPECT_EQ(gpu.track_id,      gpu_track);
    EXPECT_EQ(gpu.label,         "GPU #0");
    EXPECT_EQ(gpu.scopes.size(), 4u);
}

} // namespace
