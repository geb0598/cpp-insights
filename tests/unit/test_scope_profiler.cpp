#include <gtest/gtest.h>

#include "insights/insights.h"
#include "test_support.h"

namespace {

INSIGHTS_DECLARE_STATGROUP("Test", STATGROUP_TEST);

INSIGHTS_DECLARE_STAT("Outer", STAT_OUTER, STATGROUP_TEST);
INSIGHTS_DECLARE_STAT("Inner", STAT_INNER, STATGROUP_TEST);
INSIGHTS_DECLARE_STAT("Next",  STAT_NEXT,  STATGROUP_TEST);

// Real-clock durations here may be 0 ns, so these tests check structure and
// ordering only. Exact timing math is covered by test_reporter.cpp.
class ScopeProfilerTest : public insights::test::InsightsTest {
protected:
    void SetUp() override {
        InsightsTest::SetUp();
        insights::ScopeProfiler::GetInstance().BeginRecording();
    }
};

TEST_F(ScopeProfilerTest, FrameRecordCount) {
    INSIGHTS_FRAME_BEGIN();
    {
        INSIGHTS_SCOPE(STAT_OUTER);
        {
            INSIGHTS_SCOPE(STAT_INNER);
        }
    }
    auto frame = insights::ScopeProfiler::GetInstance().EndFrame();

    EXPECT_EQ(frame.size(), 3u);
}

TEST_F(ScopeProfilerTest, DepthTracking) {
    INSIGHTS_FRAME_BEGIN();
    {
        INSIGHTS_SCOPE(STAT_OUTER);
        {
            INSIGHTS_SCOPE(STAT_INNER);
        }
    }
    auto frame = insights::ScopeProfiler::GetInstance().EndFrame();

    ASSERT_EQ(frame.size(),   3u);
    EXPECT_EQ(frame[0].id,    STAT_INNER.GetId());
    EXPECT_EQ(frame[0].depth, 2);
    EXPECT_EQ(frame[1].id,    STAT_OUTER.GetId());
    EXPECT_EQ(frame[1].depth, 1);
    EXPECT_EQ(frame[2].id,    insights::Descriptor::FRAME_ID);
    EXPECT_EQ(frame[2].depth, 0);
}

TEST_F(ScopeProfilerTest, ScopesAreNestedAndOrdered) {
    INSIGHTS_FRAME_BEGIN();
    {
        INSIGHTS_SCOPE(STAT_OUTER);
        {
            INSIGHTS_SCOPE(STAT_INNER);
        }
    }
    {
        INSIGHTS_SCOPE(STAT_NEXT);
    }
    auto frame = insights::ScopeProfiler::GetInstance().EndFrame();

    // Records are appended as scopes close: inner, outer, next, frame.
    ASSERT_EQ(frame.size(), 4u);
    const auto& inner = frame[0];
    const auto& outer = frame[1];
    const auto& next  = frame[2];
    const auto& whole = frame[3];
    ASSERT_EQ(inner.id, STAT_INNER.GetId());
    ASSERT_EQ(outer.id, STAT_OUTER.GetId());
    ASSERT_EQ(next.id,  STAT_NEXT.GetId());
    ASSERT_EQ(whole.id, insights::Descriptor::FRAME_ID);

    for (const auto& record : frame) {
        EXPECT_GE(record.start_ns, 0);
        EXPECT_GE(record.end_ns,   record.start_ns);
        EXPECT_EQ(record.track_id, insights::TrackId::CPU_BASE);
    }

    EXPECT_LE(whole.start_ns, outer.start_ns);
    EXPECT_LE(outer.start_ns, inner.start_ns);
    EXPECT_LE(inner.end_ns,   outer.end_ns);
    EXPECT_LE(outer.end_ns,   next.start_ns);
    EXPECT_LE(next.end_ns,    whole.end_ns);
}

TEST_F(ScopeProfilerTest, LaterFramesStartLater) {
    INSIGHTS_FRAME_BEGIN();
    auto first = insights::ScopeProfiler::GetInstance().EndFrame();
    INSIGHTS_FRAME_BEGIN();
    auto second = insights::ScopeProfiler::GetInstance().EndFrame();

    ASSERT_EQ(first.size(),  1u);
    ASSERT_EQ(second.size(), 1u);
    EXPECT_LE(first[0].end_ns, second[0].start_ns);
}

TEST_F(ScopeProfilerTest, FrameClearedOnBeginFrame) {
    INSIGHTS_FRAME_BEGIN();
    {
        INSIGHTS_SCOPE(STAT_OUTER);
    }
    INSIGHTS_FRAME_END();

    INSIGHTS_FRAME_BEGIN();
    auto frame = insights::ScopeProfiler::GetInstance().EndFrame();
    ASSERT_EQ(frame.size(), 1u);
    EXPECT_EQ(frame[0].id, insights::Descriptor::FRAME_ID);
}

TEST_F(ScopeProfilerTest, ScopeOutsideFrameIsIgnored) {
    {
        INSIGHTS_SCOPE(STAT_OUTER);
    }
    INSIGHTS_FRAME_BEGIN();
    auto frame = insights::ScopeProfiler::GetInstance().EndFrame();

    ASSERT_EQ(frame.size(), 1u);
    EXPECT_EQ(frame[0].id, insights::Descriptor::FRAME_ID);
}

TEST_F(ScopeProfilerTest, NothingRecordedAfterEndRecording) {
    insights::ScopeProfiler::GetInstance().EndRecording();

    INSIGHTS_FRAME_BEGIN();
    {
        INSIGHTS_SCOPE(STAT_OUTER);
    }
    auto frame = insights::ScopeProfiler::GetInstance().EndFrame();

    EXPECT_TRUE(frame.empty());
}

} // namespace
