#include "insights/archive.h"
#include "insights/client.h"
#include "insights/scope_profiler.h"

namespace insights {

void Client::Connect() {
    if (connect_thread_.joinable()) {
        connect_thread_.join();
    }
    is_client_running_ = true;
    connect_thread_    = std::thread(&Client::ConnectWorker, this);
}

void Client::Disconnect() {
    is_client_running_ = false;
    data_pipe_.Disconnect();
    control_pipe_.Disconnect();
    StopWorkers();
    NotifyDisconnected();

    if (connect_thread_.joinable()) {
        connect_thread_.join();
    }
}

void Client::SendFrame(FrameRecord frame) {
    if (!IsRecording()) {
        return;
    }

    BinaryWriter writer;
    writer << frame;
    EnqueuePacket(PacketType::FRAME, std::move(writer).GetBuffer());
}

TransportResult Client::Send(PacketType type, const ByteBuffer& payload) {
    return data_pipe_.Send(type, payload);
}

TransportResult Client::Receive(PacketHeader& out_header, ByteBuffer& out_payload) {
    return control_pipe_.Receive(out_header, out_payload);
}

void Client::OnPacketReceived(const PacketHeader& header, const ByteBuffer& payload) {
    switch (header.type) {
    case PacketType::SESSION_START:
        ScopeProfiler::GetInstance().BeginRecording();
        SetState(ClientState::RECORDING);
        break;
    case PacketType::SESSION_STOP:
        ScopeProfiler::GetInstance().EndRecording();
        SetState(ClientState::CONNECTED);
        break;
    default:
        break;
    }
}

void Client::OnDisconnected() {
    SetState(ClientState::DISCONNECTED);
    data_pipe_.Disconnect();
    control_pipe_.Disconnect();
    NotifyDisconnected();
}

void Client::ConnectWorker() {
    auto data_pipe_name    = GetPipeName(DATA_PIPE_NAME);
    auto control_pipe_name = GetPipeName(CONTROL_PIPE_NAME);

    while (IsClientRunning()) {
        if (!IsDisconnected()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        StopWorkers();

        auto data_result = data_pipe_.Connect(data_pipe_name.c_str(), GENERIC_WRITE);
        if (!data_result) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        auto control_result = control_pipe_.Connect(control_pipe_name.c_str(), GENERIC_READ);
        if (!control_result) {
            data_pipe_.Disconnect();
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        auto handshake_result = SendHandshake();
        if (!handshake_result) {
            data_pipe_.Disconnect();
            control_pipe_.Disconnect();
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        SetState(ClientState::CONNECTED);
        NotifyConnected();
        StartWorkers();
    }
}

TransportResult Client::SendHandshake() {
    Descriptor::GetFrameDescriptor();

    BinaryWriter writer;

    auto& groups = Registry::GetInstance().GetGroups();
    auto& descs  = Registry::GetInstance().GetDescriptors();

    int32_t group_count = static_cast<int32_t>(groups.size());
    writer << group_count;
    for (auto& [id, group] : groups) {
        writer << *group;
    }

    int32_t desc_count = static_cast<int32_t>(descs.size());
    writer << desc_count;
    for (auto& [id, desc] : descs) {
        writer << *desc;
    }

    return Send(PacketType::HANDSHAKE, std::move(writer).GetBuffer());
}

} // namespace insights