// src/pipe_transport.cpp
#include <cstring>
#include <vector>

#include "insights/pipe_transport.h"

namespace insights {

#if defined(_WIN32)

std::wstring GetPipeName(const wchar_t* base_name) {
    std::wstring name(base_name);

    DWORD size = GetEnvironmentVariableW(PIPE_SUFFIX_ENV, nullptr, 0);
    if (size <= 1) {
        return name;
    }

    std::wstring suffix(size, L'\0');
    size = GetEnvironmentVariableW(PIPE_SUFFIX_ENV, suffix.data(), size);
    suffix.resize(size);
    return name + L"-" + suffix;
}

void UniquePipeHandle::Reset(HandleType handle) {
    if (handle_ != INVALID_HANDLE) {
        CancelIoEx(handle_, nullptr);
        CloseHandle(handle_);
    }
    handle_ = handle;
}

TransportResult UniquePipeHandle::ReadExact(void* buffer, DWORD size) {
    DWORD total = 0;
    while (total < size) {
        DWORD bytes_read = 0;
        if (!ReadFile(handle_,
                      static_cast<char*>(buffer) + total,
                      size - total,
                      &bytes_read, 
                      nullptr)) {
            return TransportResult::GetSystemError();
        }
        if (bytes_read == 0) {
            return TransportResult::Err(std::error_code(ERROR_BROKEN_PIPE, std::system_category()));
        }
        total += bytes_read;
    }
    return TransportResult::Ok();
}

TransportResult UniquePipeHandle::Send(PacketType type, const ByteBuffer& payload) {
    PacketHeader header;
    header.type         = type;
    header.payload_size = static_cast<uint32_t>(payload.size());

    ByteBuffer buffer;
    buffer.resize(sizeof(PacketHeader) + payload.size());
    std::memcpy(buffer.data(), &header, sizeof(PacketHeader));
    if (!payload.empty()) {
        std::memcpy(buffer.data() + sizeof(PacketHeader), payload.data(), payload.size());
    }

    DWORD bytes_written = 0;
    if (!WriteFile(handle_, buffer.data(), static_cast<DWORD>(buffer.size()), &bytes_written, nullptr)) {
        return TransportResult::GetSystemError();
    }
    return TransportResult::Ok();
}

TransportResult UniquePipeHandle::Receive(PacketHeader& out_header, ByteBuffer& out_payload) {
    auto result = ReadExact(&out_header, sizeof(PacketHeader));
    if (!result) {
        return result;
    }

    out_payload.clear();
    if (out_header.payload_size == 0) {
        return TransportResult::Ok();
    }

    out_payload.resize(out_header.payload_size);
    return ReadExact(out_payload.data(), static_cast<DWORD>(out_header.payload_size));
}

TransportResult PipeClient::Send(PacketType type, const ByteBuffer& payload) {
    if (!IsConnected()) {
        return TransportResult::NotConnected();
    }

    auto result = handle_.Send(type, payload);
    if (!result && result.IsDisconnected()) {
        Disconnect();
    }

    return result;
}

TransportResult PipeClient::Receive(PacketHeader& out_header, ByteBuffer& out_payload) {
    if (!IsConnected()) {
        return TransportResult::NotConnected();
    }

    auto result = handle_.Receive(out_header, out_payload);
    if (!result && result.IsDisconnected()) {
        Disconnect();
    }

    return result;
}

TransportResult PipeClient::Connect(const wchar_t* pipe_name, DWORD access) {
    handle_.Reset(CreateFileW(
        pipe_name,
        access,
        0, nullptr, OPEN_EXISTING, 0, nullptr
    ));

    if (!IsConnected()) {
        return TransportResult::GetSystemError();
    }

    return TransportResult::Ok();
}

TransportResult PipeServer::Send(PacketType type, const ByteBuffer& payload) {
    if (!IsConnected()) {
        return TransportResult::NotConnected();
    }

    auto result = handle_.Send(type, payload);
    if (!result && result.IsDisconnected()) {
        Disconnect();
    }

    return result;
}

TransportResult PipeServer::Receive(PacketHeader& out_header, ByteBuffer& out_payload) {
    if (!IsConnected()) {
        return TransportResult::NotConnected();
    }

    auto result = handle_.Receive(out_header, out_payload);
    if (!result && result.IsDisconnected()) {
        Disconnect();
    }

    return result;
}

TransportResult PipeServer::Listen(const wchar_t* pipe_name, DWORD access) {
    std::lock_guard<std::mutex> lock(mutex_);

    handle_.Reset(CreateNamedPipeW(
        pipe_name,
        access,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1, 4096, 4096, 0, nullptr
    ));

    if (!IsConnected()) {
        return TransportResult::GetSystemError();
    }

    return TransportResult::Ok();
}

TransportResult PipeServer::Accept() {
    UniquePipeHandle::HandleType handle = UniquePipeHandle::INVALID_HANDLE;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handle = handle_.Get();
    }

    if (handle == UniquePipeHandle::INVALID_HANDLE) {
        return TransportResult::NotConnected();
    }

    if (!ConnectNamedPipe(handle_.Get(), nullptr)) {
        DWORD err = GetLastError();
        if (err != ERROR_PIPE_CONNECTED) {
            CleanUp();
            return TransportResult::GetSystemError();
        }
    }

    return TransportResult::Ok();
}

void PipeServer::CleanUp() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (handle_.IsValid()) {
        CancelIoEx(handle_.Get(), nullptr);
        DisconnectNamedPipe(handle_.Get());
        handle_.Reset();
    }
}

#else

// @todo Linux

#endif // _WIN32

} // namespace insights