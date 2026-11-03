#include <gtest/gtest.h>

#include "insights/insights.h"
#include "test_support.h"

namespace {

INSIGHTS_DECLARE_STATGROUP("Physics",   STATGROUP_PHYSICS);
INSIGHTS_DECLARE_STATGROUP("Rendering", STATGROUP_RENDERING);

INSIGHTS_DECLARE_STAT("BVH Traverse", STAT_BVH,   STATGROUP_PHYSICS);
INSIGHTS_DECLARE_STAT("Broad Phase",  STAT_BROAD, STATGROUP_PHYSICS);
INSIGHTS_DECLARE_STAT("Draw Call",    STAT_DRAW,  STATGROUP_RENDERING);

class StatRegistryTest : public insights::test::InsightsTest {
protected:
    insights::Registry& registry_ = insights::Registry::GetInstance();
};

TEST_F(StatRegistryTest, GroupRegistered) {
    auto* group = registry_.FindGroup(STATGROUP_PHYSICS.GetId());
    ASSERT_NE(group, nullptr);
    EXPECT_EQ(group, &STATGROUP_PHYSICS);
    EXPECT_EQ(group->GetName(), "Physics");
}

TEST_F(StatRegistryTest, DescriptorRegistered) {
    auto* desc = registry_.FindDescriptor(STAT_BVH.GetId());
    ASSERT_NE(desc, nullptr);
    EXPECT_EQ(desc, &STAT_BVH);
    EXPECT_EQ(desc->GetName(), "BVH Traverse");
}

TEST_F(StatRegistryTest, GroupContainsDescriptors) {
    EXPECT_EQ(STATGROUP_PHYSICS.GetDescriptors(),
              (std::vector<insights::Descriptor::Id>{ STAT_BVH.GetId(), STAT_BROAD.GetId() }));
    EXPECT_EQ(STATGROUP_RENDERING.GetDescriptors(),
              (std::vector<insights::Descriptor::Id>{ STAT_DRAW.GetId() }));
}

TEST_F(StatRegistryTest, FrameDescriptorBelongsToFrameGroup) {
    auto& frame_desc  = insights::Descriptor::GetFrameDescriptor();
    auto& frame_group = insights::Group::GetFrameGroup();

    EXPECT_EQ(frame_desc.GetId(),  insights::Descriptor::FRAME_ID);
    EXPECT_EQ(frame_group.GetId(), insights::Group::FRAME_ID);
    EXPECT_EQ(registry_.FindDescriptor(insights::Descriptor::FRAME_ID), &frame_desc);
    EXPECT_EQ(registry_.FindGroup(insights::Group::FRAME_ID),           &frame_group);
    EXPECT_EQ(frame_group.GetDescriptors(),
              (std::vector<insights::Descriptor::Id>{ insights::Descriptor::FRAME_ID }));
}

TEST_F(StatRegistryTest, ConstructionRegistersAndJoinsGroup) {
    insights::Group      group("Audio");
    insights::Descriptor desc("Mix", group);

    EXPECT_NE(group.GetId(), STATGROUP_PHYSICS.GetId());
    EXPECT_NE(desc.GetId(),  STAT_BVH.GetId());
    EXPECT_EQ(registry_.FindGroup(group.GetId()),     &group);
    EXPECT_EQ(registry_.FindDescriptor(desc.GetId()), &desc);
    EXPECT_EQ(group.GetDescriptors(), (std::vector<insights::Descriptor::Id>{ desc.GetId() }));
}

TEST_F(StatRegistryTest, FindUnknownIdReturnsNull) {
    EXPECT_EQ(registry_.FindGroup(insights::Group::INVALID_ID),           nullptr);
    EXPECT_EQ(registry_.FindDescriptor(insights::Descriptor::INVALID_ID), nullptr);
}

TEST_F(StatRegistryTest, ClearRemovesEverything) {
    registry_.Clear();

    EXPECT_TRUE(registry_.GetGroups().empty());
    EXPECT_TRUE(registry_.GetDescriptors().empty());
    EXPECT_EQ(registry_.FindGroup(STATGROUP_PHYSICS.GetId()), nullptr);
}

} // namespace
