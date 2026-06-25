#pragma once
// ============================================================================
// ExampleMemory.h — v6 SDK
// ============================================================================
// Demonstrates the v6 MemoryService: base address, module size, pattern
// scanning, raw memory read, and the new RAII string/wstring read paths.
// ============================================================================

#include "sdk/PluginSDK.h"
#include <imgui.h>
#include <vector>
#include <cstdio>

namespace Examples {

inline void DrawMemoryPanel(const PluginSDK::Context* ctx) {
    if (!ctx) return;

    uintptr_t base = ctx->Memory.GetBaseAddress();
    uintptr_t modSize = ctx->Memory.GetModuleSize();

    ImGui::Text("Base Address: 0x%llX", static_cast<unsigned long long>(base));
    ImGui::Text("Module Size: 0x%llX (%llu KB)",
                static_cast<unsigned long long>(modSize),
                static_cast<unsigned long long>(modSize / 1024));
    ImGui::Text("PID: %u", ctx->Game.GetProcessId());

    // --- PE Header Verification ---
    if (base != 0) {
        uint16_t dosSignature = 0;
        ctx->Memory.Read(base, &dosSignature, sizeof(dosSignature));
        ImGui::Text("DOS Signature: 0x%04X (%s)", dosSignature,
                    dosSignature == 0x5A4D ? "MZ - Valid PE" : "Unknown");

        uint32_t peOffset = 0;
        ctx->Memory.Read(base + 0x3C, &peOffset, sizeof(peOffset));
        if (peOffset > 0 && peOffset < modSize) {
            uint32_t peSignature = 0;
            ctx->Memory.Read(base + peOffset, &peSignature, sizeof(peSignature));
            ImGui::Text("PE Signature: 0x%08X (%s)", peSignature,
                        peSignature == 0x00004550 ? "PE\\0\\0 - Valid" : "Invalid");

            uint16_t machine = 0;
            ctx->Memory.Read(base + peOffset + 4, &machine, sizeof(machine));
            ImGui::Text("Machine: 0x%04X (%s)", machine,
                        machine == 0x8664 ? "x64"
                        : machine == 0x14C ? "x86" : "Other");
        }
    }

    ImGui::Separator();

    if (ImGui::CollapsingHeader("Pattern Addresses", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* patterns[] = {
            "Game States", "File Root", "AreaChangeCounter",
            "Terrain Rotator Helper", "Terrain Rotation Selector", "GameCullSize"
        };
        if (ImGui::BeginTable("PatternTable", 2,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Pattern", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 140);
            ImGui::TableHeadersRow();

            for (auto* name : patterns) {
                uintptr_t addr = ctx->Memory.GetPatternAddress(name);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", name);
                ImGui::TableNextColumn();
                if (addr != 0) {
                    char buf[20]; snprintf(buf, sizeof(buf), "0x%llX",
                                           static_cast<unsigned long long>(addr));
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.6f, 1.0f), "%s", buf);
                    if (ImGui::IsItemHovered() && ImGui::IsItemClicked())
                        ImGui::SetClipboardText(buf);
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Not found");
                }
            }
            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("Memory Hex Viewer")) {
        static char addrInput[20] = "";
        static uintptr_t viewAddr = 0;
        static int viewSize = 256;
        ImGui::InputText("Address (hex)", addrInput, sizeof(addrInput));
        ImGui::SameLine();
        if (ImGui::Button("Go")) {
            viewAddr = strtoull(addrInput, nullptr, 16);
        }
        ImGui::SameLine();
        if (ImGui::Button("Base")) {
            viewAddr = base;
            snprintf(addrInput, sizeof(addrInput), "%llX",
                     static_cast<unsigned long long>(base));
        }
        ImGui::SliderInt("Bytes", &viewSize, 16, 512);

        if (viewAddr != 0) {
            std::vector<uint8_t> data(viewSize);
            if (ctx->Memory.Read(viewAddr, data.data(), viewSize)) {
                for (int i = 0; i < viewSize; i += 16) {
                    ImGui::Text("%llX: ", static_cast<unsigned long long>(viewAddr + i));
                    ImGui::SameLine();
                    for (int j = 0; j < 16 && (i + j) < viewSize; j++) {
                        if (j == 8) { ImGui::SameLine(); ImGui::TextDisabled("|"); }
                        ImGui::SameLine();
                        ImGui::Text("%02X", data[i + j]);
                    }
                    ImGui::SameLine(0, 20);
                    char ascii[17] = {};
                    for (int j = 0; j < 16 && (i + j) < viewSize; j++) {
                        uint8_t c = data[i + j];
                        ascii[j] = (c >= 32 && c < 127) ? static_cast<char>(c) : '.';
                    }
                    ImGui::TextDisabled("%s", ascii);
                }
            } else {
                ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1),
                                   "Read failed at 0x%llX",
                                   static_cast<unsigned long long>(viewAddr));
            }
        }
    }

    if (ImGui::CollapsingHeader("MemoryService API")) {
        ImGui::TextWrapped(
            "The v6 MemoryService exposes the following helpers:\n"
            "  ctx->Memory.Read(addr, buf, size)\n"
            "  ctx->Memory.ReadString(addr)           - null-terminated ASCII\n"
            "  ctx->Memory.ReadWString(addr)          - null-terminated wide\n"
            "  ctx->Memory.ReadStdWString(addr)       - StdWString container\n"
            "  ctx->Memory.ReadStdVector(addr, sz, n) - StdVector<T> raw bytes\n"
            "  ctx->Memory.GetBaseAddress()\n"
            "  ctx->Memory.GetModuleSize()\n"
            "  ctx->Memory.GetPatternAddress(name)\n");
    }
}

} // namespace Examples
