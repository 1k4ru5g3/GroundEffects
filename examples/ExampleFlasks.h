#pragma once
// ============================================================================
// ExampleFlasks.h — v6 SDK
// ============================================================================
// Demonstrates the v6 Flasks service: ctx->Flasks exposes the utility belt
// (Inventory[12]) as life/mana flasks (slots 0-1) and charms (slots 0-2),
// each with charges, per-use cost, usable/active state, and a mod-count hint.
// Mirrors the host's Debug "Flasks & Charms" tab using only the public SDK.
//
// Mods are not inlined on Flask/Charm — ModCount is a hint. The actual affix
// list is read on demand via ctx->Inventory.ReadItemMods(EntityAddress), the
// same per-entity mod reader used by the Inventory demo.
// ============================================================================

#include "sdk/PluginSDK.h"
#include <imgui.h>
#include <vector>
#include <cmath>
#include <cstdio>

namespace Examples {

inline void DrawFlaskModsPopup(const PluginSDK::Context* ctx,
                               const char* popupId, uintptr_t entityAddr) {
    if (!ImGui::BeginPopup(popupId)) return;
    ImGui::Text("Entity: 0x%llX", static_cast<unsigned long long>(entityAddr));
    ImGui::Separator();
    PluginSDK::ItemMods mods = ctx->Inventory.ReadItemMods(entityAddr);
    auto renderList = [](const char* label, const std::vector<PluginSDK::Mod>& list) {
        if (list.empty()) return;
        ImGui::TextDisabled("%s:", label);
        for (const auto& m : list) {
            if (!std::isnan(m.Value0))
                ImGui::BulletText("[%s] %s (%.0f)",
                    m.AffixName.c_str(), m.Name.c_str(), m.Value0);
            else
                ImGui::BulletText("[%s] %s", m.AffixName.c_str(), m.Name.c_str());
        }
    };
    if (mods.Valid) {
        renderList("Implicit", mods.ImplicitMods);
        renderList("Explicit", mods.ExplicitMods);
        renderList("Enchant",  mods.EnchantMods);
        int total = static_cast<int>(mods.ImplicitMods.size()
            + mods.ExplicitMods.size() + mods.EnchantMods.size());
        if (total == 0) ImGui::TextDisabled("(no mods)");
    } else {
        ImGui::TextDisabled("(mods unavailable)");
    }
    ImGui::EndPopup();
}

inline void DrawFlasksPanel(const PluginSDK::Context* ctx) {
    if (!ctx) return;

    const ImVec4 cDim   (0.55f, 0.55f, 0.55f, 1.0f);
    const ImVec4 cGood  (0.30f, 0.90f, 0.30f, 1.0f);
    const ImVec4 cOrange(0.95f, 0.55f, 0.20f, 1.0f);
    const ImVec4 cHead  (0.40f, 0.80f, 1.00f, 1.0f);

    ImGui::Text("Slot counts: %d flasks / %d charms",
        ctx->Flasks.FlaskSlotCount(), ctx->Flasks.CharmSlotCount());
    ImGui::Spacing();

    // ---- Flasks ----------------------------------------------------------
    ImGui::TextColored(cHead, "Flasks");
    std::vector<PluginSDK::Flask> flasks = ctx->Flasks.AllFlasks();
    if (ImGui::BeginTable("##exFlasks", 9,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("#");
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Chg");
        ImGui::TableSetupColumn("PerUse");
        ImGui::TableSetupColumn("Eff.");
        ImGui::TableSetupColumn("Usable");
        ImGui::TableSetupColumn("Active");
        ImGui::TableSetupColumn("Mods");
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < flasks.size(); ++i) {
            const auto& f = flasks[i];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("%zu", i);

            ImGui::TableSetColumnIndex(1);
            if (!f.Valid)      ImGui::TextColored(cDim, "-");
            else if (f.IsLife) ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1), "Life");
            else if (f.IsMana) ImGui::TextColored(ImVec4(0.40f, 0.55f, 0.95f, 1), "Mana");
            else               ImGui::TextColored(ImVec4(0.95f, 0.85f, 0.30f, 1), "Utility");

            ImGui::TableSetColumnIndex(2);
            if (f.Valid) ImGui::TextUnformatted(f.Name.c_str());
            else         ImGui::TextColored(cDim, "-");

            ImGui::TableSetColumnIndex(3);
            if (f.Valid) ImGui::Text("%d", f.ChargesCurrent);
            else         ImGui::TextColored(cDim, "-");

            ImGui::TableSetColumnIndex(4);
            if (f.Valid) ImGui::Text("%d", f.PerUseBase);
            else         ImGui::TextColored(cDim, "-");

            ImGui::TableSetColumnIndex(5);
            if (!f.Valid) ImGui::TextColored(cDim, "-");
            else ImGui::TextColored(f.PerUseEffective > f.PerUseBase ? cOrange
                                                                     : ImVec4(1, 1, 1, 1),
                                    "%d", f.PerUseEffective);

            ImGui::TableSetColumnIndex(6);
            if (!f.Valid) ImGui::TextColored(cDim, "-");
            else          ImGui::TextColored(f.Usable ? cGood : cDim, f.Usable ? "yes" : "no");

            ImGui::TableSetColumnIndex(7);
            if (!f.Valid) ImGui::TextColored(cDim, "-");
            else          ImGui::TextColored(f.Active ? cGood : cDim, f.Active ? "on" : "off");

            ImGui::TableSetColumnIndex(8);
            if (f.Valid) {
                char btn[64];
                snprintf(btn, sizeof(btn), "%d##exfm%zu", f.ModCount, i);
                if (ImGui::SmallButton(btn)) ImGui::OpenPopup(btn);
                DrawFlaskModsPopup(ctx, btn, f.EntityAddress);
            } else {
                ImGui::TextColored(cDim, "-");
            }
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();

    // ---- Charms ----------------------------------------------------------
    ImGui::TextColored(cHead, "Charms");
    std::vector<PluginSDK::Charm> charms = ctx->Flasks.AllCharms();
    if (ImGui::BeginTable("##exCharms", 6,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("#");
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Chg");
        ImGui::TableSetupColumn("PerUse");
        ImGui::TableSetupColumn("Active");
        ImGui::TableSetupColumn("Mods");
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < charms.size(); ++i) {
            const auto& c = charms[i];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("%zu", i);

            ImGui::TableSetColumnIndex(1);
            if (c.Valid) ImGui::TextUnformatted(c.Name.c_str());
            else         ImGui::TextColored(cDim, "-");

            ImGui::TableSetColumnIndex(2);
            if (c.Valid) ImGui::Text("%d", c.ChargesCurrent);
            else         ImGui::TextColored(cDim, "-");

            ImGui::TableSetColumnIndex(3);
            if (c.Valid) ImGui::Text("%d", c.PerUseBase);
            else         ImGui::TextColored(cDim, "-");

            ImGui::TableSetColumnIndex(4);
            if (!c.Valid) ImGui::TextColored(cDim, "-");
            else          ImGui::TextColored(c.Active ? cGood : cDim, c.Active ? "on" : "off");

            ImGui::TableSetColumnIndex(5);
            if (c.Valid) {
                char btn[64];
                snprintf(btn, sizeof(btn), "%d##excm%zu", c.ModCount, i);
                if (ImGui::SmallButton(btn)) ImGui::OpenPopup(btn);
                DrawFlaskModsPopup(ctx, btn, c.EntityAddress);
            } else {
                ImGui::TextColored(cDim, "-");
            }
        }
        ImGui::EndTable();
    }
}

} // namespace Examples
