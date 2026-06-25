// Plugins/ExamplePlugin/examples/ExampleLog.h
// Demonstrates every LogService method.

#pragma once
#include "sdk/PluginSDK.h"
#include <imgui.h>
#include "SdkStatus.h"

namespace Examples {

inline void DrawLogPanel(const PluginSDK::Context* ctx, int& sentCount) {
    if (!ctx) { StatusBadge(StatusLevel::Fail, "Context missing"); return; }

    StatusBadge(StatusLevel::Ok, "LogService always available");
    ImGui::Separator();

    ImGui::TextWrapped("Click a button to fire a log entry. Check the host's "
        "Logs tab (or the console / log file) to verify the message appears.");
    ImGui::Spacing();

    if (ImGui::Button("Log.Debug")) {
        ctx->Log.Debug("ExamplePlugin: Debug test message");
        ++sentCount;
    }
    ImGui::SameLine();
    if (ImGui::Button("Log.Info")) {
        ctx->Log.Info("ExamplePlugin: Info test message");
        ++sentCount;
    }
    ImGui::SameLine();
    if (ImGui::Button("Log.Warn")) {
        ctx->Log.Warn("ExamplePlugin: Warn test message");
        ++sentCount;
    }
    ImGui::SameLine();
    if (ImGui::Button("Log.Error")) {
        ctx->Log.Error("ExamplePlugin: Error test message");
        ++sentCount;
    }
    ImGui::Separator();

    if (ImGui::Button("Log.Log(\"Info\", ...)")) {
        ctx->Log.Log("Info", "ExamplePlugin: direct Log() call with explicit level");
        ++sentCount;
    }

    ImGui::Spacing();
    ImGui::Text("Total messages fired this session: %d", sentCount);
}

} // namespace Examples
