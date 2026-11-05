// Records a .trace fixture for SessionRegressionTest; usage is at the top of
// tests/unit/test_session_regression.cpp. Every fixture is recorded under the same
// conditions: the CPU track of `game-stub --frames 125`, with the session started as
// soon as the stub connects and left running until the stub exits.

#include <chrono>
#include <filesystem>
#include <iostream>

#include "insights/reporter.h"
#include "insights/server.h"
#include "stub_session.h"

namespace {

using namespace std::chrono_literals;

constexpr int STUB_FRAMES = 125;

int Record(const std::filesystem::path& path) {
    insights::test::StubSession session(STUB_FRAMES);
    auto error = session.Start();
    if (!error.empty()) {
        std::cerr << error << "\n";
        return 1;
    }

    if (!session.StartSessionOnConnect(10s)) {
        std::cerr << "game-stub did not connect\n";
        return 1;
    }
    long exit_code = session.WaitForStubExit(30s);
    if (exit_code != 0) {
        std::cerr << "game-stub did not exit cleanly (" << exit_code << ")\n";
        return 1;
    }
    if (!session.WaitForDisconnect(5s)) {
        std::cerr << "server did not see game-stub disconnect\n";
        return 1;
    }

    // Fixtures hold the CPU track only.
    auto& reporter = insights::Reporter::GetInstance();
    auto  frames   = reporter.GetTrack(insights::TrackId::CPU_BASE);
    reporter.Clear();
    for (const auto& frame : frames) {
        reporter.Submit(*frame);
    }

    insights::Server::GetInstance().SaveSession(path);
    std::cout << "recorded " << frames.size() << " frames to " << path.string() << "\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: record-fixture <path>.trace\n";
        return 1;
    }

    // A fixture is recorded once and never regenerated.
    std::filesystem::path path(argv[1]);
    if (std::filesystem::exists(path)) {
        std::cerr << path.string() << " already exists; fixtures are never re-recorded\n";
        return 1;
    }

    try {
        return Record(path);
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
