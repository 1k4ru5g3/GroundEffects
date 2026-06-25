#pragma once
// ============================================================================
// ExampleUiExplorer.h — v6 SDK
// ============================================================================
// Navigates the game's UI element tree using ctx->Ui.* services. No raw
// offsets — the v6 UiService::Read returns a populated UiElement struct
// already containing parent, child count, position, size, scale, flags,
// visibility, string-id address, etc.
// ============================================================================

#include "sdk/PluginSDK.h"
#include <imgui.h>
#include <deque>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

namespace Examples {

class PluginUiExplorer {
public:
    void Draw(const PluginSDK::Context* ctx) {
        if (!ctx) {
            ImGui::TextDisabled("Context unavailable");
            return;
        }
        m_Ctx = ctx;

        if (m_CurrentAddress == 0) {
            uintptr_t gameUi = ctx->Ui.GetGameUiRoot();
            if (gameUi != 0) {
                m_CurrentAddress = gameUi;
                m_History.clear();
                RefreshElement();
            }
        }

        if (m_CurrentAddress == 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f),
                "GameUI Address not found. Game may not be in InGameState.");
            return;
        }

        RefreshElement();

        DrawBreadcrumbs();
        ImGui::Spacing();
        DrawSearchPanel();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        std::string currentSid = ctx->Ui.GetStringId(m_CurrentAddress);

        ImGui::BeginGroup();
        if (!currentSid.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f),
                "Current: \"%s\"", currentSid.c_str());
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f),
                "Current: 0x%llX", static_cast<unsigned long long>(m_CurrentAddress));
        }
        ImGui::EndGroup();
        if (ImGui::IsItemHovered()) {
            DrawHighlighter(m_CurrentAddress, IM_COL32(255, 255, 0, 255),
                currentSid.empty() ? "Current" : currentSid);
            ImGui::SetTooltip("Click to copy address");
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                char buf[20]; snprintf(buf, sizeof(buf), "0x%llX",
                                       static_cast<unsigned long long>(m_CurrentAddress));
                ImGui::SetClipboardText(buf);
            }
        }
        ImGui::Spacing();

        // Navigation buttons
        PluginSDK::UiElement currentElem = ctx->Ui.Read(m_CurrentAddress);
        bool hasParent = (currentElem.ParentAddress != 0);
        bool hasSelectedChild = (m_CurrentChildIndex >= 0 &&
            m_CurrentChildIndex < static_cast<int>(m_ChildAddresses.size()));

        if (!hasParent) ImGui::BeginDisabled();
        if (ImGui::Button("Parent")) NavigateTo(currentElem.ParentAddress);
        if (!hasParent) ImGui::EndDisabled();

        ImGui::SameLine();
        if (!hasSelectedChild) ImGui::BeginDisabled();
        if (ImGui::Button("Child")) NavigateTo(m_ChildAddresses[m_CurrentChildIndex]);
        if (!hasSelectedChild) ImGui::EndDisabled();

        ImGui::SameLine();
        if (m_History.empty()) ImGui::BeginDisabled();
        if (ImGui::Button("Back")) {
            if (!m_History.empty()) {
                uintptr_t prevAddr = m_History.back().Address;
                m_History.pop_back();
                m_CurrentAddress = prevAddr;
                m_CurrentChildIndex = -1;
                RefreshElement();
            }
        }
        if (m_History.empty()) ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Root")) {
            uintptr_t gameUi = ctx->Ui.GetGameUiRoot();
            if (gameUi != 0) {
                m_History.clear();
                m_CurrentAddress = gameUi;
                m_CurrentChildIndex = -1;
                RefreshElement();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("UiRoot")) {
            uintptr_t ui = ctx->Ui.GetUiRoot();
            if (ui != 0) {
                m_History.clear();
                m_CurrentAddress = ui;
                m_CurrentChildIndex = -1;
                RefreshElement();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Copy Info"))
            CopyElementInfo(currentSid);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::CollapsingHeader("Children", ImGuiTreeNodeFlags_DefaultOpen)) {
            DrawChildrenList();
        }

        ImGui::Spacing();
        if (ImGui::CollapsingHeader("Element Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
            DrawElementPropertiesFor(m_CurrentAddress);
        }

        if (hasSelectedChild) {
            ImGui::Spacing();
            if (ImGui::CollapsingHeader("Selected Child Preview")) {
                ImGui::Indent();
                DrawElementPropertiesFor(m_ChildAddresses[m_CurrentChildIndex]);
                ImGui::Unindent();
            }
        }
    }

private:
    // Sanity-check an address before treating it as a UI element. The bridge
    // already filters obviously bad pointers (kernel range, < 0x10000), but
    // we also reject addresses that don't produce a valid UiElement to avoid
    // navigating into corrupted memory after a parent-pointer scribble.
    bool IsNavigable(uintptr_t addr) const {
        if (addr == 0 || addr < 0x10000ull) return false;
        if (addr > 0x00007FFFFFFFFFFFull) return false;  // kernel space
        if (!m_Ctx) return false;
        PluginSDK::UiElement probe = m_Ctx->Ui.Read(addr);
        return probe.Valid;
    }

    void NavigateTo(uintptr_t addr) {
        if (!IsNavigable(addr)) return;
        if (m_CurrentAddress != 0) {
            BreadcrumbEntry entry;
            entry.Address = m_CurrentAddress;
            entry.Label = m_Ctx->Ui.GetStringId(m_CurrentAddress);
            if (entry.Label.empty()) {
                char buf[20]; snprintf(buf, sizeof(buf), "0x%llX",
                                       static_cast<unsigned long long>(m_CurrentAddress));
                entry.Label = buf;
            }
            if (m_History.empty() || m_History.back().Address != entry.Address) {
                m_History.push_back(entry);
                if (m_History.size() > 32) m_History.pop_front();
            }
        }
        m_CurrentAddress = addr;
        m_CurrentChildIndex = -1;
        RefreshElement();
    }

    void RefreshElement() {
        m_ChildAddresses.clear();
        if (m_CurrentAddress == 0 || !m_Ctx) return;
        m_ChildAddresses = m_Ctx->Ui.GetChildren(m_CurrentAddress);
    }

    void DrawHighlighter(uintptr_t addr, ImU32 color, const std::string& label = "") {
        float x, y, w, h;
        if (!m_Ctx->Ui.ComputeScreenRect(addr, x, y, w, h)) return;
        if (w <= 0 || h <= 0) return;
        ImVec2 pos(x, y);
        ImVec2 size(w, h);
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        ImVec2 maxPos(pos.x + size.x, pos.y + size.y);
        dl->AddRect(pos, maxPos, color, 0.0f, 0, 2.0f);
        ImU32 fillColor = (color & 0x00FFFFFF) | 0x18000000;
        dl->AddRectFilled(pos, maxPos, fillColor);
        if (!label.empty()) {
            ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
            ImVec2 textBgMax(pos.x + textSize.x + 4, pos.y + textSize.y + 2);
            dl->AddRectFilled(pos, textBgMax, IM_COL32(0, 0, 0, 180));
            dl->AddText(ImVec2(pos.x + 2, pos.y + 1), color, label.c_str());
        }
    }

    void DrawBreadcrumbs() {
        if (m_History.empty()) return;

        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Path:");
        ImGui::SameLine();

        int startIdx = 0;
        if (m_History.size() > 6) {
            startIdx = static_cast<int>(m_History.size()) - 6;
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "...");
            ImGui::SameLine();
        }

        for (int i = startIdx; i < static_cast<int>(m_History.size()); i++) {
            auto& entry = m_History[i];
            std::string displayLabel = entry.Label;
            if (displayLabel.length() > 20) displayLabel = displayLabel.substr(0, 17) + "...";

            ImGui::PushID(i);
            if (ImGui::SmallButton(displayLabel.c_str())) {
                uintptr_t targetAddr = entry.Address;
                while (m_History.size() > static_cast<size_t>(i))
                    m_History.pop_back();
                m_CurrentAddress = targetAddr;
                m_CurrentChildIndex = -1;
                RefreshElement();
                ImGui::PopID();
                return;
            }
            ImGui::PopID();

            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), ">");
            ImGui::SameLine();
        }

        std::string currentLabel = m_Ctx->Ui.GetStringId(m_CurrentAddress);
        if (currentLabel.empty()) {
            char buf[20]; snprintf(buf, sizeof(buf), "0x%llX",
                                   static_cast<unsigned long long>(m_CurrentAddress));
            currentLabel = buf;
        }
        if (currentLabel.length() > 20) currentLabel = currentLabel.substr(0, 17) + "...";
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s", currentLabel.c_str());
    }

    void DrawSearchPanel() {
        ImGui::SetNextItemWidth(200);
        bool searchTriggered = ImGui::InputTextWithHint("##UiSearch", "Search StringId...",
            m_SearchBuffer, sizeof(m_SearchBuffer), ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::SameLine();
        if (ImGui::Button("Search") || searchTriggered) {
            if (std::strlen(m_SearchBuffer) > 0) {
                m_SearchResults.clear();
                std::string query(m_SearchBuffer);
                std::transform(query.begin(), query.end(), query.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

                uintptr_t searchRoot = m_Ctx->Ui.GetGameUiRoot();
                if (searchRoot != 0) {
                    SearchTree(searchRoot, query, 0, 15);
                }
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            m_SearchBuffer[0] = '\0';
            m_SearchResults.clear();
        }

        if (!m_SearchResults.empty()) {
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "Found %d result(s):",
                static_cast<int>(m_SearchResults.size()));

            float listHeight = (std::min)(120.0f,
                static_cast<float>(m_SearchResults.size()) * 20.0f + 4.0f);
            if (ImGui::BeginListBox("##SearchResults", ImVec2(-1, listHeight))) {
                for (auto& [addr, name] : m_SearchResults) {
                    char label[256];
                    snprintf(label, sizeof(label), "0x%llX - %s",
                             static_cast<unsigned long long>(addr), name.c_str());
                    if (ImGui::Selectable(label)) NavigateTo(addr);
                    if (ImGui::IsItemHovered())
                        DrawHighlighter(addr, IM_COL32(0, 255, 255, 255), name);
                }
                ImGui::EndListBox();
            }
        }
    }

    void SearchTree(uintptr_t addr, const std::string& query, int depth, int maxDepth) {
        if (depth > maxDepth || addr == 0 || m_SearchResults.size() >= 100) return;
        // Filter clearly-bogus addresses (kernel range, low pointers) before
        // trusting the bridge to RPM through them.
        if (addr < 0x10000ull || addr > 0x00007FFFFFFFFFFFull) return;

        std::string stringId = m_Ctx->Ui.GetStringId(addr);
        if (!stringId.empty()) {
            std::string lower = stringId;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (lower.find(query) != std::string::npos) {
                m_SearchResults.emplace_back(addr, stringId);
            }
        }

        auto kids = m_Ctx->Ui.GetChildren(addr);
        for (uintptr_t childAddr : kids) {
            if (m_SearchResults.size() >= 100) break;
            SearchTree(childAddr, query, depth + 1, maxDepth);
        }
    }

    void DrawChildrenList() {
        int childCount = static_cast<int>(m_ChildAddresses.size());
        ImGui::Text("Children: %d", childCount);
        if (childCount == 0) return;

        ImGui::SameLine(150);

        if (ImGui::SmallButton("|<")) m_CurrentChildIndex = 0;
        ImGui::SameLine();
        if (ImGui::SmallButton("<")) {
            if (m_CurrentChildIndex > 0) m_CurrentChildIndex--;
            else m_CurrentChildIndex = childCount - 1;
        }
        ImGui::SameLine();
        ImGui::Text("%d/%d", m_CurrentChildIndex + 1, childCount);
        ImGui::SameLine();
        if (ImGui::SmallButton(">")) {
            if (m_CurrentChildIndex < childCount - 1) m_CurrentChildIndex++;
            else m_CurrentChildIndex = 0;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton(">|")) m_CurrentChildIndex = childCount - 1;

        std::string preview = (m_CurrentChildIndex >= 0)
            ? std::to_string(m_CurrentChildIndex) : "None";

        if (ImGui::BeginCombo("##Children", preview.c_str())) {
            for (int i = 0; i < childCount; ++i) {
                uintptr_t childAddr = m_ChildAddresses[i];
                bool isSelected = (m_CurrentChildIndex == i);
                bool isVis = m_Ctx->Ui.IsVisible(childAddr);
                PluginSDK::UiElement elem = m_Ctx->Ui.Read(childAddr);

                std::stringstream ss;
                ss << i;
                std::string childSid = m_Ctx->Ui.GetStringId(childAddr);
                if (!childSid.empty()) {
                    if (childSid.length() > 30) childSid = childSid.substr(0, 27) + "...";
                    ss << " \"" << childSid << "\"";
                }
                if (elem.ChildCount > 0) ss << " [" << elem.ChildCount << "]";

                float gx, gy, gw, gh;
                if (m_Ctx->Ui.ComputeScreenRect(childAddr, gx, gy, gw, gh) && gw > 0 && gh > 0) {
                    ss << " (" << static_cast<int>(gw) << "x" << static_cast<int>(gh) << ")";
                }

                ImVec4 textColor = isVis
                    ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f)
                    : ImVec4(0.6f, 0.6f, 0.6f, 1.0f);

                ImGui::PushStyleColor(ImGuiCol_Text, textColor);
                if (ImGui::Selectable(ss.str().c_str(), isSelected))
                    m_CurrentChildIndex = i;
                ImGui::PopStyleColor();

                if (isSelected) ImGui::SetItemDefaultFocus();

                if (ImGui::IsItemHovered()) {
                    std::string hoverLabel = std::to_string(i);
                    if (!childSid.empty()) hoverLabel += " " + childSid;
                    DrawHighlighter(childAddr, IM_COL32(255, 255, 0, 255), hoverLabel);
                }
            }
            ImGui::EndCombo();
        }

        if (m_CurrentChildIndex >= 0 && m_CurrentChildIndex < childCount) {
            uintptr_t childAddr = m_ChildAddresses[m_CurrentChildIndex];
            std::string label = std::to_string(m_CurrentChildIndex);
            std::string cs = m_Ctx->Ui.GetStringId(childAddr);
            if (!cs.empty()) label += " " + cs;
            DrawHighlighter(childAddr, IM_COL32(255, 80, 80, 255), label);
        }
    }

    void DrawElementPropertiesFor(uintptr_t addr) {
        if (addr == 0) return;

        std::string stringId = m_Ctx->Ui.GetStringId(addr);
        PluginSDK::UiElement elem = m_Ctx->Ui.Read(addr);

        if (!stringId.empty()) {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.6f, 1.0f),
                "StringId: \"%s\"", stringId.c_str());
            if (ImGui::IsItemHovered() && ImGui::IsItemClicked())
                ImGui::SetClipboardText(stringId.c_str());
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "StringId: (empty)");
        }
        ImGui::Spacing();

        if (ImGui::BeginTable("UiProps", 2,
            ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 180.0f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

            // Address
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Address");
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.6f, 1.0f), "0x%llX",
                               static_cast<unsigned long long>(addr));
            if (ImGui::IsItemHovered() && ImGui::IsItemClicked()) {
                char buf[20]; snprintf(buf, sizeof(buf), "0x%llX",
                                       static_cast<unsigned long long>(addr));
                ImGui::SetClipboardText(buf);
            }

            // Parent
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Parent");
            ImGui::TableNextColumn();
            if (elem.ParentAddress != 0)
                ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "0x%llX",
                                   static_cast<unsigned long long>(elem.ParentAddress));
            else
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(root)");

            // Visible
            bool vis = elem.IsVisible;
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Visible (local)");
            ImGui::TableNextColumn();
            ImGui::TextColored(vis ? ImVec4(0.3f,1,0.3f,1) : ImVec4(1,0.3f,0.3f,1),
                               vis ? "TRUE" : "FALSE");

            // Children
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Children");
            ImGui::TableNextColumn();
            ImGui::Text("%d", elem.ChildCount);

            // --- Position ---
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "--- Position ---");
            ImGui::TableNextColumn();

            float sx, sy, sw, sh;
            if (m_Ctx->Ui.ComputeScreenRect(addr, sx, sy, sw, sh)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Screen Pos");
                ImGui::TableNextColumn(); ImGui::Text("%.1f, %.1f", sx, sy);

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Screen Size");
                ImGui::TableNextColumn(); ImGui::Text("%.1f x %.1f", sw, sh);
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Relative Pos");
            ImGui::TableNextColumn();
            ImGui::Text("%.1f, %.1f", elem.RelativeX, elem.RelativeY);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Pos Modifier");
            ImGui::TableNextColumn();
            if (elem.HasPositionModifier)
                ImGui::Text("%.1f, %.1f", elem.PositionModX, elem.PositionModY);
            else
                ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1), "disabled");

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Unscaled Size");
            ImGui::TableNextColumn();
            ImGui::Text("%.1f x %.1f", elem.UnscaledWidth, elem.UnscaledHeight);

            // --- Scale ---
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "--- Scale ---");
            ImGui::TableNextColumn();

            const char* scaleNames[] = { "None (Multiply)", "Width/Width", "Height/Height", "Width/Height" };
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Scale Index");
            ImGui::TableNextColumn();
            ImGui::Text("%u (%s)", elem.ScaleIndex,
                        (elem.ScaleIndex <= 3) ? scaleNames[elem.ScaleIndex] : "Unknown");

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Local Scale Mult");
            ImGui::TableNextColumn();
            ImGui::Text("%.4f", elem.LocalScaleMultiplier);

            // --- Flags ---
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "--- Flags ---");
            ImGui::TableNextColumn();

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Flags (raw)");
            ImGui::TableNextColumn(); ImGui::Text("0x%08X", elem.Flags);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Element Type");
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "0x%04X (%u)",
                               elem.ElementType, elem.ElementType);

            // Text from UI element (if any)
            std::string text = m_Ctx->Ui.GetText(addr);
            if (!text.empty()) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Text");
                ImGui::TableNextColumn();
                ImGui::TextColored(ImVec4(0.8f, 0.9f, 1, 1), "\"%s\"", text.c_str());
            }

            ImGui::EndTable();
        }
    }

    void CopyElementInfo(const std::string& currentSid) {
        PluginSDK::UiElement elem = m_Ctx->Ui.Read(m_CurrentAddress);
        float sx = 0, sy = 0, sw = 0, sh = 0;
        m_Ctx->Ui.ComputeScreenRect(m_CurrentAddress, sx, sy, sw, sh);

        std::ostringstream ss;
        ss << "=== UI Element Debug ===\n";
        ss << "Address: 0x" << std::hex << m_CurrentAddress << "\n";
        ss << "StringId: " << currentSid << "\n";
        ss << std::dec << std::fixed << std::setprecision(1);
        ss << "Screen Pos: " << sx << ", " << sy << "\n";
        ss << "Screen Size: " << sw << " x " << sh << "\n";
        ss << "Unscaled Size: " << elem.UnscaledWidth << " x " << elem.UnscaledHeight << "\n";
        ss << "Visible: " << (elem.IsVisible ? "true" : "false") << "\n";
        ss << "Children: " << m_ChildAddresses.size() << "\n";
        ss << "Flags: 0x" << std::hex << elem.Flags << "\n";
        ss << "ScaleIndex: " << std::dec << static_cast<int>(elem.ScaleIndex) << "\n";
        ss << "ScaleMultiplier: " << elem.LocalScaleMultiplier << "\n";
        ss << "RelPos: " << elem.RelativeX << ", " << elem.RelativeY << "\n";
        ss << "Parent: 0x" << std::hex << elem.ParentAddress << "\n";
        ss << "ElementType: 0x" << std::hex << elem.ElementType
           << " (" << std::dec << elem.ElementType << ")\n";
        ImGui::SetClipboardText(ss.str().c_str());
    }

    const PluginSDK::Context* m_Ctx = nullptr;
    uintptr_t m_CurrentAddress = 0;
    int m_CurrentChildIndex = -1;

    struct BreadcrumbEntry {
        uintptr_t Address = 0;
        std::string Label;
    };
    std::deque<BreadcrumbEntry> m_History;
    std::vector<uintptr_t> m_ChildAddresses;

    char m_SearchBuffer[128] = {};
    std::vector<std::pair<uintptr_t, std::string>> m_SearchResults;
};

} // namespace Examples
