#include <chrono>
#include <set>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include "insights/reporter.h"
#include "insights/server.h"
#include "stub_session.h"
#include "test_support.h"

namespace {

using namespace std::chrono_literals;

// game-stub's declarations, plus the frame group and descriptor every client sends.
const std::set<std::string> STUB_GROUPS = { "Frame", "Physics", "Rendering", "AI" };
const std::set<std::string> STUB_DESCS  = {
    "Frame",
    "PhysicsUpdate", "BVHTraverse", "Collision", "DrawCall", "ShadowPass", "AIUpdate",
    "GpuShadow", "GpuOpaque", "GpuParticles",
};

// About 15 ms per frame, so the stub is still running when the test stops the session.
constexpr int STUB_FRAMES     = 200;
constexpr int FRAMES_TO_AWAIT = 3;

// -------------------------------------------------
// IpcEndToEndTest
// -------------------------------------------------
// The handshake replaces the Registry with the stub's declarations; InsightsTest
// restores it after the test.
class IpcEndToEndTest : public insights::test::InsightsTest {};

TEST_F(IpcEndToEndTest, StubStreamsFramesUntilSessionStops) {
    auto& server   = insights::Server::GetInstance();
    auto& reporter = insights::Reporter::GetInstance();
    auto  cpu      = insights::TrackId::CPU_BASE;

    insights::test::StubSession session(STUB_FRAMES);
    auto error = session.Start();
    ASSERT_TRUE(error.empty()) << error;

    ASSERT_TRUE(session.StartSessionOnConnect(5s)) << "stub did not connect";
    ASSERT_TRUE(insights::test::WaitUntil([&]() { return reporter.Size(cpu) >= FRAMES_TO_AWAIT; }, 5s))
        << "received " << reporter.Size(cpu) << " frames";

    ASSERT_TRUE(session.IsStubRunning()) << "the stub exited before the session stopped";
    server.StopSession();

    // A frame the receive thread was already submitting when the state changed may
    // still land; after that, the stub runs on for seconds without adding any.
    std::this_thread::sleep_for(100ms);
    size_t frames_at_stop = reporter.Size(cpu);

    // The handshake precedes every frame on the data pipe and is handled on the same
    // receive thread, so the Registry is complete once a frame has arrived.
    std::set<std::string> groups;
    for (auto& [id, group] : insights::Registry::GetInstance().GetGroups()) {
        groups.insert(group->GetName());
    }
    std::set<std::string> descs;
    for (auto& [id, desc] : insights::Registry::GetInstance().GetDescriptors()) {
        descs.insert(desc->GetName());
    }
    EXPECT_EQ(groups, STUB_GROUPS);
    EXPECT_EQ(descs, STUB_DESCS);

    EXPECT_EQ(session.WaitForStubExit(10s), 0);
    EXPECT_TRUE(session.WaitForDisconnect(5s)) << "server did not see the stub disconnect";
    EXPECT_EQ(reporter.Size(cpu), frames_at_stop) << "frames arrived after the session stopped";

    std::set<insights::Descriptor::Id> unknown_ids;
    for (const auto& frame : reporter.GetTrack(cpu)) {
        ASSERT_FALSE(frame->empty());
        EXPECT_EQ(frame->back().id, insights::Descriptor::FRAME_ID);
        EXPECT_EQ(frame->back().depth, 0);
        for (const auto& record : *frame) {
            if (!insights::Registry::GetInstance().FindDescriptor(record.id)) {
                unknown_ids.insert(record.id);
            }
        }
    }
    EXPECT_TRUE(unknown_ids.empty()) << unknown_ids.size() << " descriptor IDs in frames are not registered";
}

} // namespace
