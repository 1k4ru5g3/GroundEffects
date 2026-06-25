#pragma once
// ============================================================================
// ExamplePrices.h — v6 SDK
// ============================================================================
// Demonstrates the v6 Prices service: ctx->Prices exposes market prices the
// host loads ONCE per session from poe2scout on a background thread. Every
// plugin (and the built-in radar/overlays) shares this single price database,
// so plugins never fetch prices themselves — they just look them up.
//
//   - GetStatus()  : gate UI on .loaded before trusting prices; per-category
//                    counts show how far the background loader got.
//   - GetRates()   : Divine / Exalted -> Chaos conversion rates.
//   - LookupPrice(): fuzzy host-side match by display name; returns chaos +
//                    divine + exalt + the poe2scout category that matched.
//
// All prices are Chaos-denominated. The panel below shows the load status, an
// interactive name lookup, a live common-currency table, and the real-world
// workflow: scan an inventory and value every item in chaos.
// ============================================================================

#include "sdk/PluginSDK.h"
#include <imgui.h>
#include <vector>
#include <string>
#include <utility>
#include <cstdio>

namespace Examples {

// Render one PriceResult inline: chaos | divine | exalt [category].
inline void DrawPriceResultRow(const PluginSDK::PriceResult& p) {
    if (!p.found) {
        ImGui::TextColored(ImVec4(0.60f, 0.60f, 0.60f, 1), "not found");
        return;
    }
    ImGui::Text("%.1fc", p.chaos);
    ImGui::SameLine(); ImGui::TextDisabled("|");
    ImGui::SameLine(); ImGui::Text("%.2f div", p.divine);
    ImGui::SameLine(); ImGui::TextDisabled("|");
    ImGui::SameLine(); ImGui::Text("%.1f ex", p.exalt);
    if (!p.category.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.50f, 0.80f, 1.00f, 1), "[%s]", p.category.c_str());
    }
}

inline void DrawPricesPanel(const PluginSDK::Context* ctx) {
    if (!ctx) return;

    const ImVec4 cHead(0.40f, 0.80f, 1.00f, 1.0f);
    const ImVec4 cDim (0.55f, 0.55f, 0.55f, 1.0f);
    const ImVec4 cGood(0.30f, 0.90f, 0.30f, 1.0f);
    const ImVec4 cWarn(0.95f, 0.75f, 0.25f, 1.0f);

    // ---- Load status -----------------------------------------------------
    // Always gate on GetStatus().loaded before trusting prices: the host loads
    // them on a background thread, so they may not be ready on the first frame.
    PluginSDK::PriceStatus st = ctx->Prices.GetStatus();
    ImGui::TextColored(cHead, "PriceService status");
    if (st.loaded) {
        ImGui::TextColored(cGood, "loaded");
        ImGui::SameLine();
        ImGui::Text("- %d items", st.totalItems);
    } else {
        ImGui::TextColored(cWarn, "loading prices...");
    }
    ImGui::Text("categories: %d ok / %d pending / %d failed",
                st.catsOk, st.catsPending, st.catsFailed);

    // ---- Conversion rates ------------------------------------------------
    PluginSDK::PriceRates rates = ctx->Prices.GetRates();
    ImGui::Text("1 Divine = %.0fc   |   1 Exalted = %.1fc",
                rates.divineInChaos, rates.exaltedInChaos);

    ImGui::Separator();

    // ---- Interactive lookup ----------------------------------------------
    ImGui::TextColored(cHead, "Look up a price by name");
    static char query[128] = "Divine Orb";
    bool submitted = ImGui::InputText("##exPriceQuery", query, sizeof(query),
                                      ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    submitted |= ImGui::Button("Look up");
    ImGui::SameLine();
    ImGui::TextDisabled("(currency, unique, or base name)");

    static PluginSDK::PriceResult lastResult;
    static std::string            lastQuery;
    if (submitted && query[0]) {
        lastResult = ctx->Prices.LookupPrice(query);
        lastQuery  = query;
    }
    if (!lastQuery.empty()) {
        ImGui::Text("\"%s\":", lastQuery.c_str());
        ImGui::SameLine();
        DrawPriceResultRow(lastResult);
    }

    ImGui::Separator();

    // ---- Common currency (looked up live each frame; cheap) --------------
    ImGui::TextColored(cHead, "Common currency");
    static const char* kPresets[] = {
        "Divine Orb", "Exalted Orb", "Chaos Orb",
        "Orb of Annulment", "Mirror of Kalandra", "Vaal Orb",
    };
    if (ImGui::BeginTable("##exPrices", 2,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Price");
        ImGui::TableHeadersRow();
        for (const char* name : kPresets) {
            PluginSDK::PriceResult p = ctx->Prices.LookupPrice(name);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(name);
            ImGui::TableSetColumnIndex(1); DrawPriceResultRow(p);
        }
        ImGui::EndTable();
    }

    ImGui::Separator();

    // ---- Inventory valuation (on demand) ---------------------------------
    // The real-world workflow (this is what NinjaPricer does): scan an
    // inventory, price each item by its unique-or-base name, sum in chaos.
    // Triggered by a button so the per-item LookupPrice loop runs once, not
    // every frame.
    ImGui::TextColored(cHead, "Value my backpack");
    static std::vector<std::pair<std::string, float>> invPriced;  // name -> chaos
    static float invTotalChaos = 0.f;
    static bool  invScanned    = false;
    if (ImGui::Button("Scan & price inventory")) {
        ctx->Inventory.Scan(0);          // 0 = backpack
        invPriced.clear();
        invTotalChaos = 0.f;
        for (const auto& item : ctx->Inventory.GetItems(0)) {
            std::string name = ctx->Inventory.ReadItemUniqueName(item.Address);
            if (name.empty()) name = ctx->Inventory.ReadItemBaseTypeName(item.Address);
            if (name.empty()) continue;
            PluginSDK::PriceResult p = ctx->Prices.LookupPrice(name);
            if (p.found) {
                float stackChaos = p.chaos * (item.StackCount > 0 ? item.StackCount : 1);
                invPriced.emplace_back(name, stackChaos);
                invTotalChaos += stackChaos;
            }
        }
        invScanned = true;
    }
    if (invScanned) {
        ImGui::SameLine();
        float div = rates.divineInChaos > 0 ? invTotalChaos / rates.divineInChaos : 0.f;
        ImGui::Text("total: %.1fc (%.2f div)", invTotalChaos, div);
        if (invPriced.empty()) {
            ImGui::TextColored(cDim, "(no priced items found in backpack)");
        } else if (ImGui::BeginTable("##exInvPrices", 2,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                ImVec2(0, 160))) {
            ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Chaos");
            ImGui::TableHeadersRow();
            for (const auto& row : invPriced) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(row.first.c_str());
                ImGui::TableSetColumnIndex(1); ImGui::Text("%.1f", row.second);
            }
            ImGui::EndTable();
        }
    }
}

} // namespace Examples
