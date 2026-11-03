#define NOMINMAX
#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <implot.h>

#include "insights/reporter.h"
#include "insights/scope_profiler.h"

#include "timeline_panel.h"

namespace insights::viewer {

namespace {
    ImVec4 GetColorFromId(Descriptor::Id id) {
        float hue = (id * 0.618033988749895f);
        hue -= std::floor(hue);
        float r, g, b;
        ImGui::ColorConvertHSVtoRGB(hue, 0.6f, 0.8f, r, g, b);
        return ImVec4(r, g, b, 1.0f);
    }
}

void TimelinePanel::Render() {
    auto& context  = GetContext();
    auto& reporter = Reporter::GetInstance();
    auto& registry = Registry::GetInstance();

    bool is_recording = (context.server_state == ServerState::RECORDING);
    if (was_recording_ && !is_recording)
        reset_view_ = true;
    was_recording_ = is_recording;

    auto summary = reporter.GetTimelineSummary();
    int count = static_cast<int>(summary.total_frame_ms.size());

    float panel_height = ImGui::GetWindowHeight() * 0.3f;
    ImGui::BeginChild("Timeline", ImVec2(0, panel_height), true);
    
    ImGui::Text("Selected Frame Range: [%zu ~ %zu]", context.timeline_begin, context.timeline_end);

    if (count > 0) {
        if (ImPlot::BeginPlot("##TimelinePlot", ImVec2(-1, -1), ImPlotFlags_NoLegend | ImPlotFlags_NoMenus)) {
            float max_time = 1.0f;
            if (count > 0) {
                max_time = *std::max_element(summary.total_frame_ms.begin(), summary.total_frame_ms.end());
            }

            ImPlotAxisFlags x_flags = ImPlotAxisFlags_NoLabel;
            if (is_recording) x_flags |= ImPlotAxisFlags_Lock;

            ImPlot::SetupAxes(nullptr, "Time (ms)", x_flags, ImPlotAxisFlags_None);
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, max_time * 1.2, ImPlotCond_Always);

            if (is_recording) {
                ImPlot::SetupAxisLimits(ImAxis_X1, -0.5, count - 0.5, ImPlotCond_Always);
            } else if (reset_view_) {
                ImPlot::SetupAxisLimits(ImAxis_X1, -0.5, count - 0.5, ImPlotCond_Always);
                reset_view_ = false;
            }

            constexpr float BAR_MAX_PX = 12.0f;
            ImVec2 plot_size = ImPlot::GetPlotSize();
            double bar_size  = (count > 0 && plot_size.x > 0)
                ? std::min(0.85, static_cast<double>(BAR_MAX_PX) / (plot_size.x / count))
                : 0.85;

            ImPlot::SetNextFillStyle(ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            ImPlot::PlotBars("Unaccounted (Overhead)", summary.total_frame_ms.data(), count, bar_size);

            std::vector<std::pair<Descriptor::Id, const std::vector<float>*>> track_list;
            for (const auto& kv : summary.tracks) {
                track_list.push_back({kv.first, &kv.second});
            }

            std::vector<std::vector<float>> cum_arrays(track_list.size(), std::vector<float>(count, 0.0f));
            for (int i = 0; i < count; ++i) {
                float current_sum = 0.0f;
                for (size_t t = 0; t < track_list.size(); ++t) {
                    current_sum += (*track_list[t].second)[i];
                    cum_arrays[t][i] = current_sum;
                }
            }

            for (int t = static_cast<int>(track_list.size()) - 1; t >= 0; --t) {
                auto id = track_list[t].first;
                auto* d = registry.FindDescriptor(id);
                const char* name = d ? d->GetName().c_str() : "Unknown";

                ImPlot::SetNextFillStyle(GetColorFromId(id));
                ImPlot::PlotBars(name, cum_arrays[t].data(), count, bar_size);
            }

            double b = static_cast<double>(context.timeline_begin);
            double e = static_cast<double>(context.timeline_end);
            
            if (b == 0 && e == 0 && count > 0) {
                e = count - 1; 
            }

            struct FpsRef { double ms; const char* label; ImU32 color; };
            static const FpsRef fps_refs[] = {
                {  8.333, "120fps", IM_COL32(100, 220, 100, 200) },
                { 16.667,  "60fps", IM_COL32(220, 220,  80, 200) },
                { 33.333,  "30fps", IM_COL32(220,  80,  80, 200) },
            };
            ImPlot::PushPlotClipRect();
            ImDrawList* ref_dl = ImPlot::GetPlotDrawList();
            for (const auto& ref : fps_refs) {
                if (ref.ms > max_time * 1.2) continue;
                ImVec2 p1 = ImPlot::PlotToPixels(-0.5,          ref.ms);
                ImVec2 p2 = ImPlot::PlotToPixels(count - 0.5,   ref.ms);
                ref_dl->AddLine(p1, p2, ref.color, 1.5f);
                ref_dl->AddText(ImVec2(p1.x + 4.0f, p1.y - 14.0f), ref.color, ref.label);
            }
            ImPlot::PopPlotClipRect();

            ImPlot::DragLineX(0, &b, ImVec4(1.0f, 1.0f, 1.0f, 0.8f), 1.5f);
            ImPlot::DragLineX(1, &e, ImVec4(1.0f, 1.0f, 1.0f, 0.8f), 1.5f);

            if (b < 0) {
                b = 0;
            }

            if (e > count - 1) {
                e = count - 1;
            }

            if (b > e) {
                std::swap(b, e);
            }

            double annot_y = max_time * 1.15;
            ImPlot::Annotation(b, annot_y, ImVec4(0.2f, 0.6f, 1.0f, 1.0f), ImVec2(0, 0), true, "Start: %d", static_cast<int>(std::round(b)));
            ImPlot::Annotation(e, annot_y, ImVec4(1.0f, 0.4f, 0.4f, 1.0f), ImVec2(0, 0), true, "End: %d", static_cast<int>(std::round(e)));

            context.timeline_begin = static_cast<size_t>(std::round(b));
            context.timeline_end   = static_cast<size_t>(std::round(e));

            if (ImPlot::IsPlotHovered()) {
                ImPlotPoint mouse = ImPlot::GetPlotMousePos();
                int frame_i = static_cast<int>(std::round(mouse.x));

                if (frame_i >= 0 && frame_i < count) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Frame #%d", frame_i);
                    ImGui::Separator();
                    
                    ImGui::Text("Total: %.3f ms", summary.total_frame_ms[frame_i]);
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Unaccounted: %.3f ms", summary.unaccounted_ms[frame_i]);
                    
                    ImGui::Separator();
                    for (const auto& [id, track] : summary.tracks) {
                        float ms = track[frame_i];
                        if (ms > 0.001f) {
                            auto* d = registry.FindDescriptor(id);
                            ImVec4 color = GetColorFromId(id);
                            ImGui::TextColored(color, "%s: %.3f ms", d ? d->GetName().c_str() : "Unknown", ms);
                        }
                    }
                    ImGui::EndTooltip();
                }
            }
            ImPlot::EndPlot();
        }
    } else {
        ImGui::Text("Waiting for profile data...");
    }

    ImGui::EndChild();
}

void TimelinePanel::Reset() {
    reset_view_ = true;
}

} // namespace insightss::viewer