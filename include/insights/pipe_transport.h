#pragma once

#include <mutex>
#include <string>

#include "insights/transport.h"

namespace insights {

#if defined(_WIN32)
constexpr wchar_t DATA_PIPE_NAME[]    = L"\\\\.\\pipe\\cpp-insights-data";
constexpr wchar_t CONTROL_PIPE_NAME[] = L"\\\\.\\pipe\\cpp-insights-control";

// If this variable is set and not empty, Client and Server append "-<value>" to both
// pipe names, so a test can run its own pair beside a viewer. Child processes inherit it.
constexpr wchar_t PIPE_SUFFIX_ENV[]   = L"INSIGHTS_PIPE_SUFFIX";

std::wstring GetPipeName(const wchar_t* base_name);
#endif

// -------------------------------------------------
// UniquePipeHandle
// -------------------------------------------------
class UniquePipeHandle {
public:
#if defined(_WIN32)
    using HandleType = HANDLE;
    static constexpr HandleType INVALID_HANDLE = INVALID_HANDLE_VALUE;
#else
    using HandleType = int;
    static constexpr HandleType INVALID_HANDLE = -1;
#endif

    UniquePipeHandle() = default;

    explicit UniquePipeHandle(HandleType handle)
        : handle_(handle) {}

    ~UniquePipeHandle() { Reset(); }

    UniquePipeHandle(const UniquePipeHandle&)            = delete;
    UniquePipeHandle& operator=(const UniquePipeHandle&) = delete;

    UniquePipeHandle(UniquePipeHandle&& other) noexcept
        : handle_(other.Release()) {}

    UniquePipeHandle& operator=(UniquePipeHandle&& other) noexcept {
        if (this != &other) {
            Reset();
            handle_ = other.Release();
        }
        return *this;
    }

    HandleType Get()     const { return handle_; }
    bool       IsValid() const { return handle_ != INVALID_HANDLE; }

    explicit operator bool() const { return IsValid(); }

    HandleType Release() {
        HandleType h = handle_;
        handle_ = INVALID_HANDLE;
        return h;
    }

    void Reset(HandleType handle = INVALID_HANDLE);

    TransportResult Send(PacketType type, const ByteBuffer& payload);
    TransportResult Receive(PacketHeader& out_header, ByteBuffer& out_payload);

private:
    TransportResult ReadExact(void* buffer, DWORD size); 

    HandleType handle_ = INVALID_HANDLE;
};


// -------------------------------------------------
// PipeClient 
// -------------------------------------------------
class PipeClient : public IClientTransport {
public:
    PipeClient()           = default;
    ~PipeClient() override = default;

    TransportResult Connect(const wchar_t* pipe_name, DWORD access)       override;
    void            Disconnect()                                          override { handle_.Reset(); }
    bool            IsConnected()                                   const override { return handle_.IsValid(); }

    TransportResult Send(PacketType type, const ByteBuffer& payload)    override;
    TransportResult Receive(PacketHeader& out_type, ByteBuffer& out_payload) override;

private:
    UniquePipeHandle handle_;
    std::mutex       mutex_;
};

// -------------------------------------------------
// PipeServer 
// -------------------------------------------------
class PipeServer : public IServerTransport {
public:
    PipeServer()           = default;
    ~PipeServer() override { CleanUp(); }

    TransportResult Listen(const wchar_t* pipe_name, DWORD access)       override;
    TransportResult Accept()                                             override;
    void            Disconnect()                                         override { CleanUp(); }
    bool            IsConnected()                                  const override { return handle_.IsValid(); }

    TransportResult Send(PacketType type, const ByteBuffer& payload)    override;
    TransportResult Receive(PacketHeader& out_header, ByteBuffer& out_payload) override;

private:
    void CleanUp();

    UniquePipeHandle handle_;
    std::mutex       mutex_;
};


} // namespace insights