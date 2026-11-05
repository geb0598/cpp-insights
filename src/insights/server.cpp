#include <chrono>
#include <ctime>
#include <filesystem>

#include "insights/archive.h"
#include "insights/reporter.h"
#include "insights/server.h"
#include "insights/windows_archive.h"

namespace insights {
    
std::filesystem::path Server::GenerateSessionFilename() {
    auto      now    = std::chrono::system_clock::now();
    auto      time_t = std::chrono::system_clock::to_time_t(now);
    struct tm tm_info {};
    localtime_s(&tm_info, &time_t);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d_%H-%M-%S", &tm_info);
    return std::filesystem::path(std::string(buf) + ".trace");
}

void Server::Listen() {
    StopWorkers();

    auto data_result = data_pipe_.Listen(GetPipeName(DATA_PIPE_NAME).c_str(), PIPE_ACCESS_INBOUND);
    if (!data_result) {
        return;
    }

    auto control_result = control_pipe_.Listen(GetPipeName(CONTROL_PIPE_NAME).c_str(), PIPE_ACCESS_OUTBOUND);
    if (!control_result) {
        data_pipe_.Disconnect();
        return;
    }

    SetState(ServerState::LISTENING);

    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }
    accept_thread_ = std::thread(&Server::AcceptWorker, this);
}

void Server::Stop() {
    SetState(ServerState::OFFLINE);
    data_pipe_.Disconnect();
    control_pipe_.Disconnect();
    StopWorkers();
    NotifyDisconnected();

    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }
}

void Server::StartSession() {
    if (!IsConnected()) {
        return;
    }
    Reporter::GetInstance().Clear();
    state_ = ServerState::RECORDING;
    EnqueuePacket(PacketType::SESSION_START, {});
}

void Server::StopSession() {
    if (!IsRecording()) {
        return;
    }
    SetState(ServerState::CONNECTED);
    EnqueuePacket(PacketType::SESSION_STOP, {});
}

TransportResult Server::Send(PacketType type, const ByteBuffer& payload) {
    return control_pipe_.Send(type, payload);
}

TransportResult Server::Receive(PacketHeader& out_header, ByteBuffer& out_payload) {
    return data_pipe_.Receive(out_header, out_payload);
}

void Server::OnPacketReceived(const PacketHeader& header, const ByteBuffer& payload) {
    switch (header.type) {
    case PacketType::HANDSHAKE:
        OnHandshake(payload);
        break;
    case PacketType::FRAME:
        OnFrame(payload);
        break;
    default:
        break;
    }
}

void Server::OnDisconnected() {
    SetState(ServerState::OFFLINE);
    data_pipe_.Disconnect();
    control_pipe_.Disconnect();
    NotifyDisconnected();
}

void Server::AcceptWorker() {
    std::thread data_accept_thread([this]() {
        auto result = data_pipe_.Accept();
        if (!result) {
            data_pipe_.Disconnect();
        }
    });

    std::thread control_accept_thread([this]() {
        auto result = control_pipe_.Accept();
        if (!result) {
            control_pipe_.Disconnect();
        }
    });

    data_accept_thread.join();
    control_accept_thread.join();

    if (!data_pipe_.IsConnected() || !control_pipe_.IsConnected()) {
        state_ = ServerState::OFFLINE;
        data_pipe_.Disconnect();
        control_pipe_.Disconnect();
        return;
    }

    state_ = ServerState::CONNECTED;
    NotifyConnected();
    StartWorkers();
}

namespace {
    constexpr uint32_t FILE_MAGIC   = 0x494E5347u; // 'INSG'
    constexpr uint16_t FILE_VERSION = 1;           // bump on any .trace layout change
} // namespace

void Server::SaveSession(const std::filesystem::path& path) {
    std::filesystem::path tmp_path = path;
    tmp_path += ".tmp";

    {
        WindowsBinaryWriter ar(tmp_path);

        uint32_t magic    = FILE_MAGIC;
        uint16_t version  = FILE_VERSION;
        uint16_t reserved = 0;
        ar << magic << version << reserved;

        auto& registry = Registry::GetInstance();
        auto& groups   = registry.GetGroups();
        auto& descs    = registry.GetDescriptors();

        int32_t group_count = static_cast<int32_t>(groups.size());
        ar << group_count;
        for (auto& [id, group] : groups) {
            ar << *group;
        }

        int32_t desc_count = static_cast<int32_t>(descs.size());
        ar << desc_count;
        for (auto& [id, desc] : descs) {
            ar << *desc;
        }

        auto& reporter  = Reporter::GetInstance();
        auto  track_ids = reporter.GetTrackIds();

        uint32_t track_count = static_cast<uint32_t>(track_ids.size());
        ar << track_count;

        for (uint32_t track_id : track_ids) {
            auto frames = reporter.GetTrack(track_id);
            ar << track_id;

            uint32_t frame_count = static_cast<uint32_t>(frames.size());
            ar << frame_count;

            for (const auto& shared_frame : frames) {
                auto frame = *shared_frame;
                ar << frame;
            }
        }

        ar.Close();
    }

    std::filesystem::remove(path);
    std::error_code ec;
    std::filesystem::rename(tmp_path, path, ec);
    if (ec) {
        std::filesystem::remove(tmp_path);
        throw std::system_error(ec, "insights::Server::SaveSession: rename failed");
    }
}

void Server::LoadSession(const std::filesystem::path& path) {
    WindowsBinaryReader ar(path);

    uint32_t magic    = 0;
    uint16_t version  = 0;
    uint16_t reserved = 0;
    ar << magic << version << reserved;

    if (magic != FILE_MAGIC) {
        throw std::system_error(std::make_error_code(std::errc::invalid_argument),
                                "insights::Server::LoadSession: invalid file magic");
    }
    if (version != FILE_VERSION) {
        throw std::system_error(std::make_error_code(std::errc::not_supported),
                                "insights::Server::LoadSession: unsupported version");
    }

    Registry::GetInstance().Clear();
    Reporter::GetInstance().Clear();
    owned_groups_.clear();
    owned_descs_.clear();

    int32_t group_count = 0;
    ar << group_count;
    for (int32_t i = 0; i < group_count; ++i) {
        auto group = std::make_unique<Group>();
        ar << *group;
        Registry::GetInstance().RegisterGroup(group.get());
        owned_groups_.push_back(std::move(group));
    }

    int32_t desc_count = 0;
    ar << desc_count;
    for (int32_t i = 0; i < desc_count; ++i) {
        auto desc = std::make_unique<Descriptor>();
        ar << *desc;
        Registry::GetInstance().RegisterDescriptor(desc.get());
        owned_descs_.push_back(std::move(desc));
    }

    uint32_t track_count = 0;
    ar << track_count;

    for (uint32_t t = 0; t < track_count; ++t) {
        uint32_t track_id    = 0;
        uint32_t frame_count = 0;
        ar << track_id << frame_count;

        for (uint32_t f = 0; f < frame_count; ++f) {
            FrameRecord frame;
            ar << frame;
            Reporter::GetInstance().Submit(std::move(frame));
        }
    }
}

void Server::OnHandshake(const ByteBuffer& data) {
    BinaryReader reader(data);

    Registry::GetInstance().Clear();
    owned_groups_.clear();
    owned_descs_.clear();

    int32_t group_count;
    reader << group_count;
    for (int32_t i = 0; i < group_count; ++i) {
        auto group = std::make_unique<Group>();
        reader << *group;
        Registry::GetInstance().RegisterGroup(group.get());
        owned_groups_.push_back(std::move(group));
    }

    int32_t desc_count;
    reader << desc_count;
    for (int32_t i = 0; i < desc_count; ++i) {
        auto desc = std::make_unique<Descriptor>();
        reader << *desc;
        Registry::GetInstance().RegisterDescriptor(desc.get());
        owned_descs_.push_back(std::move(desc));
    }
}

void Server::OnFrame(const ByteBuffer& data) {
    if (!IsRecording()) {
        return;
    }

    BinaryReader reader(data);
    FrameRecord  frame;
    reader << frame;

    Reporter::GetInstance().Submit(std::move(frame));
}

} // namespace insights