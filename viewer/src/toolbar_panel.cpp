#include <algorithm>
#include <array>
#include <windows.h>
#include <commdlg.h>

#include <imgui.h>

#include "insights/reporter.h"

#include "toolbar_panel.h"
#include "icons.h"

namespace insights::viewer {

void ToolbarPanel::Render() {
    auto& server  = Server::GetInstance();
    auto& context = GetContext();

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            bool can_save = context.server_state != ServerState::RECORDING
                         && Reporter::GetInstance().Size() > 0;
            bool can_open = context.server_state == ServerState::OFFLINE;

            if (ImGui::MenuItem("Save Session", nullptr, false, can_save)) {
                auto default_name = Server::GenerateSessionFilename().string();

                std::array<char, MAX_PATH> path_buf{};
                std::copy(default_name.begin(), default_name.end(), path_buf.begin());

                OPENFILENAMEA ofn{};
                ofn.lStructSize = sizeof(ofn);
                ofn.lpstrFilter = "Trace Files (*.trace)\0*.trace\0All Files (*.*)\0*.*\0";
                ofn.lpstrFile   = path_buf.data();
                ofn.nMaxFile    = static_cast<DWORD>(path_buf.size());
                ofn.lpstrDefExt = "trace";
                ofn.Flags       = OFN_OVERWRITEPROMPT;

                if (GetSaveFileNameA(&ofn)) {
                    try {
                        server.SaveSession(path_buf.data());
                        status_message_ = "Saved: " + std::string(path_buf.data());
                    } catch (const std::exception& e) {
                        status_message_ = std::string("Save failed: ") + e.what();
                    }
                }
            }

            if (ImGui::MenuItem("Open Session", nullptr, false, can_open)) {
                std::array<char, MAX_PATH> path_buf{};

                OPENFILENAMEA ofn{};
                ofn.lStructSize = sizeof(ofn);
                ofn.lpstrFilter = "Trace Files (*.trace)\0*.trace\0All Files (*.*)\0*.*\0";
                ofn.lpstrFile   = path_buf.data();
                ofn.nMaxFile    = static_cast<DWORD>(path_buf.size());
                ofn.lpstrDefExt = "trace";
                ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

                if (GetOpenFileNameA(&ofn)) {
                    try {
                        server.Stop();
                        server.LoadSession(path_buf.data());
                        context.server_state   = ServerState::OFFLINE;
                        context.needs_reset    = true;
                        context.timeline_begin = 0;
                        context.timeline_end   = Reporter::GetInstance().Size();
                        status_message_ = "Loaded: " + std::string(path_buf.data());
                    } catch (const std::exception& e) {
                        status_message_ = std::string("Load failed: ") + e.what();
                    }
                }
            }

            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    switch (context.server_state) {

        case ServerState::OFFLINE:
            if (ImGui::Button(ICON_FA_PLAY "  Connect")) {
                server.Listen();
                context.server_state = ServerState::LISTENING;
            }
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Offline");
            break;

        case ServerState::LISTENING:
            if (ImGui::Button(ICON_FA_STOP "  Stop")) {
                server.Stop();
                context.server_state = ServerState::OFFLINE;
            }
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Waiting for client...");
            break;

        case ServerState::CONNECTED:
            if (ImGui::Button(ICON_FA_POWER "  Disconnect")) {
                server.Stop();
                context.server_state = ServerState::OFFLINE;
            }
            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.5f, 0.0f, 0.0f, 1.0f));
            if (ImGui::Button(ICON_FA_CIRCLE "  Record")) {
                server.StartSession();
                context.server_state   = ServerState::RECORDING;
                context.needs_reset    = true;
                context.timeline_begin = 0;
                context.timeline_end   = 0;
                record_start_          = std::chrono::steady_clock::now();
            }
            ImGui::PopStyleColor(3);

            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Connected");
            break;

        case ServerState::RECORDING: {
            if (ImGui::Button(ICON_FA_POWER "  Disconnect")) {
                server.Stop();
                context.server_state = ServerState::OFFLINE;
            }
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_STOP "  Stop")) {
                server.StopSession();
                context.server_state = ServerState::CONNECTED;
                context.timeline_end = Reporter::GetInstance().Size();
            }
            ImGui::SameLine();

            auto elapsed = std::chrono::steady_clock::now() - record_start_;
            auto secs    = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
            ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f),
                ICON_FA_CIRCLE "  Recording   %02lld:%02lld", secs / 60, secs % 60);
            break;
        }
    }

    ImGui::Separator();

    if (!status_message_.empty()) {
        ImGui::TextDisabled("%s", status_message_.c_str());
    }
}

} // namespace insights::viewer
