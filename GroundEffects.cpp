// ============================================================================
// GroundEffects — POE2Fixer SDK v6 plugin
// Marks hostile GroundEffect entities with configurable colored world-space
// circles. This build intentionally supports GroundEffect components only;
// non-GroundEffect explosions / ServerEffect hazards are ignored.
//
// Performance notes:
//   - no per-frame WorldToScreen circle projection; projected points are cached
//     during the scan step
//   - cheap wide-string path prefilter before UTF-8 conversion
//   - configurable scan time budget to avoid the POE2Fixer watchdog
//
// Integration:
//   1) Copy this file into a clone/copy of POEFixer/ExamplePlugin.
//   2) Replace ExamplePlugin.cpp with this file, or add it as the only plugin
//      entry point source in the .vcxproj.
//   3) Build Release | x64 and copy the resulting DLL to:
//      POE2Fixer/Plugins/GroundEffects/
// ============================================================================

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include "sdk/PluginSDK.h"
#include "imgui.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cfloat>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class GroundEffectsPlugin final : public PluginSDK::Plugin {
public:
    const char* GetName() const override { return "GroundEffects"; }

    void OnEnable(bool /*isGameAttached*/) override {
        LoadSettings();
        m_NextScan = std::chrono::steady_clock::now();
        if (ctx()->ImGuiContext) {
            ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));
        }
        ctx()->Log.Info("GroundEffects enabled");
    }

    void OnDisable() override {
        SaveSettings();
        m_Markers.clear();
        ctx()->Log.Info("GroundEffects disabled");
    }

    bool WantsOverlay() const override { return m_MasterEnabled; }

    void DrawSettings() override {
        if (ctx()->ImGuiContext) {
            ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));
        }

        ImGui::Checkbox("Enable plugin", &m_MasterEnabled);
        ImGui::Checkbox("Only draw while PoE2 is foreground", &m_OnlyWhenForeground);
        ImGui::Checkbox("Disable in towns and hideouts", &m_SkipTownHideout);
        ImGui::Checkbox("Only hostile ground effects (Positioned.IsFriendly == false)", &m_OnlyHostileGroundEffects);
        ImGui::Checkbox("Use path pre-filter", &m_UsePathPrefilter);
        ImGui::Checkbox("Hide while the game menu is visible", &m_HideWhenMenuVisible);
        ImGui::Checkbox("Fill ground circles", &m_FillCircles);
        ImGui::Checkbox("Show labels", &m_ShowLabels);

        ImGui::SliderInt("Scan interval (ms)", &m_ScanIntervalMs, 100, 1000);
        ImGui::SliderInt("Max scan time (ms)", &m_MaxScanTimeMs, 3, 30);
        ImGui::SliderInt("Max markers", &m_MaxMarkers, 8, 256);
        ImGui::SliderInt("Circle quality", &m_CircleSegments, 8, kMaxCircleSegments);
        ImGui::SliderFloat("Outline thickness", &m_OutlineThickness, 1.0f, 8.0f, "%.1f");
        ImGui::SliderFloat("Global Z offset", &m_ZOffset, -150.0f, 150.0f, "%.0f");

        ImGui::Separator();
        ImGui::TextUnformatted("Ground effect rules");
        ImGui::TextDisabled("Match text is searched case-insensitively in GroundEffect TypeId, visuals, AoFile, EndEffect and entity paths.");
        ImGui::TextDisabled("The radius is read from the GroundEffect component. Explosion / ServerEffect hazards are intentionally ignored.");

        if (ImGui::Button("+ Add effect")) {
            m_Rules.emplace_back();
            MarkRulesDirty();
        }
        ImGui::SameLine();
        if (ImGui::Button("Save")) SaveSettings();
        ImGui::SameLine();
        ImGui::TextDisabled("Current markers: %d", static_cast<int>(m_Markers.size()));

        if (ImGui::BeginTable("##ground_effect_rules", 5,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("On", ImGuiTableColumnFlags_WidthFixed, 40.0f);
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthStretch, 140.0f);
            ImGui::TableSetupColumn("TypeId / match text", ImGuiTableColumnFlags_WidthStretch, 240.0f);
            ImGui::TableSetupColumn("Color + opacity", ImGuiTableColumnFlags_WidthFixed, 190.0f);
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 36.0f);
            ImGui::TableHeadersRow();

            for (size_t i = 0; i < m_Rules.size();) {
                auto& r = m_Rules[i];
                ImGui::PushID(static_cast<int>(i));
                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                if (ImGui::Checkbox("##enabled", &r.Enabled)) MarkRulesDirty();

                ImGui::TableNextColumn();
                if (ImGui::InputText("##label", r.Label, sizeof(r.Label))) MarkRulesDirty();

                ImGui::TableNextColumn();
                if (ImGui::InputText("##match", r.MatchText, sizeof(r.MatchText))) MarkRulesDirty();

                ImGui::TableNextColumn();
                if (ImGui::ColorEdit4("##color", &r.Color.x,
                    ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_NoInputs)) {
                    m_ForceRescan = true;
                }

                ImGui::TableNextColumn();
                const bool remove = ImGui::SmallButton("X");
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
        DrawDiscoverySettings();
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
        if (now >= m_NextScan || m_ForceRescan) {
            m_ForceRescan = false;
            RebuildMarkerCache();
            const int interval = std::clamp(m_ScanIntervalMs, 100, 5000);
            m_NextScan = now + std::chrono::milliseconds(interval);
        }

        DrawMarkers();
    }

    void SaveSettings() override {
        namespace fs = std::filesystem;
        fs::path dir = DirectoryPath() / "config";
        std::error_code ec;
        fs::create_directories(dir, ec);

        std::ofstream file(dir / "GroundEffects.ini", std::ios::trunc);
        if (!file.is_open()) return;

        file << "MasterEnabled=" << (m_MasterEnabled ? 1 : 0) << "\n";
        file << "OnlyWhenForeground=" << (m_OnlyWhenForeground ? 1 : 0) << "\n";
        file << "SkipTownHideout=" << (m_SkipTownHideout ? 1 : 0) << "\n";
        file << "OnlyHostileGroundEffects=" << (m_OnlyHostileGroundEffects ? 1 : 0) << "\n";
        file << "UsePathPrefilter=" << (m_UsePathPrefilter ? 1 : 0) << "\n";
        file << "HideWhenMenuVisible=" << (m_HideWhenMenuVisible ? 1 : 0) << "\n";
        file << "FillCircles=" << (m_FillCircles ? 1 : 0) << "\n";
        file << "ShowLabels=" << (m_ShowLabels ? 1 : 0) << "\n";
        file << "DiscoveryEnabled=" << (m_DiscoveryEnabled ? 1 : 0) << "\n";
        file << "DiscoveryLogToFile=" << (m_DiscoveryLogToFile ? 1 : 0) << "\n";
        file << "ScanIntervalMs=" << m_ScanIntervalMs << "\n";
        file << "MaxScanTimeMs=" << m_MaxScanTimeMs << "\n";
        file << "MaxMarkers=" << m_MaxMarkers << "\n";
        file << "CircleSegments=" << m_CircleSegments << "\n";
        file << "OutlineThickness=" << m_OutlineThickness << "\n";
        file << "ZOffset=" << m_ZOffset << "\n";
        file << "EffectCount=" << m_Rules.size() << "\n";

        for (size_t i = 0; i < m_Rules.size(); ++i) {
            const auto& r = m_Rules[i];
            file << "Effect" << i << ".Enabled=" << (r.Enabled ? 1 : 0) << "\n";
            file << "Effect" << i << ".Label=" << r.Label << "\n";
            file << "Effect" << i << ".MatchText=" << r.MatchText << "\n";
            file << "Effect" << i << ".Color="
                 << r.Color.x << "," << r.Color.y << "," << r.Color.z << "," << r.Color.w << "\n";
        }
    }

private:
    static constexpr int kMaxCircleSegments = 32;
    static constexpr float kPi = 3.14159265358979323846f;
    static constexpr float kFallbackWorldRadius = 40.0f;
    static constexpr float kMaxWorldRadius = 500.0f;
    static constexpr size_t kMaxDiscoveryRows = 120;

    struct EffectRule {
        bool Enabled = true;
        char Label[96] = "";
        char MatchText[192] = "";
        ImVec4 Color = ImVec4(1.0f, 0.15f, 0.10f, 0.45f);
        std::string MatchTextLower;
    };

    struct GroundEffectData {
        bool Valid = false;
        bool PositionedValid = false;
        bool IsFriendly = false;
        std::string TypeId;
        std::string EndEffect;
        std::string BuffVisual1;
        std::string BuffVisual2;
        std::string AoFile;
        uintptr_t GroundEffectsRowAddr = 0;
        uintptr_t GroundEffectTypesRowAddr = 0;
        float Radius = 0.0f;
        float WorldX = 0.0f;
        float WorldY = 0.0f;
        float WorldZ = 0.0f;
        float BoundsX = 0.0f;
        float BoundsY = 0.0f;
        std::string Path;
        std::string TgtPath;
        std::string SearchTextLower;
    };

    struct Marker {
        uint32_t EntityId = 0;
        float WorldX = 0.0f;
        float WorldY = 0.0f;
        float WorldZ = 0.0f;
        float WorldRadius = kFallbackWorldRadius;
        ImVec4 Color = ImVec4(1, 0, 0, 0.45f);
        std::string Label;

        std::array<ImVec2, kMaxCircleSegments + 1> Points{};
        int PointCount = 0;
        ImVec2 Center = ImVec2(0.0f, 0.0f);
        ImVec2 MinPoint = ImVec2(0.0f, 0.0f);
        ImVec2 MaxPoint = ImVec2(0.0f, 0.0f);
        bool ScreenValid = false;
    };

    struct DiscoveryRow {
        uint32_t EntityId = 0;
        int EntityType = 0;
        int EntitySubtype = 0;
        bool PositionedValid = false;
        bool IsFriendly = false;
        std::string TypeId;
        float Radius = 0.0f;
        std::string EndEffect;
        std::string BuffVisual1;
        std::string BuffVisual2;
        std::string AoFile;
        std::string GroundEffectsRow;
        std::string GroundEffectTypesRow;
        std::string Path;
        std::string Fingerprint;
    };

    static std::string ToLower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return s;
    }

    static std::string WideToUtf8(const std::wstring& ws) {
        if (ws.empty()) return {};
        const int needed = ::WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()),
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

    static std::string ParseString(const std::unordered_map<std::string, std::string>& kv,
                                   const std::string& key,
                                   const std::string& fallback = {}) {
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

    static std::string FormatAddress(uintptr_t address) {
        if (address == 0) return "0x0";
        std::ostringstream ss;
        ss << "0x" << std::hex << std::uppercase << address;
        return ss.str();
    }

    static float PositiveMax(float a, float b) {
        a = std::fabs(a);
        b = std::fabs(b);
        return (a > b) ? a : b;
    }

    static bool ContainsNoCase(const std::string& lowerHaystack, const std::string& lowerNeedle) {
        return !lowerNeedle.empty() && lowerHaystack.find(lowerNeedle) != std::string::npos;
    }

    static bool LooksLikeGroundEffectPath(const std::string& lowerText) {
        return lowerText.find("ground_effect") != std::string::npos ||
               lowerText.find("groundeffects") != std::string::npos ||
               lowerText.find("visiblegroundeffect") != std::string::npos ||
               lowerText.find("visibleservergroundeffect") != std::string::npos;
    }

    static wchar_t ToLowerAscii(wchar_t c) {
        return (c >= L'A' && c <= L'Z') ? static_cast<wchar_t>(c - L'A' + L'a') : c;
    }

    static bool ContainsWideAsciiNoCase(const std::wstring& haystack, std::wstring_view needle) {
        if (needle.empty() || haystack.size() < needle.size()) return false;

        for (size_t i = 0; i <= haystack.size() - needle.size(); ++i) {
            bool match = true;
            for (size_t j = 0; j < needle.size(); ++j) {
                if (ToLowerAscii(haystack[i + j]) != ToLowerAscii(needle[j])) {
                    match = false;
                    break;
                }
            }
            if (match) return true;
        }
        return false;
    }

    static bool LooksLikeGroundEffectPath(const std::wstring& path) {
        return ContainsWideAsciiNoCase(path, L"ground_effect") ||
               ContainsWideAsciiNoCase(path, L"groundeffects") ||
               ContainsWideAsciiNoCase(path, L"visiblegroundeffect") ||
               ContainsWideAsciiNoCase(path, L"visibleservergroundeffect");
    }

    static bool IsLegacyExplosionRule(const EffectRule& r) {
        const std::string match = ToLower(std::string(r.MatchText));
        return match.find("explosion") != std::string::npos ||
               match.find("ondeath") != std::string::npos ||
               match.find("servereffect") != std::string::npos ||
               match.find("entityhazard") != std::string::npos ||
               match.find("volatile") != std::string::npos;
    }

    void LoadSettings() {
        namespace fs = std::filesystem;
        fs::path path = DirectoryPath() / "config" / "GroundEffects.ini";

        if (!fs::exists(path)) {
            AddDefaultRules();
            MarkRulesDirty();
            return;
        }

        std::ifstream file(path);
        if (!file.is_open()) {
            AddDefaultRules();
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
        m_OnlyHostileGroundEffects = ParseBool(kv, "OnlyHostileGroundEffects", m_OnlyHostileGroundEffects);
        m_UsePathPrefilter = ParseBool(kv, "UsePathPrefilter", ParseBool(kv, "UseEntityTypeFilter", m_UsePathPrefilter));
        m_HideWhenMenuVisible = ParseBool(kv, "HideWhenMenuVisible", m_HideWhenMenuVisible);
        m_FillCircles = ParseBool(kv, "FillCircles", m_FillCircles);
        m_ShowLabels = ParseBool(kv, "ShowLabels", m_ShowLabels);
        // Reset discovery on update. Old configs may have logging enabled, which can
        // push DrawUI over POE2Fixer's watchdog threshold. Re-enable manually if needed.
        m_DiscoveryEnabled = false;
        m_DiscoveryLogToFile = false;
        m_ScanIntervalMs = std::clamp(ParseInt(kv, "ScanIntervalMs", m_ScanIntervalMs), 100, 1000);
        m_MaxScanTimeMs = std::clamp(ParseInt(kv, "MaxScanTimeMs", m_MaxScanTimeMs), 3, 30);
        m_MaxMarkers = std::clamp(ParseInt(kv, "MaxMarkers", m_MaxMarkers), 8, 256);
        m_CircleSegments = std::clamp(ParseInt(kv, "CircleSegments", m_CircleSegments), 8, kMaxCircleSegments);
        m_OutlineThickness = ParseFloat(kv, "OutlineThickness", m_OutlineThickness);
        m_ZOffset = ParseFloat(kv, "ZOffset", m_ZOffset);

        m_Rules.clear();
        const int count = std::clamp(ParseInt(kv, "EffectCount", 0), 0, 256);
        for (int i = 0; i < count; ++i) {
            EffectRule r;
            const std::string prefix = "Effect" + std::to_string(i) + ".";
            r.Enabled = ParseBool(kv, (prefix + "Enabled").c_str(), true);

            const std::string legacyName = ParseString(kv, prefix + "Name");
            const std::string label = ParseString(kv, prefix + "Label", legacyName);
            const std::string match = ParseString(kv, prefix + "MatchText", legacyName.empty() ? label : legacyName);
            std::snprintf(r.Label, sizeof(r.Label), "%s", label.c_str());
            std::snprintf(r.MatchText, sizeof(r.MatchText), "%s", match.c_str());

            r.Color = ParseColor(ParseString(kv, prefix + "Color"), r.Color);
            m_Rules.push_back(r);
        }

        m_Rules.erase(std::remove_if(m_Rules.begin(), m_Rules.end(), IsLegacyExplosionRule), m_Rules.end());

        if (m_Rules.empty()) AddDefaultRules();
        MarkRulesDirty();
    }

    void AddDefaultRules() {
        AddRule("Shocked Ground", "ShockedGround", ImVec4(0.45f, 0.65f, 1.0f, 0.38f));
        AddRule("Chilled Ground", "ChilledGround", ImVec4(0.35f, 0.8f, 1.0f, 0.34f));
        AddRule("Ignited Ground", "IgnitedGround", ImVec4(1.0f, 0.22f, 0.08f, 0.40f));
        AddRule("Caustic Cloud", "CausticCloud", ImVec4(0.35f, 1.0f, 0.20f, 0.34f));
    }

    void AddRule(const char* label, const char* matchText, const ImVec4& color) {
        EffectRule r;
        r.Enabled = true;
        std::snprintf(r.Label, sizeof(r.Label), "%s", label);
        std::snprintf(r.MatchText, sizeof(r.MatchText), "%s", matchText);
        r.Color = color;
        m_Rules.push_back(r);
    }

    void MarkRulesDirty() {
        m_RulesDirty = true;
        m_ForceRescan = true;
    }

    void PrepareRulesIfNeeded() {
        if (!m_RulesDirty) return;
        for (auto& r : m_Rules) {
            r.MatchTextLower = ToLower(std::string(r.MatchText));
            while (!r.MatchTextLower.empty() && std::isspace(static_cast<unsigned char>(r.MatchTextLower.front()))) {
                r.MatchTextLower.erase(r.MatchTextLower.begin());
            }
            while (!r.MatchTextLower.empty() && std::isspace(static_cast<unsigned char>(r.MatchTextLower.back()))) {
                r.MatchTextLower.pop_back();
            }
        }
        m_RulesDirty = false;
    }

    bool AnyEnabledRule() const {
        for (const auto& r : m_Rules) {
            if (r.Enabled && !r.MatchTextLower.empty()) return true;
        }
        return false;
    }

    bool IsCandidateEntity(const PluginSDK::Entity& e) const {
        if (!e.IsValid || !e.Components.HasRender()) return false;
        if (m_UsePathPrefilter && !LooksLikeGroundEffectPath(e.Path)) return false;
        return true;
    }

    GroundEffectData ReadGroundEffectData(const PluginSDK::Entity& e, const std::string& pathUtf8) const {
        GroundEffectData d;
        d.Path = pathUtf8;
        d.TgtPath = e.TgtPath;
        d.WorldX = e.WorldX;
        d.WorldY = e.WorldY;
        d.WorldZ = e.WorldZ;

        const PluginSDK::GroundEffect ge = ctx()->Components.ReadGroundEffect(e.Address);
        if (!ge.Valid) return d;

        d.Valid = true;
        d.TypeId = ge.TypeId;
        d.EndEffect = ge.EndEffect;
        d.BuffVisual1 = ge.BuffVisual1;
        d.BuffVisual2 = ge.BuffVisual2;
        d.AoFile = ge.AoFile;
        d.GroundEffectsRowAddr = ge.GroundEffectsRowAddr;
        d.GroundEffectTypesRowAddr = ge.GroundEffectTypesRowAddr;
        d.Radius = ge.Radius;

        if (e.Components.HasPositioned()) {
            const PluginSDK::Positioned positioned = ctx()->Components.ReadPositioned(e.Components.Positioned);
            d.PositionedValid = positioned.Valid;
            d.IsFriendly = positioned.Valid && positioned.IsFriendly;
        }

        if (e.Components.HasRender()) {
            const PluginSDK::Render render = ctx()->Components.ReadRender(e.Components.Render);
            if (render.Valid) {
                d.WorldX = render.WorldX;
                d.WorldY = render.WorldY;
                d.WorldZ = render.WorldZ;
                d.BoundsX = render.ModelBoundsX;
                d.BoundsY = render.ModelBoundsY;
            }
        }

        if (d.Radius <= 0.0f) {
            d.Radius = PositiveMax(d.BoundsX, d.BoundsY);
        }
        if (d.Radius <= 0.0f) {
            d.Radius = kFallbackWorldRadius;
        }
        d.Radius = std::clamp(d.Radius, 4.0f, kMaxWorldRadius);

        std::string searchText;
        searchText.reserve(512);
        searchText += d.TypeId;
        searchText += ' ';
        searchText += d.EndEffect;
        searchText += ' ';
        searchText += d.BuffVisual1;
        searchText += ' ';
        searchText += d.BuffVisual2;
        searchText += ' ';
        searchText += d.AoFile;
        searchText += ' ';
        searchText += d.Path;
        searchText += ' ';
        searchText += d.TgtPath;
        d.SearchTextLower = ToLower(std::move(searchText));

        return d;
    }

    const EffectRule* FindMatchingRule(const GroundEffectData& data) const {
        for (const auto& r : m_Rules) {
            if (!r.Enabled || r.MatchTextLower.empty()) continue;
            if (ContainsNoCase(data.SearchTextLower, r.MatchTextLower)) return &r;
        }
        return nullptr;
    }

    DiscoveryRow BuildDiscoveryRow(const PluginSDK::Entity& e, const GroundEffectData& data) const {
        DiscoveryRow row;
        row.EntityId = e.Id;
        row.EntityType = static_cast<int>(e.EntityType);
        row.EntitySubtype = static_cast<int>(e.EntitySubtype);
        row.PositionedValid = data.PositionedValid;
        row.IsFriendly = data.IsFriendly;
        row.TypeId = data.TypeId;
        row.Radius = data.Radius;
        row.EndEffect = data.EndEffect;
        row.BuffVisual1 = data.BuffVisual1;
        row.BuffVisual2 = data.BuffVisual2;
        row.AoFile = data.AoFile;
        row.GroundEffectsRow = FormatAddress(data.GroundEffectsRowAddr);
        row.GroundEffectTypesRow = FormatAddress(data.GroundEffectTypesRowAddr);
        row.Path = data.Path;

        std::ostringstream fp;
        fp << "EntityId=" << row.EntityId
           << " | Type=" << row.EntityType
           << " | Subtype=" << row.EntitySubtype
           << " | TypeId=" << row.TypeId
           << " | Radius=" << row.Radius
           << " | IsFriendly=" << (row.PositionedValid ? (row.IsFriendly ? "true" : "false") : "unknown")
           << " | EndEffect=" << row.EndEffect
           << " | BuffVisual1=" << row.BuffVisual1
           << " | BuffVisual2=" << row.BuffVisual2
           << " | AoFile=" << row.AoFile
           << " | GroundEffectsRow=" << row.GroundEffectsRow
           << " | GroundEffectTypesRow=" << row.GroundEffectTypesRow
           << " | Path=" << row.Path;
        row.Fingerprint = fp.str();
        return row;
    }

    void AddDiscoveryRow(DiscoveryRow row) {
        for (auto& existing : m_DiscoveryRows) {
            if (existing.EntityId == row.EntityId) {
                existing = std::move(row);
                return;
            }
        }

        if (m_DiscoveryRows.size() >= kMaxDiscoveryRows) {
            m_DiscoveryRows.erase(m_DiscoveryRows.begin());
        }
        m_DiscoveryRows.push_back(std::move(row));
    }

    void AppendDiscoveryLog(const DiscoveryRow& row) const {
        if (!m_DiscoveryLogToFile) return;

        namespace fs = std::filesystem;
        fs::path dir = DirectoryPath() / "config";
        std::error_code ec;
        fs::create_directories(dir, ec);
        std::ofstream file(dir / "GroundEffectsDiscovery.log", std::ios::app);
        if (!file.is_open()) return;
        file << row.Fingerprint << "\n";
    }

    void RebuildMarkerCache() {
        PrepareRulesIfNeeded();
        m_Markers.clear();

        PluginSDK::Snapshot snap = ctx()->Game.GetSnapshot();
        if (!snap.IsAttached || snap.State != PluginSDK::GameState::InGame) return;
        if (m_SkipTownHideout && (snap.IsTown || snap.IsHideout)) return;

        if (snap.AreaChangeCounter != m_LastAreaChangeCounter) {
            m_LastAreaChangeCounter = snap.AreaChangeCounter;
            m_Markers.clear();
            m_DiscoveryRows.clear();
        }

        const bool hasEnabledRules = AnyEnabledRule();
        if (!hasEnabledRules && !m_DiscoveryEnabled) return;

        const int maxMarkers = std::clamp(m_MaxMarkers, 1, 2048);
        const int reserveCount = (maxMarkers < 128) ? maxMarkers : 128;
        m_Markers.reserve(static_cast<size_t>(reserveCount));

        const auto scanStart = std::chrono::steady_clock::now();
        const int scanBudgetMs = std::clamp(m_MaxScanTimeMs, 3, 30);
        int checkedEntities = 0;

        for (const auto& e : snap.Entities) {
            if ((++checkedEntities & 0x3F) == 0) {
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - scanStart).count();
                if (elapsed >= scanBudgetMs) break;
            }

            if (!IsCandidateEntity(e)) continue;

            const std::string pathUtf8 = WideToUtf8(e.Path);
            GroundEffectData data = ReadGroundEffectData(e, pathUtf8);
            if (!data.Valid) continue;

            if (m_OnlyHostileGroundEffects && (!data.PositionedValid || data.IsFriendly)) {
                continue;
            }

            if (m_DiscoveryEnabled) {
                DiscoveryRow row = BuildDiscoveryRow(e, data);
                AddDiscoveryRow(row);
                AppendDiscoveryLog(row);
            }

            const EffectRule* rule = hasEnabledRules ? FindMatchingRule(data) : nullptr;
            if (rule && static_cast<int>(m_Markers.size()) < maxMarkers) {
                Marker m;
                m.EntityId = e.Id;
                m.WorldX = data.WorldX;
                m.WorldY = data.WorldY;
                m.WorldZ = data.WorldZ + m_ZOffset;
                m.WorldRadius = data.Radius;
                m.Color = rule->Color;
                m.Label = rule->Label[0] ? rule->Label : data.TypeId;
                if (!ProjectMarker(m)) continue;
                m_Markers.push_back(std::move(m));

                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - scanStart).count();
                if (elapsed >= scanBudgetMs) break;
            }

            if (!m_DiscoveryEnabled && static_cast<int>(m_Markers.size()) >= maxMarkers) {
                break;
            }
        }
    }

    bool ProjectMarker(Marker& marker) const {
        float centerX = 0.0f, centerY = 0.0f;
        if (!ctx()->Render.WorldToScreen(marker.WorldX, marker.WorldY, marker.WorldZ, centerX, centerY)) return false;
        marker.Center = ImVec2(centerX, centerY);

        const int segments = std::clamp(m_CircleSegments, 8, kMaxCircleSegments);
        marker.PointCount = 0;
        marker.MinPoint = ImVec2(FLT_MAX, FLT_MAX);
        marker.MaxPoint = ImVec2(-FLT_MAX, -FLT_MAX);

        for (int i = 0; i < segments; ++i) {
            const float angle = (2.0f * kPi * static_cast<float>(i)) / static_cast<float>(segments);
            const float wx = marker.WorldX + std::cos(angle) * marker.WorldRadius;
            const float wy = marker.WorldY + std::sin(angle) * marker.WorldRadius;

            float sx = 0.0f, sy = 0.0f;
            if (!ctx()->Render.WorldToScreen(wx, wy, marker.WorldZ, sx, sy)) continue;

            marker.Points[static_cast<size_t>(marker.PointCount)] = ImVec2(sx, sy);
            if (sx < marker.MinPoint.x) marker.MinPoint.x = sx;
            if (sy < marker.MinPoint.y) marker.MinPoint.y = sy;
            if (sx > marker.MaxPoint.x) marker.MaxPoint.x = sx;
            if (sy > marker.MaxPoint.y) marker.MaxPoint.y = sy;
            ++marker.PointCount;
        }

        if (marker.PointCount < 3) return false;
        marker.Points[static_cast<size_t>(marker.PointCount)] = marker.Points[0];
        marker.ScreenValid = true;
        return true;
    }

    void DrawMarkers() const {
        if (m_Markers.empty()) return;

        ImDrawList* draw = ImGui::GetForegroundDrawList();
        if (!draw) return;

        PluginSDK::ScreenSize screen = ctx()->Game.GetScreenSize();
        const float width = screen.Width > 0 ? screen.Width : 100000.0f;
        const float height = screen.Height > 0 ? screen.Height : 100000.0f;
        const float cullPadding = 80.0f;

        for (const auto& marker : m_Markers) {
            if (!marker.ScreenValid || marker.PointCount < 3) continue;

            if (marker.MaxPoint.x < -cullPadding || marker.MaxPoint.y < -cullPadding ||
                marker.MinPoint.x > width + cullPadding || marker.MinPoint.y > height + cullPadding) {
                continue;
            }

            const ImU32 fill = ImGui::ColorConvertFloat4ToU32(marker.Color);

            ImVec4 outlineColor = marker.Color;
            outlineColor.w = std::clamp(outlineColor.w + 0.30f, 0.0f, 1.0f);
            const ImU32 outline = ImGui::ColorConvertFloat4ToU32(outlineColor);

            if (m_FillCircles) {
                draw->AddConvexPolyFilled(marker.Points.data(), marker.PointCount, fill);
            }

            const float thickness = std::clamp(m_OutlineThickness, 1.0f, 8.0f);
            draw->AddPolyline(marker.Points.data(), marker.PointCount + 1, outline, 0, thickness);

            if (m_ShowLabels && !marker.Label.empty()) {
                draw->AddText(ImVec2(marker.MaxPoint.x + 4.0f, marker.Center.y - 8.0f), outline, marker.Label.c_str());
            }
        }
    }

    void DrawDiscoverySettings() {
        ImGui::TextUnformatted("Discovery Mode");
        ImGui::TextDisabled("Lists GroundEffect components read through Components.ReadGroundEffect(entity address). ServerEffect / explosion hazards are ignored.");
        ImGui::TextDisabled("Copy the TypeId, for example ChilledGround or ShockedGround, into a rule's match text.");

        if (ImGui::Checkbox("Enable discovery mode", &m_DiscoveryEnabled)) {
            m_ForceRescan = true;
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Append discovery log", &m_DiscoveryLogToFile)) SaveSettings();
        ImGui::SameLine();
        if (ImGui::Button("Refresh now")) m_ForceRescan = true;
        ImGui::SameLine();
        if (ImGui::Button("Clear rows")) m_DiscoveryRows.clear();

        ImGui::TextDisabled("Rows: %d | Log: Plugins/GroundEffects/config/GroundEffectsDiscovery.log", static_cast<int>(m_DiscoveryRows.size()));

        if (!m_DiscoveryRows.empty() && ImGui::BeginTable("##ground_effect_discovery", 10,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
            ImVec2(0.0f, 300.0f))) {
            ImGui::TableSetupColumn("Copy", ImGuiTableColumnFlags_WidthFixed, 44.0f);
            ImGui::TableSetupColumn("Id", ImGuiTableColumnFlags_WidthFixed, 54.0f);
            ImGui::TableSetupColumn("TypeId", ImGuiTableColumnFlags_WidthFixed, 130.0f);
            ImGui::TableSetupColumn("Friendly", ImGuiTableColumnFlags_WidthFixed, 66.0f);
            ImGui::TableSetupColumn("Radius", ImGuiTableColumnFlags_WidthFixed, 64.0f);
            ImGui::TableSetupColumn("End", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableSetupColumn("BuffVisual1", ImGuiTableColumnFlags_WidthStretch, 150.0f);
            ImGui::TableSetupColumn("BuffVisual2", ImGuiTableColumnFlags_WidthStretch, 150.0f);
            ImGui::TableSetupColumn("AoFile", ImGuiTableColumnFlags_WidthStretch, 220.0f);
            ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch, 220.0f);
            ImGui::TableHeadersRow();

            for (size_t i = 0; i < m_DiscoveryRows.size(); ++i) {
                const auto& row = m_DiscoveryRows[i];
                ImGui::PushID(static_cast<int>(i));
                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                if (ImGui::SmallButton("Copy")) {
                    ImGui::SetClipboardText(row.TypeId.empty() ? row.Fingerprint.c_str() : row.TypeId.c_str());
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Copy TypeId");

                ImGui::TableNextColumn();
                ImGui::Text("%u", row.EntityId);

                ImGui::TableNextColumn();
                ImGui::TextWrapped("%s", row.TypeId.c_str());

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(row.PositionedValid ? (row.IsFriendly ? "true" : "false") : "unknown");

                ImGui::TableNextColumn();
                ImGui::Text("%.1f", row.Radius);

                ImGui::TableNextColumn();
                ImGui::TextWrapped("%s", row.EndEffect.c_str());

                ImGui::TableNextColumn();
                ImGui::TextWrapped("%s", row.BuffVisual1.c_str());

                ImGui::TableNextColumn();
                ImGui::TextWrapped("%s", row.BuffVisual2.c_str());

                ImGui::TableNextColumn();
                ImGui::TextWrapped("%s", row.AoFile.c_str());

                ImGui::TableNextColumn();
                ImGui::TextWrapped("%s", row.Path.c_str());

                ImGui::PopID();
            }

            ImGui::EndTable();
        }
    }

private:
    bool m_MasterEnabled = true;
    bool m_OnlyWhenForeground = true;
    bool m_SkipTownHideout = true;
    bool m_OnlyHostileGroundEffects = true;
    bool m_UsePathPrefilter = true;
    bool m_HideWhenMenuVisible = true;
    bool m_FillCircles = true;
    bool m_ShowLabels = false;
    bool m_DiscoveryEnabled = false;
    bool m_DiscoveryLogToFile = false;
    bool m_ForceRescan = false;

    int m_ScanIntervalMs = 150;
    int m_MaxScanTimeMs = 8;
    int m_MaxMarkers = 80;
    int m_CircleSegments = 16;
    float m_OutlineThickness = 2.0f;
    float m_ZOffset = 0.0f;

    std::vector<EffectRule> m_Rules;
    bool m_RulesDirty = true;
    uint64_t m_LastAreaChangeCounter = 0;
    std::vector<Marker> m_Markers;
    std::vector<DiscoveryRow> m_DiscoveryRows;
    std::chrono::steady_clock::time_point m_NextScan = std::chrono::steady_clock::now();
};

extern "C" PLUGIN_API PluginSDK::Plugin* CreatePlugin() {
    return new GroundEffectsPlugin();
}

extern "C" PLUGIN_API void DestroyPlugin(PluginSDK::Plugin* plugin) {
    delete plugin;
}
