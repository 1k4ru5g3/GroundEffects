#pragma once
// ============================================================================
// ExampleInventory.h — v6 SDK
// ============================================================================
// Demonstrates the v6 Inventory service: scan, enumerate, read item mods.
// Replaces the v5 "WatchInventory/GetWatchedInventoryData" demo with the
// simpler v6 model where InventoryService::GetAll() / Get(id) returns
// full inventory snapshots on demand.
// ============================================================================

#include "sdk/PluginSDK.h"
#include <imgui.h>
#include <chrono>
#include <cmath>
#include <algorithm>

namespace Examples {

inline void DrawInventoryPanel(
    const PluginSDK::Context* ctx,
    std::chrono::steady_clock::time_point& lastScan) {

    if (!ctx) return;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastScan).count();
    if (elapsed > 2000) {
        ctx->Inventory.Scan(-1);
        lastScan = now;
    }

    std::vector<PluginSDK::Inventory> inventories = ctx->Inventory.GetAll();

    ImGui::Text("Inventories: %d", static_cast<int>(inventories.size()));

    if (inventories.empty()) {
        ImGui::TextDisabled("No inventory data (scanning...)");
        return;
    }

    static int selectedIdx = 0;
    int invCount = static_cast<int>(inventories.size());
    if (selectedIdx >= invCount) selectedIdx = 0;

    char previewLabel[64];
    {
        const auto& inv = inventories[selectedIdx];
        const char* name = ctx->Inventory.GetName(inv.InventoryId);
        snprintf(previewLabel, sizeof(previewLabel), "[%d] %s",
                 inv.InventoryId, name ? name : "");
    }

    if (ImGui::BeginCombo("Inspect Inventory", previewLabel)) {
        for (int ci = 0; ci < invCount; ci++) {
            const auto& inv = inventories[ci];
            const char* name = ctx->Inventory.GetName(inv.InventoryId);
            char itemLabel[64];
            snprintf(itemLabel, sizeof(itemLabel), "[%d] %s",
                     inv.InventoryId, name ? name : "");
            bool isSelected = (ci == selectedIdx);
            if (ImGui::Selectable(itemLabel, isSelected))
                selectedIdx = ci;
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    const auto& inv = inventories[selectedIdx];

    if (ImGui::BeginTable("InvDetail", 2,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("Address");
        ImGui::TableNextColumn();
        char ab[20]; snprintf(ab, sizeof(ab), "0x%llX",
                              static_cast<unsigned long long>(inv.Address));
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.6f, 1.0f), "%s", ab);
        if (ImGui::IsItemHovered() && ImGui::IsItemClicked()) ImGui::SetClipboardText(ab);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("Total Boxes X");
        ImGui::TableNextColumn(); ImGui::Text("%d", inv.TotalBoxesX);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("Total Boxes Y");
        ImGui::TableNextColumn(); ImGui::Text("%d", inv.TotalBoxesY);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("ServerRequestCounter");
        ImGui::TableNextColumn(); ImGui::Text("%d", inv.ServerRequestCounter);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("Items");
        ImGui::TableNextColumn(); ImGui::Text("%d", static_cast<int>(inv.Items.size()));

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("Grid Valid");
        ImGui::TableNextColumn();
        ImGui::TextColored(inv.Grid.Valid ? ImVec4(0.3f,1,0.3f,1) : ImVec4(1,0.3f,0.3f,1),
                           inv.Grid.Valid ? "true" : "false");

        if (inv.Grid.Valid) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Grid Screen X/Y");
            ImGui::TableNextColumn();
            ImGui::Text("%.1f, %.1f", inv.Grid.GridScreenX, inv.Grid.GridScreenY);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Cell Size");
            ImGui::TableNextColumn(); ImGui::Text("%.2f", inv.Grid.CellSize);
        }
        ImGui::EndTable();
    }

    // --- Visual slot grid -------------------------------------------------
    if (inv.TotalBoxesX > 0 && inv.TotalBoxesY > 0 && ImGui::TreeNode("Inventory Slots")) {
        std::vector<uint8_t> occupied(
            static_cast<size_t>(inv.TotalBoxesX) * inv.TotalBoxesY, 0);
        for (const auto& it : inv.Items) {
            for (int yy = 0; yy < it.Height; ++yy)
                for (int xx = 0; xx < it.Width; ++xx) {
                    int gx = it.SlotX + xx;
                    int gy = it.SlotY + yy;
                    if (gx >= 0 && gx < inv.TotalBoxesX
                        && gy >= 0 && gy < inv.TotalBoxesY) {
                        occupied[static_cast<size_t>(gy) * inv.TotalBoxesX + gx] = 1;
                    }
                }
        }

        float cellSize = 18.0f;
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 origin = ImGui::GetCursorScreenPos();
        for (int y = 0; y < inv.TotalBoxesY; y++) {
            for (int x = 0; x < inv.TotalBoxesX; x++) {
                int idx = y * inv.TotalBoxesX + x;
                bool occ = (idx < static_cast<int>(occupied.size()))
                    && occupied[static_cast<size_t>(idx)];
                ImVec2 p0(origin.x + x * cellSize, origin.y + y * cellSize);
                ImVec2 p1(p0.x + cellSize - 1, p0.y + cellSize - 1);
                ImU32 col = occ
                    ? IM_COL32(80, 200, 80, 255)
                    : IM_COL32(60, 60, 60, 255);
                drawList->AddRectFilled(p0, p1, col);
                drawList->AddRect(p0, p1, IM_COL32(100, 100, 100, 255));
            }
        }
        ImGui::Dummy(ImVec2(inv.TotalBoxesX * cellSize, inv.TotalBoxesY * cellSize));
        ImGui::TreePop();
    }

    // --- Items list with mods --------------------------------------------
    if (!inv.Items.empty() && ImGui::TreeNode("Items")) {
        ImVec4 rarityColors[] = {
            ImVec4(0.8f, 0.8f, 0.8f, 1.0f),
            ImVec4(0.5f, 0.5f, 1.0f, 1.0f),
            ImVec4(1.0f, 1.0f, 0.3f, 1.0f),
            ImVec4(0.8f, 0.5f, 0.2f, 1.0f),
        };
        const char* rarityNames[] = { "Normal", "Magic", "Rare", "Unique" };

        auto renderMods = [ctx](const char* label, const std::vector<PluginSDK::Mod>& mods) {
            if (mods.empty()) return;
            if (ImGui::TreeNode(label)) {
                for (const auto& mod : mods) {
                    // Affix-type tag + display name (additive — raw line follows).
                    const char* genTag = (mod.GenerationType == 1) ? "[PREFIX] "
                                       : (mod.GenerationType == 2) ? "[SUFFIX] "
                                       : (mod.GenerationType == 3) ? "[IMPLICIT] " : "";
                    if (genTag[0]) {
                        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", genTag);
                        ImGui::SameLine(0, 0);
                    }
                    if (!mod.AffixName.empty()) {
                        ImGui::TextColored(ImVec4(0.8f, 0.6f, 0.9f, 1.0f), "%s ", mod.AffixName.c_str());
                        ImGui::SameLine(0, 0);
                    }
                    bool hasV0 = !std::isnan(mod.Value0);
                    bool hasV1 = !std::isnan(mod.Value1);
                    if (hasV0 && hasV1)
                        ImGui::Text("%s: %.0f - %.0f", mod.Name.c_str(), mod.Value0, mod.Value1);
                    else if (hasV0)
                        ImGui::Text("%s: %.0f", mod.Name.c_str(), mod.Value0);
                    else
                        ImGui::Text("%s", mod.Name.c_str());
                    if (ImGui::IsItemHovered() && ImGui::IsItemClicked())
                        ImGui::SetClipboardText(mod.Name.c_str());
                    // In-game-style line via the host .csd formatter.
                    float v0 = std::isnan(mod.Value0) ? 0.0f : mod.Value0;
                    float v1 = std::isnan(mod.Value1) ? 0.0f : mod.Value1;
                    std::string ig = ctx->Inventory.FormatStat(mod.StatKey, v0, v1);
                    if (!ig.empty())
                        ImGui::TextColored(ImVec4(0.55f, 0.65f, 1.0f, 1.0f), "    %s", ig.c_str());
                }
                ImGui::TreePop();
            }
        };

        for (size_t i = 0; i < inv.Items.size(); i++) {
            const auto& item = inv.Items[i];
            int ri = (item.Rarity < 0) ? 0 : (item.Rarity > 3 ? 3 : item.Rarity);

            std::string label = !item.UniqueName.empty()
                ? item.UniqueName
                : (!item.BaseTypeName.empty() ? item.BaseTypeName : item.Path);

            char itemNodeLabel[256];
            snprintf(itemNodeLabel, sizeof(itemNodeLabel), "%s##item%zu", label.c_str(), i);
            ImGui::PushStyleColor(ImGuiCol_Text, rarityColors[ri]);
            bool open = ImGui::TreeNode(itemNodeLabel);
            ImGui::PopStyleColor();

            if (open) {
                if (ImGui::BeginTable("ItemT", 2,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Address");
                    ImGui::TableNextColumn();
                    char ab3[20]; snprintf(ab3, sizeof(ab3), "0x%llX",
                                           static_cast<unsigned long long>(item.Address));
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.6f, 1.0f), "%s", ab3);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Slot");
                    ImGui::TableNextColumn();
                    ImGui::Text("(%d, %d) %dx%d", item.SlotX, item.SlotY, item.Width, item.Height);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Rarity");
                    ImGui::TableNextColumn(); ImGui::Text("%s", rarityNames[ri]);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Stack");
                    ImGui::TableNextColumn(); ImGui::Text("%d", item.StackCount);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Path");
                    ImGui::TableNextColumn(); ImGui::TextWrapped("%s", item.Path.c_str());

                    ImGui::EndTable();
                }

                PluginSDK::ItemMods mods = ctx->Inventory.ReadItemMods(item.Address);
                if (mods.Valid) {
                    renderMods("Implicit Mods", mods.ImplicitMods);
                    renderMods("Explicit Mods", mods.ExplicitMods);
                    renderMods("Enchant Mods", mods.EnchantMods);
                    renderMods("Hellscape Mods", mods.HellscapeMods);
                    renderMods("Crucible Mods", mods.CrucibleMods);
                    int total = static_cast<int>(
                        mods.ImplicitMods.size() + mods.ExplicitMods.size() +
                        mods.EnchantMods.size() + mods.HellscapeMods.size() +
                        mods.CrucibleMods.size());
                    if (total == 0)
                        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(no mods)");
                } else {
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(mods unavailable)");
                }

                // Item base defensive stats. Energy Shield is the in-game
                // (computed) value: round((baseES + flat131) * (1 + %132/100)).
                PluginSDK::ItemBaseStats bs = ctx->Inventory.ReadItemBaseStats(item.Address);
                auto agg = ctx->Inventory.ReadItemAggregatedStats(item.Address);
                int flatES = 0, pctES = 0;
                for (const auto& kv : agg) {
                    if (kv.first == 131) flatES = kv.second;
                    else if (kv.first == 132) pctES = kv.second;
                }
                int es = (bs.EnergyShield > 0)
                    ? static_cast<int>(std::lround((bs.EnergyShield + flatES) * (1.0 + pctES / 100.0)))
                    : 0;
                if (bs.Valid && (es || bs.Ward || bs.Armour || bs.Evasion)
                    && ImGui::TreeNode("Base Stats")) {
                    if (es)         ImGui::Text("Energy Shield: %d", es);
                    if (bs.Ward)    ImGui::Text("Runic Ward: %d", bs.Ward);
                    if (bs.Armour)  ImGui::Text("Armour: %d", bs.Armour);
                    if (bs.Evasion) ImGui::Text("Evasion: %d", bs.Evasion);
                    ImGui::TreePop();
                }

                // Aggregated stats — only the known waystone/map header props.
                bool anyKnown = false;
                for (const auto& kv : agg)
                    if (kv.first >= 8205 && kv.first <= 8209) { anyKnown = true; break; }
                if (anyKnown && ImGui::TreeNode("Aggregated Stats")) {
                    for (const auto& kv : agg) {
                        const char* lbl = (kv.first == 8205) ? "Item Rarity"
                                        : (kv.first == 8206) ? "Pack Size"
                                        : (kv.first == 8207) ? "Monster Rarity"
                                        : (kv.first == 8208) ? "Monster Effectiveness"
                                        : (kv.first == 8209) ? "Waystone Drop Chance" : nullptr;
                        if (lbl) ImGui::Text("%s: %+d%%", lbl, kv.second);
                    }
                    ImGui::TreePop();
                }

                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }
}

} // namespace Examples
