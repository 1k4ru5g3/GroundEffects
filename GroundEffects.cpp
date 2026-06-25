// ============================================================================
// GroundEffectMarker — POE2Fixer SDK v6 plugin
// Marks user-defined ground-effect entities with colored circles.
//
// Integration:
//   1) Copy this file into a clone/copy of POEFixer/ExamplePlugin.
//   2) Replace ExamplePlugin.cpp with this file, or add it as the only plugin
//      entry point source in the .vcxproj.
//   3) Build Release | x64 and copy the resulting DLL to:
//      POE2Fixer/Plugins/GroundEffectMarker/
// ============================================================================

#include "sdk/PluginSDK.h"
#include "imgui.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

class GroundEffectMarkerPlugin final : public PluginSDK::Plugin {
public:
    const char* GetName() const override { return "Ground Effect Marker"; }

    void OnEnable(bool /*isGameAttached*/) override {
        LoadSettings();
        m_NextScan = std::chrono::steady_clock::now();
        if (ctx()->ImGuiContext) {
            ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));
        }
        ctx()->Log.Info("GroundEffectMarker enabled");
    }

    void OnDisable() override {
        SaveSettings();
        m_Markers.clear();
        ctx()->Log.Info("GroundEffectMarker disabled");
    }

    bool WantsOverlay() const override { return m_MasterEnabled; }

    void DrawSettings() override {
        if (ctx()->ImGuiContext) {
            ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));
        }

        ImGui::Checkbox("Plugin aktiv", &m_MasterEnabled);
        ImGui::Checkbox("Nur zeichnen, wenn PoE2 im Vordergrund ist", &m_OnlyWhenForeground);
        ImGui::Checkbox("In Stadt/Hideout deaktivieren", &m_SkipTownHideout);
        ImGui::Checkbox("Entity-Type-Filter nutzen (ressourcenschonender)", &m_UseEntityTypeFilter);
        ImGui::Checkbox("Bei geoeffnetem Spielmenue ausblenden", &m_HideWhenMenuVisible);
        ImGui::Checkbox("Kreise fuellen", &m_FillCircles);
        ImGui::Checkbox("Labels anzeigen", &m_ShowLabels);

        ImGui::SliderInt("Scan-Intervall (ms)", &m_ScanIntervalMs, 33, 500);
        ImGui::SliderInt("Max. Marker", &m_MaxMarkers, 16, 512);
        ImGui::SliderFloat("Globaler Z-Offset", &m_ZOffset, -150.0f, 150.0f, "%.0f");

        ImGui::Separator();
        ImGui::TextUnformatted("Effekte");
        ImGui::TextDisabled("Name ist ein case-insensitive Substring aus Entity Path oder TgtPath.");

        if (ImGui::Button("+ Effekt hinzufuegen")) {
            m_Rules.emplace_back();
            MarkRulesDirty();
        }
        ImGui::SameLine();
        if (ImGui::Button("Speichern")) {
            SaveSettings();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Aktuelle Treffer: %d", static_cast<int>(m_Markers.size()));

        if (ImGui::BeginTable("##ground_effect_rules", 5,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("An", ImGuiTableColumnFlags_WidthFixed, 40.0f);
            ImGui::TableSetupColumn("Name / Substring", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Farbe + Deckkraft", ImGuiTableColumnFlags_WidthFixed, 190.0f);
            ImGui::TableSetupColumn("Radius", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 36.0f);
            ImGui::TableHeadersRow();

            for (size_t i = 0; i < m_Rules.size();) {
                auto& r = m_Rules[i];
                ImGui::PushID(static_cast<int>(i));
                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                if (ImGui::Checkbox("##enabled", &r.Enabled)) MarkRulesDirty();

                ImGui::TableNextColumn();
                if (ImGui::InputText("##name", r.Name, sizeof(r.Name))) MarkRulesDirty();

                ImGui::TableNextColumn();
                if (ImGui::ColorEdit4("##color", &r.Color.x,
                    ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_NoInputs)) {
                    // Color changes do not require rebuilding the lower-case match cache,
                    // but we rescan soon so cached markers pick up the new color.
                    m_NextScan = std::chrono::steady_clock::now();
                }

                ImGui::TableNextColumn();
                if (ImGui::SliderFloat("##radius", &r.Radius, 6.0f, 220.0f, "%.0f")) {
                    m_NextScan = std::chrono::steady_clock::now();
                }

                ImGui::TableNextColumn();
                bool remove = ImGui::SmallButton("X");
                ImGui::PopID();

                if (remove) {
                    m_Rules.erase(m_Rules.begin() + static_cast<std::ptrdiff_t>(i));
                    MarkRulesDirty();
                    continue;
                }
                ++i;
            }

            ImGui::EndTable();
        }

        ImGui::Separator();
        ImGui::TextDisabled("Tipp: Nutze den Entity Explorer aus dem ExamplePlugin, um die passenden Path/TgtPath-Namen der GroundEffects zu finden.");
    }

    void DrawUI() override {
        if (!m_MasterEnabled) return;
        if (ctx()->ImGuiContext) {
            ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));
        }

        if (!ctx()->Game.IsAttached() || !ctx()->Game.IsInGame()) {
            m_Markers.clear();
            return;
        }
        if (m_OnlyWhenForeground && !ctx()->Game.IsForeground()) return;
        if (m_HideWhenMenuVisible && ctx()->Game.IsMenuVisible()) return;

        const auto now = std::chrono::steady_clock::now();
        if (now >= m_NextScan) {
            RebuildMarkerCache();
            const int interval = std::clamp(m_ScanIntervalMs, 33, 5000);
            m_NextScan = now + std::chrono::milliseconds(interval);
        }

        DrawMarkers();
    }

    void SaveSettings() override {
        namespace fs = std::filesystem;
        fs::path dir = DirectoryPath() / "config";
        std::error_code ec;
        fs::create_directories(dir, ec);

        std::ofstream file(dir / "GroundEffectMarker.ini", std::ios::trunc);
        if (!file.is_open()) return;

        file << "MasterEnabled=" << (m_MasterEnabled ? 1 : 0) << "\n";
        file << "OnlyWhenForeground=" << (m_OnlyWhenForeground ? 1 : 0) << "\n";
        file << "SkipTownHideout=" << (m_SkipTownHideout ? 1 : 0) << "\n";
        file << "UseEntityTypeFilter=" << (m_UseEntityTypeFilter ? 1 : 0) << "\n";
        file << "HideWhenMenuVisible=" << (m_HideWhenMenuVisible ? 1 : 0) << "\n";
        file << "FillCircles=" << (m_FillCircles ? 1 : 0) << "\n";
        file << "ShowLabels=" << (m_ShowLabels ? 1 : 0) << "\n";
        file << "ScanIntervalMs=" << m_ScanIntervalMs << "\n";
        file << "MaxMarkers=" << m_MaxMarkers << "\n";
        file << "ZOffset=" << m_ZOffset << "\n";
        file << "EffectCount=" << m_Rules.size() << "\n";

        for (size_t i = 0; i < m_Rules.size(); ++i) {
            const auto& r = m_Rules[i];
            file << "Effect" << i << ".Enabled=" << (r.Enabled ? 1 : 0) << "\n";
            file << "Effect" << i << ".Name=" << r.Name << "\n";
            file << "Effect" << i << ".Radius=" << r.Radius << "\n";
            file << "Effect" << i << ".Color="
                 << r.Color.x << "," << r.Color.y << "," << r.Color.z << "," << r.Color.w << "\n";
        }
    }

private:
    struct EffectRule {
        bool Enabled = true;
        char Name[160] = "";
        ImVec4 Color = ImVec4(1.0f, 0.15f, 0.10f, 0.45f);
        float Radius = 42.0f;
        std::string NameLower;
    };

    struct Marker {
        uint32_t EntityId = 0;
        float WorldX = 0.0f;
        float WorldY = 0.0f;
        float WorldZ = 0.0f;
        float Radius = 42.0f;
        ImVec4 Color = ImVec4(1, 0, 0, 0.45f);
        std::string Label;
    };

    static std::string ToLower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return s;
    }

    static std::string WideToUtf8(const std::wstring& ws) {
        if (ws.empty()) return {};
        int needed = ::WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()),
                                           nullptr, 0, nullptr, nullptr);
        if (needed <= 0) return {};
        std::string out(static_cast<size_t>(needed), '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()),
                              out.data(), needed, nullptr, nullptr);
        return out;
    }

    static bool ParseBool(const std::unordered_map<std::string, std::string>& kv, const char* key, bool fallback) {
        auto it = kv.find(key);
        if (it == kv.end()) return fallback;
        return it->second == "1" || it->second == "true" || it->second == "True";
    }

    static int ParseInt(const std::unordered_map<std::string, std::string>& kv, const char* key, int fallback) {
        auto it = kv.find(key);
        if (it == kv.end()) return fallback;
        try { return std::stoi(it->second); } catch (...) { return fallback; }
    }

    static float ParseFloat(const std::unordered_map<std::string, std::string>& kv, const char* key, float fallback) {
        auto it = kv.find(key);
        if (it == kv.end()) return fallback;
        try { return std::stof(it->second); } catch (...) { return fallback; }
    }

    static std::string ParseString(const std::unordered_map<std::string, std::string>& kv, const std::string& key, const std::string& fallback = {}) {
        auto it = kv.find(key);
        return it == kv.end() ? fallback : it->second;
    }

    static ImVec4 ParseColor(const std::string& s, ImVec4 fallback) {
        float r = fallback.x, g = fallback.y, b = fallback.z, a = fallback.w;
        if (std::sscanf(s.c_str(), "%f,%f,%f,%f", &r, &g, &b, &a) == 4) {
            return ImVec4(std::clamp(r, 0.0f, 1.0f), std::clamp(g, 0.0f, 1.0f),
                          std::clamp(b, 0.0f, 1.0f), std::clamp(a, 0.0f, 1.0f));
        }
        return fallback;
    }

    void LoadSettings() {
        namespace fs = std::filesystem;
        fs::path path = DirectoryPath() / "config" / "GroundEffectMarker.ini";

        if (!fs::exists(path)) {
            AddDefaultRule();
            MarkRulesDirty();
            return;
        }

        std::ifstream file(path);
        if (!file.is_open()) {
            AddDefaultRule();
            MarkRulesDirty();
            return;
        }

        std::unordered_map<std::string, std::string> kv;
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            const size_t eq = line.find('=');
            if (eq == std::string::npos) continue;
            kv[line.substr(0, eq)] = line.substr(eq + 1);
        }

        m_MasterEnabled = ParseBool(kv, "MasterEnabled", m_MasterEnabled);
        m_OnlyWhenForeground = ParseBool(kv, "OnlyWhenForeground", m_OnlyWhenForeground);
        m_SkipTownHideout = ParseBool(kv, "SkipTownHideout", m_SkipTownHideout);
        m_UseEntityTypeFilter = ParseBool(kv, "UseEntityTypeFilter", m_UseEntityTypeFilter);
        m_HideWhenMenuVisible = ParseBool(kv, "HideWhenMenuVisible", m_HideWhenMenuVisible);
        m_FillCircles = ParseBool(kv, "FillCircles", m_FillCircles);
        m_ShowLabels = ParseBool(kv, "ShowLabels", m_ShowLabels);
        m_ScanIntervalMs = ParseInt(kv, "ScanIntervalMs", m_ScanIntervalMs);
        m_MaxMarkers = ParseInt(kv, "MaxMarkers", m_MaxMarkers);
        m_ZOffset = ParseFloat(kv, "ZOffset", m_ZOffset);

        m_Rules.clear();
        const int count = std::clamp(ParseInt(kv, "EffectCount", 0), 0, 256);
        for (int i = 0; i < count; ++i) {
            EffectRule r;
            const std::string prefix = "Effect" + std::to_string(i) + ".";
            r.Enabled = ParseBool(kv, (prefix + "Enabled").c_str(), true);

            const std::string name = ParseString(kv, prefix + "Name");
            std::snprintf(r.Name, sizeof(r.Name), "%s", name.c_str());

            r.Radius = ParseFloat(kv, (prefix + "Radius").c_str(), r.Radius);
            r.Color = ParseColor(ParseString(kv, prefix + "Color"), r.Color);
            m_Rules.push_back(r);
        }

        if (m_Rules.empty()) AddDefaultRule();
        MarkRulesDirty();
    }

    void AddDefaultRule() {
        EffectRule r;
        r.Enabled = false;
        std::snprintf(r.Name, sizeof(r.Name), "%s", "metadata/effects/");
        r.Color = ImVec4(1.0f, 0.15f, 0.10f, 0.45f);
        r.Radius = 42.0f;
        m_Rules.push_back(r);
    }

    void MarkRulesDirty() {
        m_RulesDirty = true;
        m_NextScan = std::chrono::steady_clock::now();
    }

    void PrepareRulesIfNeeded() {
        if (!m_RulesDirty) return;
        for (auto& r : m_Rules) {
            r.NameLower = ToLower(std::string(r.Name));
            // Trim lightweight whitespace around the search term.
            while (!r.NameLower.empty() && std::isspace(static_cast<unsigned char>(r.NameLower.front()))) r.NameLower.erase(r.NameLower.begin());
            while (!r.NameLower.empty() && std::isspace(static_cast<unsigned char>(r.NameLower.back()))) r.NameLower.pop_back();
        }
        m_RulesDirty = false;
    }

    bool AnyEnabledRule() const {
        for (const auto& r : m_Rules) {
            if (r.Enabled && !r.NameLower.empty()) return true;
        }
        return false;
    }

    bool IsCandidateEntity(const PluginSDK::Entity& e) const {
        if (!e.IsValid || !e.Components.HasRender()) return false;
        if (!m_UseEntityTypeFilter) return true;

        using T = PluginSDK::EntityType;
        switch (e.EntityType) {
            case T::Player:
            case T::Monster:
            case T::NPC:
            case T::Chest:
            case T::Shrine:
            case T::Item:
            case T::AreaTransition:
            case T::ExpeditionMarker:
            case T::ExpeditionRemnant:
                return false;
            default:
                return true;
        }
    }

    const EffectRule* FindMatchingRule(const PluginSDK::Entity& e) const {
        std::string haystack = WideToUtf8(e.Path);
        if (!e.TgtPath.empty()) {
            haystack += ' ';
            haystack += e.TgtPath;
        }
        haystack = ToLower(std::move(haystack));

        for (const auto& r : m_Rules) {
            if (!r.Enabled || r.NameLower.empty()) continue;
            if (haystack.find(r.NameLower) != std::string::npos) return &r;
        }
        return nullptr;
    }

    void RebuildMarkerCache() {
        PrepareRulesIfNeeded();
        m_Markers.clear();

        if (!AnyEnabledRule()) return;

        PluginSDK::Snapshot snap = ctx()->Game.GetSnapshot();
        if (!snap.IsAttached || snap.State != PluginSDK::GameState::InGame) return;
        if (m_SkipTownHideout && (snap.IsTown || snap.IsHideout)) return;

        if (snap.AreaChangeCounter != m_LastAreaChangeCounter) {
            m_LastAreaChangeCounter = snap.AreaChangeCounter;
            m_Markers.clear();
        }

        const int maxMarkers = std::clamp(m_MaxMarkers, 1, 2048);
        m_Markers.reserve(static_cast<size_t>(std::min(maxMarkers, 128)));

        for (const auto& e : snap.Entities) {
            if (static_cast<int>(m_Markers.size()) >= maxMarkers) break;
            if (!IsCandidateEntity(e)) continue;

            const EffectRule* rule = FindMatchingRule(e);
            if (!rule) continue;

            Marker m;
            m.EntityId = e.Id;
            m.WorldX = e.WorldX;
            m.WorldY = e.WorldY;
            m.WorldZ = e.WorldZ + m_ZOffset;
            m.Radius = std::clamp(rule->Radius, 4.0f, 400.0f);
            m.Color = rule->Color;
            m.Label = rule->Name;
            m_Markers.push_back(std::move(m));
        }
    }

    void DrawMarkers() const {
        if (m_Markers.empty()) return;

        ImDrawList* draw = ImGui::GetForegroundDrawList();
        if (!draw) return;

        PluginSDK::ScreenSize screen = ctx()->Game.GetScreenSize();
        const float width = screen.Width > 0 ? screen.Width : 100000.0f;
        const float height = screen.Height > 0 ? screen.Height : 100000.0f;

        for (const auto& m : m_Markers) {
            float sx = 0.0f, sy = 0.0f;
            if (!ctx()->Render.WorldToScreen(m.WorldX, m.WorldY, m.WorldZ, sx, sy)) continue;
            if (sx < -m.Radius || sy < -m.Radius || sx > width + m.Radius || sy > height + m.Radius) continue;

            const ImVec2 center(sx, sy);
            const ImU32 fill = ImGui::ColorConvertFloat4ToU32(m.Color);

            ImVec4 outlineColor = m.Color;
            outlineColor.w = std::clamp(outlineColor.w + 0.30f, 0.0f, 1.0f);
            const ImU32 outline = ImGui::ColorConvertFloat4ToU32(outlineColor);

            if (m_FillCircles) {
                draw->AddCircleFilled(center, m.Radius, fill, 48);
            }
            draw->AddCircle(center, m.Radius, outline, 48, 2.0f);

            if (m_ShowLabels && !m.Label.empty()) {
                draw->AddText(ImVec2(center.x + m.Radius + 4.0f, center.y - 8.0f), outline, m.Label.c_str());
            }
        }
    }

private:
    bool m_MasterEnabled = true;
    bool m_OnlyWhenForeground = true;
    bool m_SkipTownHideout = true;
    bool m_UseEntityTypeFilter = true;
    bool m_HideWhenMenuVisible = true;
    bool m_FillCircles = true;
    bool m_ShowLabels = false;

    int m_ScanIntervalMs = 100;
    int m_MaxMarkers = 128;
    float m_ZOffset = 0.0f;

    std::vector<EffectRule> m_Rules;
    bool m_RulesDirty = true;
    uint64_t m_LastAreaChangeCounter = 0;
    std::vector<Marker> m_Markers;
    std::chrono::steady_clock::time_point m_NextScan = std::chrono::steady_clock::now();
};

extern "C" PLUGIN_API PluginSDK::Plugin* CreatePlugin() {
    return new GroundEffectMarkerPlugin();
}

extern "C" PLUGIN_API void DestroyPlugin(PluginSDK::Plugin* plugin) {
    delete plugin;
}
