#pragma once

#include <algorithm>
#include <ostream>
#include <vector>

#include <gtest/gtest.h>

#include "insights/insights.h"
#include "insights/reporter.h"

namespace insights {

// gtest needs these to compare and print records; production code has no use for them.
inline bool operator==(const ScopeRecord& lhs, const ScopeRecord& rhs) {
    return lhs.id       == rhs.id       &&
           lhs.start_ns == rhs.start_ns &&
           lhs.end_ns   == rhs.end_ns   &&
           lhs.depth    == rhs.depth    &&
           lhs.track_id == rhs.track_id;
}

inline void PrintTo(const ScopeRecord& record, std::ostream* os) {
    *os << "{ id: "       << record.id
        << ", start_ns: " << record.start_ns
        << ", end_ns: "   << record.end_ns
        << ", depth: "    << record.depth
        << ", track_id: " << record.track_id << " }";
}

namespace test {

// -------------------------------------------------
// InsightsTest
// -------------------------------------------------
// Base fixture for every suite. The singletons outlive a test, so each test
// starts from cleared Reporter and ScopeProfiler state, and the Registry is put
// back exactly as the static declarations left it, whatever the test did to it.
class InsightsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // The frame group and descriptor register themselves on first use. Touch
        // them before the snapshot so a test that first uses them cannot drop them.
        Descriptor::GetFrameDescriptor();

        auto& registry = Registry::GetInstance();
        saved_groups_  = registry.GetGroups();
        saved_descs_   = registry.GetDescriptors();

        ResetProfilers();
    }

    void TearDown() override {
        ResetProfilers();

        auto& registry = Registry::GetInstance();
        registry.Clear();
        for (auto& [id, group] : saved_groups_) { registry.RegisterGroup(group); }
        for (auto& [id, desc]  : saved_descs_)  { registry.RegisterDescriptor(desc); }
    }

private:
    static void ResetProfilers() {
        ScopeProfiler::GetInstance().EndRecording();
        ScopeProfiler::GetInstance().Clear();
        Reporter::GetInstance().Clear();
    }

    Registry::GroupMap      saved_groups_;
    Registry::DescriptorMap saved_descs_;
};

// -------------------------------------------------
// Helpers
// -------------------------------------------------
constexpr int64_t NS_PER_MS = 1'000'000;

inline ScopeRecord MakeRecord(const Descriptor& desc, int64_t start_ms, int64_t end_ms, int depth,
                              uint32_t track_id = TrackId::CPU_BASE) {
    return { desc.GetId(), start_ms * NS_PER_MS, end_ms * NS_PER_MS, depth, track_id };
}

// Lookups return nullptr when the entry is missing; callers ASSERT_NE on the
// result so a missing entry fails the test instead of skipping its checks.
inline const GroupSummary* FindGroup(const std::vector<GroupSummary>& groups, const Group& group) {
    auto it = std::find_if(groups.begin(), groups.end(), [&](const GroupSummary& summary) {
        return summary.group_id == group.GetId();
    });
    return it != groups.end() ? &*it : nullptr;
}

inline const StatSummary* FindStat(const GroupSummary& group, const Descriptor& desc) {
    auto it = std::find_if(group.stats.begin(), group.stats.end(), [&](const StatSummary& summary) {
        return summary.id == desc.GetId();
    });
    return it != group.stats.end() ? &*it : nullptr;
}

inline const StackSummary* FindStack(const std::vector<StackSummary>& stacks,
                                     const Descriptor& desc, Descriptor::Id parent_id) {
    auto it = std::find_if(stacks.begin(), stacks.end(), [&](const StackSummary& summary) {
        return summary.id == desc.GetId() && summary.parent_id == parent_id;
    });
    return it != stacks.end() ? &*it : nullptr;
}

} // namespace test
} // namespace insights
