#pragma once
// ============================================================================
// ExampleBuffs.h — v6 SDK
// ============================================================================
// Demonstrates reading the player's Buffs component and rendering a list with
// progress bars and filtering. Uses ctx->Components.EnumerateAggregatedBuffs()
// so multi-instance charge stacks (e.g. Chayula breach charges, stored as N
// separate StatusEffect instances) show their summed count in the Charges
// column — matching the in-game charge-stack icon. Swap in EnumerateBuffs()
// for the raw per-instance list instead.
// ============================================================================

#include "sdk/PluginSDK.h"
#include <imgui.h>

namespace Examples {

inline void DrawBuffsPanel(const PluginSDK::Context* ctx,
                           const PluginSDK::Snapshot& snapshot) {
    if (!ctx) return;

    if (!snapshot.Player.Components.HasBuffs()) {
        ImGui::TextDisabled("Player has no Buffs component");
        return;
    }

    std::vector<PluginSDK::Buff> buffs =
        ctx->Components.EnumerateAggregatedBuffs(snapshot.Player.Components.Buffs);

    ImGui::Text("Active Buffs: %d", static_cast<int>(buffs.size()));

    if (buffs.empty()) {
        ImGui::TextDisabled("No active buffs");
        return;
    }

    static char buffFilter[64] = "";
    ImGui::InputText("Filter##buffFilter", buffFilter, sizeof(buffFilter));

    if (ImGui::BeginTable("BuffTable", 6,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_Sortable | ImGuiTableFlags_Resizable,
        ImVec2(0, 250))) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Time Left", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Total Time", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Charges", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("Source Entity Id", ImGuiTableColumnFlags_WidthFixed, 110);
        ImGui::TableSetupColumn("Progress", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableHeadersRow();

        for (const auto& b : buffs) {
            if (buffFilter[0] != '\0') {
                if (b.Name.find(buffFilter) == std::string::npos) continue;
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            if (b.TotalTime <= 0) {
                ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%s", b.Name.c_str());
            } else {
                ImGui::TextUnformatted(b.Name.c_str());
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", b.Name.c_str());
                if (ImGui::IsItemClicked()) ImGui::SetClipboardText(b.Name.c_str());
            }

            ImGui::TableNextColumn();
            if (b.TimeLeft > 0)
                ImGui::Text("%.1fs", b.TimeLeft);
            else
                ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "Permanent");

            ImGui::TableNextColumn();
            if (b.TotalTime > 0)
                ImGui::Text("%.1fs", b.TotalTime);
            else
                ImGui::TextDisabled("--");

            ImGui::TableNextColumn();
            if (b.Charges > 0)
                ImGui::Text("%d", b.Charges);
            else
                ImGui::TextDisabled("0");

            ImGui::TableNextColumn();
            if (b.SourceEntityId != 0)
                ImGui::Text("%u", b.SourceEntityId);
            else
                ImGui::TextDisabled("--");

            ImGui::TableNextColumn();
            if (b.TotalTime > 0 && b.TimeLeft > 0) {
                float frac = b.TimeLeft / b.TotalTime;
                ImVec4 barColor = (frac > 0.5f) ? ImVec4(0.3f, 0.8f, 0.3f, 1.0f)
                    : (frac > 0.2f) ? ImVec4(0.8f, 0.8f, 0.3f, 1.0f)
                    : ImVec4(0.8f, 0.3f, 0.3f, 1.0f);
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barColor);
                ImGui::ProgressBar(frac, ImVec2(-1, 14), "");
                ImGui::PopStyleColor();
            } else {
                ImGui::TextDisabled("--");
            }
        }
        ImGui::EndTable();
    }
}

} // namespace Examples
