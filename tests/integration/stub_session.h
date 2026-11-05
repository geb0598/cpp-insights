#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>
#include <thread>

#include "insights/pipe_transport.h"
#include "insights/server.h"

namespace insights::test {

// -------------------------------------------------
// WaitUntil
// -------------------------------------------------
// Every wait on the other process is bounded, so a peer that stops responding fails
// the caller instead of hanging it.
template <typename Predicate>
bool WaitUntil(Predicate predicate, std::chrono::milliseconds timeout) {
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}

// -------------------------------------------------
// StubSession
// -------------------------------------------------
// Runs `game-stub --frames N` as a child process against the in-process Server, on
// pipes named for this session alone (PIPE_SUFFIX_ENV), so it never meets a viewer or
// another test. Shared by the IPC end-to-end test and the fixture recorder, so both
// drive the stub the same way.
class StubSession {
public:
    explicit StubSession(int frame_count) : frame_count_(frame_count) {}

    ~StubSession() {
        if (process_) {
            TerminateProcess(process_, 1);
            WaitForSingleObject(process_, INFINITE);
            CloseHandle(process_);
        }
        if (job_) {
            CloseHandle(job_);
        }
        Server::GetInstance().Stop();
        SetEnvironmentVariableW(PIPE_SUFFIX_ENV, nullptr);
    }

    StubSession(const StubSession&)            = delete;
    StubSession& operator=(const StubSession&) = delete;
    StubSession(StubSession&&)                 = delete;
    StubSession& operator=(StubSession&&)      = delete;

    // Listens, then launches the stub. Returns an error message, or empty on success.
    std::string Start() {
        static std::atomic<int> counter = 0;
        std::wstring suffix = L"stub-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                              std::to_wstring(counter++);
        SetEnvironmentVariableW(PIPE_SUFFIX_ENV, suffix.c_str());

        auto& server = Server::GetInstance();
        server.Listen();
        if (!server.IsListening()) {
            return "Server::Listen failed";
        }

        // The job kills the stub if this process dies before the destructor runs.
        job_ = CreateJobObjectW(nullptr, nullptr);
        if (!job_) {
            return "CreateJobObject failed: " + std::to_string(GetLastError());
        }
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(job_, JobObjectExtendedLimitInformation, &limits, sizeof(limits));

        auto stub_path = std::filesystem::path(INSIGHTS_GAME_STUB_PATH).make_preferred();
        std::wstring command_line = L"\"" + stub_path.wstring() + L"\" --frames " +
                                    std::to_wstring(frame_count_);

        STARTUPINFOW        startup_info{};
        PROCESS_INFORMATION process_info{};
        startup_info.cb = sizeof(startup_info);
        if (!CreateProcessW(nullptr, command_line.data(), nullptr, nullptr, FALSE,
                            CREATE_SUSPENDED, nullptr, nullptr, &startup_info, &process_info)) {
            return "CreateProcess failed for " + stub_path.string() + ": " + std::to_string(GetLastError());
        }
        process_ = process_info.hProcess;
        AssignProcessToJobObject(job_, process_);
        ResumeThread(process_info.hThread);
        CloseHandle(process_info.hThread);
        return {};
    }

    // Starts the session as soon as the stub connects, as the viewer's record button does.
    bool StartSessionOnConnect(std::chrono::milliseconds timeout) {
        auto& server = Server::GetInstance();
        if (!WaitUntil([&]() { return server.IsConnected(); }, timeout)) {
            return false;
        }
        server.StartSession();
        return true;
    }

    bool IsStubRunning() const {
        return process_ && WaitForSingleObject(process_, 0) == WAIT_TIMEOUT;
    }

    // Waits for the stub to exit after its frames and returns its exit code, or -1 on timeout.
    long WaitForStubExit(std::chrono::milliseconds timeout) {
        if (!process_ ||
            WaitForSingleObject(process_, static_cast<DWORD>(timeout.count())) != WAIT_OBJECT_0) {
            return -1;
        }
        DWORD exit_code = 0;
        GetExitCodeProcess(process_, &exit_code);
        CloseHandle(process_);
        process_ = nullptr;
        return static_cast<long>(exit_code);
    }

    // The Server goes OFFLINE once its receive thread sees the stub's pipe close, so
    // every frame sent before that has been submitted to the Reporter.
    bool WaitForDisconnect(std::chrono::milliseconds timeout) {
        auto& server = Server::GetInstance();
        return WaitUntil([&]() { return server.GetState() == ServerState::OFFLINE; }, timeout);
    }

private:
    int    frame_count_;
    HANDLE process_ = nullptr;
    HANDLE job_     = nullptr;
};

} // namespace insights::test
