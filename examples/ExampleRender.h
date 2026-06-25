// Plugins/ExamplePlugin/examples/ExampleRender.h
// Demonstrates every RenderService method.

#pragma once
#include "sdk/PluginSDK.h"
#include <imgui.h>
#include "SdkStatus.h"

namespace Examples {

inline void DrawRenderPanel(const PluginSDK::Context* ctx,
                             const PluginSDK::Snapshot& snap)
{
    if (!ctx) { StatusBadge(StatusLevel::Fail, "Context missing"); return; }

    if (snap.State != PluginSDK::GameState::InGame || !snap.Player.IsValid) {
        StatusBadge(StatusLevel::Warn, "Enter game to exercise projection methods");
        return;
    }
    StatusBadge(StatusLevel::Ok, "RenderService ready");
    ImGui::Separator();

    if (ImGui::CollapsingHeader("WorldToScreen", ImGuiTreeNodeFlags_DefaultOpen)) {
        float sx = 0, sy = 0;
        bool ok = ctx->Render.WorldToScreen(
            snap.Player.WorldX, snap.Player.WorldY,
            snap.Player.WorldZ, sx, sy);
        if (ok) {
            ImGui::Text("Player world (%.1f, %.1f, %.1f) -> screen (%.0f, %.0f)",
                snap.Player.WorldX, snap.Player.WorldY, snap.Player.WorldZ, sx, sy);
            bool onScreen = sx >= 0 && sx < snap.ScreenWidth
                         && sy >= 0 && sy < snap.ScreenHeight;
            StatusBadge(onScreen ? StatusLevel::Ok : StatusLevel::Warn,
                onScreen ? "On screen" : "Off screen (camera angle?)");
        } else {
            StatusBadge(StatusLevel::Warn, "Projection failed (behind camera)");
        }
    }

    if (ImGui::CollapsingHeader("GridToLargeMap")) {
        if (!snap.LargeMap.IsVisible) {
            StatusBadge(StatusLevel::Warn, "Open Large Map (default: TAB) to test");
        } else {
            float sx = 0, sy = 0;
            bool ok = ctx->Render.GridToLargeMap(
                snap.Player.GridPositionX, snap.Player.GridPositionY,
                snap.Player.TerrainHeight, sx, sy);
            if (ok) {
                ImGui::Text("Player grid (%.1f, %.1f) -> large-map (%.0f, %.0f)",
                    snap.Player.GridPositionX, snap.Player.GridPositionY, sx, sy);
                StatusBadge(StatusLevel::Ok, "Projected");
            } else {
                StatusBadge(StatusLevel::Fail, "GridToLargeMap returned false");
            }
        }
    }

    if (ImGui::CollapsingHeader("GridToMiniMap")) {
        if (!snap.MiniMap.IsVisible) {
            StatusBadge(StatusLevel::Warn, "Mini-map not currently visible");
        } else {
            float sx = 0, sy = 0;
            bool ok = ctx->Render.GridToMiniMap(
                snap.Player.GridPositionX, snap.Player.GridPositionY,
                snap.Player.TerrainHeight, sx, sy);
            if (ok) {
                ImGui::Text("Player grid (%.1f, %.1f) -> mini-map (%.0f, %.0f)",
                    snap.Player.GridPositionX, snap.Player.GridPositionY, sx, sy);
                StatusBadge(StatusLevel::Ok, "Projected");
            } else {
                StatusBadge(StatusLevel::Fail, "GridToMiniMap returned false");
            }
        }
    }

    if (ImGui::CollapsingHeader("Large Map Transform")) {
        PluginSDK::MapTransform t = ctx->Render.GetLargeMapTransform();
        ImGui::Text("IsVisible: %s", t.IsVisible ? "yes" : "no");
        ImGui::Text("Center: (%.2f, %.2f)", t.CenterX, t.CenterY);
        ImGui::Text("Scale:  (%.4f, %.4f)", t.ScaleX, t.ScaleY);
        ImGui::Text("Player grid origin: (%.2f, %.2f)", t.PlayerGridX, t.PlayerGridY);
    }

    if (ImGui::CollapsingHeader("Mini Map Transform")) {
        PluginSDK::MapTransform t = ctx->Render.GetMiniMapTransform();
        ImGui::Text("IsVisible: %s", t.IsVisible ? "yes" : "no");
        ImGui::Text("Center: (%.2f, %.2f)", t.CenterX, t.CenterY);
        ImGui::Text("Scale:  (%.4f, %.4f)", t.ScaleX, t.ScaleY);
        ImGui::Text("Player grid origin: (%.2f, %.2f)", t.PlayerGridX, t.PlayerGridY);
    }
}

} // namespace Examples
