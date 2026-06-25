#pragma once
// ============================================================================
// ExampleEntities.h — v6 SDK
// ============================================================================
// Walks the snapshot's entity list with filtering. For each entity, lets the
// user toggle a "watch" so the host can keep its component data hot, then
// reads component data through ctx->Components.ReadXxx().
// ============================================================================

#include "sdk/PluginSDK.h"
#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <limits>
#include <sstream>

namespace Examples {

// --- Per-component renderers (v6 PluginSDK:: types) -------------------------

inline void DrawAddressRow(const char* label, uintptr_t addr) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn(); ImGui::Text("%s", label);
    ImGui::TableNextColumn();
    char buf[20]; snprintf(buf, sizeof(buf), "0x%llX", static_cast<unsigned long long>(addr));
    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.6f, 1.0f), "%s", buf);
    if (ImGui::IsItemHovered() && ImGui::IsItemClicked()) ImGui::SetClipboardText(buf);
}

inline void DrawVital(const char* label, const PluginSDK::Vital& v) {
    if (ImGui::TreeNode(label)) {
        if (ImGui::BeginTable("VitalT", 2,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Regeneration");
            ImGui::TableNextColumn(); ImGui::Text("%.4f", v.Regeneration);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Total");
            ImGui::TableNextColumn(); ImGui::Text("%d", v.Total);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("ReservedFlat");
            ImGui::TableNextColumn(); ImGui::Text("%d", v.ReservedFlat);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Current");
            ImGui::TableNextColumn(); ImGui::Text("%d", v.Current);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Reserved(%%)");
            ImGui::TableNextColumn(); ImGui::Text("%d", v.ReservedPercent);

            if (v.Total > 0) {
                float pct = static_cast<float>(v.Current) / v.Total * 100.0f;
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Current(%%)");
                ImGui::TableNextColumn(); ImGui::Text("%.1f%%", pct);
            }
            ImGui::EndTable();
        }
        ImGui::TreePop();
    }
}

inline void DrawLifeComp(const PluginSDK::Life& l) {
    if (ImGui::BeginTable("LifeT", 2,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        DrawAddressRow("Address", l.Address);
        DrawAddressRow("Owner Address", l.OwnerAddress);
        ImGui::EndTable();
    }
    DrawVital("Health", l.Health);
    DrawVital("Energy Shield", l.EnergyShield);
    DrawVital("Mana", l.Mana);
}

inline void DrawRenderComp(const PluginSDK::Render& r) {
    if (ImGui::BeginTable("RenderT", 2,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        DrawAddressRow("Address", r.Address);
        DrawAddressRow("Owner Address", r.OwnerAddress);
        ImGui::EndTable();
    }
    ImGui::Text("World Position: {%.2f, %.2f, %.2f}", r.WorldX, r.WorldY, r.WorldZ);
    ImGui::Text("Terrain Height: %.4f", r.TerrainHeight);
    ImGui::Text("Model Bounds: {%.2f, %.2f, %.2f}", r.ModelBoundsX, r.ModelBoundsY, r.ModelBoundsZ);
}

inline void DrawPositionedComp(const PluginSDK::Positioned& p) {
    if (ImGui::BeginTable("PosT", 2,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        DrawAddressRow("Address", p.Address);
        DrawAddressRow("Owner Address", p.OwnerAddress);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("Reaction");
        ImGui::TableNextColumn(); ImGui::Text("0x%02X", p.Reaction);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("IsFriendly");
        ImGui::TableNextColumn();
        ImGui::TextColored(p.IsFriendly ? ImVec4(0.3f,1,0.3f,1) : ImVec4(1,0.3f,0.3f,1),
                           p.IsFriendly ? "true" : "false");
        ImGui::EndTable();
    }
}

inline void DrawTargetableComp(const PluginSDK::Targetable& t) {
    if (ImGui::BeginTable("TgtT", 2,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        auto boolRow = [](const char* name, bool val) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("%s", name);
            ImGui::TableNextColumn();
            ImGui::TextColored(val ? ImVec4(0.3f,1,0.3f,1) : ImVec4(1,0.3f,0.3f,1),
                               val ? "true" : "false");
        };
        boolRow("IsTargetable", t.IsTargetable);
        boolRow("IsHighlightable", t.IsHighlightable);
        boolRow("IsTargetedByPlayer", t.IsTargetedByPlayer);
        boolRow("HiddenFromPlayer", t.HiddenFromPlayer);
        boolRow("MeetsQuestState", t.MeetsQuestState);
        boolRow("MeetsItemRequirements", t.MeetsItemRequirements);
        ImGui::EndTable();
    }
}

inline void DrawAnimatedComp(const PluginSDK::Animated& a) {
    if (ImGui::BeginTable("AnimT", 2,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("Path");
        ImGui::TableNextColumn(); ImGui::TextWrapped("%s", a.Path.c_str());
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("Id");
        ImGui::TableNextColumn(); ImGui::Text("%u", a.Id);
        ImGui::EndTable();
    }
}

inline void DrawActorComp(const PluginSDK::Actor& a,
                          const std::vector<PluginSDK::ActiveSkill>& skills,
                          uint32_t entityId) {
    if (ImGui::BeginTable("ActorT", 2,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("Animation");
        ImGui::TableNextColumn(); ImGui::Text("%s", a.AnimationName.c_str());
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("AnimationId");
        ImGui::TableNextColumn(); ImGui::Text("%d", a.AnimationId);
        ImGui::EndTable();
    }
    char treeId[32];
    snprintf(treeId, sizeof(treeId), "Skills (%zu)##as%u", skills.size(), entityId);
    if (!skills.empty() && ImGui::TreeNode(treeId)) {
        for (const auto& skill : skills) {
            if (ImGui::TreeNode(skill.Name.c_str())) {
                if (ImGui::BeginTable("SkillT", 2,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                    // Existing fields
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Use Stage");
                    ImGui::TableNextColumn(); ImGui::Text("%d", skill.UseStage);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Cast Type");
                    ImGui::TableNextColumn(); ImGui::Text("%d", skill.CastType);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Total Uses");
                    ImGui::TableNextColumn(); ImGui::Text("%d", skill.TotalUses);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Cooldown Time (ms)");
                    ImGui::TableNextColumn(); ImGui::Text("%d", skill.TotalCooldownMs);

                    // New cooldown fields
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Max Uses");
                    ImGui::TableNextColumn(); ImGui::Text("%d", skill.MaxUses);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Active Cooldowns");
                    ImGui::TableNextColumn(); ImGui::Text("%d", skill.TotalActiveCooldowns);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Remaining Uses");
                    ImGui::TableNextColumn();
                    if (skill.MaxUses > 0) {
                        int remaining = skill.MaxUses - skill.TotalActiveCooldowns;
                        if (remaining < 0) remaining = 0;
                        ImGui::Text("%d / %d", remaining, skill.MaxUses);
                    } else {
                        ImGui::TextDisabled("(no cooldown)");
                    }

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Can Be Used");
                    ImGui::TableNextColumn();
                    ImGui::TextColored(skill.CanBeUsed ? ImVec4(0.3f,1,0.3f,1) : ImVec4(1,0.3f,0.3f,1),
                                       skill.CanBeUsed ? "true" : "false");

                    // Equipment subtree
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Gem Name Hash");
                    ImGui::TableNextColumn(); ImGui::Text("0x%08X", skill.Equipment.GemNameHash);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Inventory Slot");
                    ImGui::TableNextColumn(); ImGui::Text("%d", skill.Equipment.InventorySlot);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Socket Index");
                    ImGui::TableNextColumn(); ImGui::Text("%d", skill.Equipment.SocketIndex);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Link Index");
                    ImGui::TableNextColumn(); ImGui::Text("%d", skill.Equipment.LinkIndex);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Can Be On Player Item");
                    ImGui::TableNextColumn(); ImGui::Text("%s", skill.Equipment.CanBeOnPlayerItem ? "true" : "false");
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Unknown Flag");
                    ImGui::TableNextColumn(); ImGui::Text("%d", skill.Equipment.UnknownFlag);

                    // DAT pointers — click any row to copy to clipboard for Memory.Read use.
                    DrawAddressRow("GrantedEffectsPerLevel",      skill.GrantedEffectsPerLevelAddr);
                    DrawAddressRow("ActiveSkillsDat",             skill.ActiveSkillsDatAddr);
                    DrawAddressRow("GrantedEffectStatSetsPerLvl", skill.GrantedEffectStatSetsPerLevelAddr);
                    DrawAddressRow("SkillDetailsAddr (raw)",      skill.SkillDetailsAddr);

                    ImGui::EndTable();
                }
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }
}

inline void DrawBuffsComp(const std::vector<PluginSDK::Buff>& buffs, uint32_t entityId) {
    ImGui::Text("Effects: %zu", buffs.size());
    char treeId[32];
    snprintf(treeId, sizeof(treeId), "Effects##bf%u", entityId);
    if (!buffs.empty() && ImGui::TreeNode(treeId)) {
        for (const auto& buff : buffs) {
            if (ImGui::TreeNode(buff.Name.c_str())) {
                if (ImGui::BeginTable("BuffT", 2,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Name");
                    ImGui::TableNextColumn(); ImGui::Text("%s", buff.Name.c_str());
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Total Time");
                    ImGui::TableNextColumn();
                    ImGui::Text("%.2f", buff.TotalTime > 0
                        ? buff.TotalTime : std::numeric_limits<float>::infinity());
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Time Left");
                    ImGui::TableNextColumn();
                    ImGui::Text("%.2f", buff.TimeLeft > 0
                        ? buff.TimeLeft : std::numeric_limits<float>::infinity());
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Source Entity Id");
                    ImGui::TableNextColumn(); ImGui::Text("%u", buff.SourceEntityId);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Charges");
                    ImGui::TableNextColumn(); ImGui::Text("%d", buff.Charges);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Source FlaskSlot");
                    ImGui::TableNextColumn(); ImGui::Text("%d", buff.FlaskSlot);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Effectiveness");
                    ImGui::TableNextColumn(); ImGui::Text("%d", 100 + buff.Effectiveness);
                    ImGui::EndTable();
                }
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }
}

// ----------------------------------------------------------------------------
// Type-name helpers
// ----------------------------------------------------------------------------

inline const char* EntityTypeName(PluginSDK::EntityType t) {
    using E = PluginSDK::EntityType;
    switch (t) {
        case E::Chest:             return "Chest";
        case E::NPC:               return "NPC";
        case E::Player:            return "Player";
        case E::Shrine:            return "Shrine";
        case E::Monster:           return "Monster";
        case E::DeliriumBomb:      return "DeliriumBomb";
        case E::DeliriumSpawner:   return "DeliriumSpawner";
        case E::OtherImportant:    return "Important";
        case E::Item:              return "Item";
        case E::Renderable:        return "Renderable";
        case E::AreaTransition:    return "Transition";
        case E::ExpeditionMarker:  return "Expedition";
        case E::ExpeditionRemnant: return "Remnant";
        default:                   return "Unknown";
    }
}

inline const char* NearbyZoneName(PluginSDK::NearbyZone z) {
    using Z = PluginSDK::NearbyZone;
    switch (z) {
        case Z::InnerCircle: return "Inner";
        case Z::OuterCircle: return "Outer";
        case Z::Far:         return "Far";
        default:             return "None";
    }
}

// ----------------------------------------------------------------------------
// Main entity panel — walks snapshot.Entities directly (no v5 Watch list)
// ----------------------------------------------------------------------------

inline void DrawEntitiesPanel(const PluginSDK::Context* ctx,
                              const PluginSDK::Snapshot& snapshot) {
    if (!ctx) return;

    ImGui::Text("Total: %zu entities", snapshot.Entities.size());

    static char idFilter[32] = "";
    static char pathFilter[128] = "";
    static int typeFilter = -1;
    ImGui::InputText("Filter by Id", idFilter, sizeof(idFilter));
    ImGui::InputText("Filter by Path", pathFilter, sizeof(pathFilter));

    const char* typeNames[] = {
        "All", "Unidentified", "Chest", "NPC", "Player", "Shrine", "Monster",
        "DeliriumBomb", "DeliriumSpawner", "Important", "Item", "Renderable",
        "AreaTransition", "ExpeditionMarker", "ExpeditionRemnant"
    };
    ImGui::Combo("Filter by Type", &typeFilter, typeNames, IM_ARRAYSIZE(typeNames));

    int shown = 0;
    for (const auto& e : snapshot.Entities) {
        std::string pathNarrow;
        pathNarrow.reserve(e.Path.size());
        for (wchar_t c : e.Path) pathNarrow += (c < 128) ? static_cast<char>(c) : '?';

        if (idFilter[0] != '\0') {
            char idStr[16]; snprintf(idStr, sizeof(idStr), "%u", e.Id);
            if (std::strstr(idStr, idFilter) == nullptr) continue;
        }
        if (pathFilter[0] != '\0') {
            if (pathNarrow.find(pathFilter) == std::string::npos) continue;
        }
        if (typeFilter > 0 && static_cast<int>(e.EntityType) != (typeFilter - 1)) continue;

        shown++;
        if (shown > 500) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "... truncated (>500 shown)");
            break;
        }

        char nodeLabel[256];
        snprintf(nodeLabel, sizeof(nodeLabel), "%u %s##ent%u",
                 e.Id, pathNarrow.c_str(), e.Id);

        if (ImGui::TreeNode(nodeLabel)) {
            // Entity properties
            if (ImGui::BeginTable("EntProps", 2,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableHeadersRow();

                DrawAddressRow("Address", e.Address);

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Id");
                ImGui::TableNextColumn(); ImGui::Text("%u", e.Id);

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Path");
                ImGui::TableNextColumn(); ImGui::TextWrapped("%s", pathNarrow.c_str());

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Entity Type");
                ImGui::TableNextColumn(); ImGui::Text("%s", EntityTypeName(e.EntityType));

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Entity SubType");
                ImGui::TableNextColumn(); ImGui::Text("%d", static_cast<int>(e.EntitySubtype));

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Entity State");
                ImGui::TableNextColumn(); ImGui::Text("%d", static_cast<int>(e.EntityState));

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Rarity");
                ImGui::TableNextColumn(); ImGui::Text("%d", e.Rarity);

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Nearby Zone");
                ImGui::TableNextColumn(); ImGui::Text("%s", NearbyZoneName(e.Zone));

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("World");
                ImGui::TableNextColumn();
                ImGui::Text("%.0f, %.0f, %.0f", e.WorldX, e.WorldY, e.WorldZ);

                ImGui::EndTable();
            }

            // Component readers — only call if the component exists on the entity
            char compLabel[64];
            snprintf(compLabel, sizeof(compLabel), "Components##comp%u", e.Id);
            if (ImGui::TreeNode(compLabel)) {
                const auto& c = e.Components;

                if (c.HasLife() && ImGui::TreeNode("Life")) {
                    DrawLifeComp(ctx->Components.ReadLife(c.Life));
                    ImGui::TreePop();
                }
                if (c.HasRender() && ImGui::TreeNode("Render")) {
                    DrawRenderComp(ctx->Components.ReadRender(c.Render));
                    ImGui::TreePop();
                }
                if (c.HasPositioned() && ImGui::TreeNode("Positioned")) {
                    DrawPositionedComp(ctx->Components.ReadPositioned(c.Positioned));
                    ImGui::TreePop();
                }
                if (c.HasTargetable() && ImGui::TreeNode("Targetable")) {
                    DrawTargetableComp(ctx->Components.ReadTargetable(c.Targetable));
                    ImGui::TreePop();
                }
                if (c.HasAnimated() && ImGui::TreeNode("Animated")) {
                    DrawAnimatedComp(ctx->Components.ReadAnimated(c.Animated));
                    ImGui::TreePop();
                }
                if (c.HasActor() && ImGui::TreeNode("Actor")) {
                    DrawActorComp(ctx->Components.ReadActor(c.Actor),
                                  ctx->Components.EnumerateActiveSkills(c.Actor),
                                  e.Id);
                    ImGui::TreePop();
                }
                if (c.HasBuffs() && ImGui::TreeNode("Buffs")) {
                    DrawBuffsComp(ctx->Components.EnumerateBuffs(c.Buffs), e.Id);
                    ImGui::TreePop();
                }
                // Monster mods (ObjectMagicProperties) — present on monster
                // entities; readable at spawn, before any related buff. Match on
                // MonsterMod::Id (most stable), Metadata, or Hash16/Hash32.
                if (c.HasOMP() && ImGui::TreeNode("Monster Mods")) {
                    auto mmods = ctx->Components.EnumerateMonsterMods(c.OMP);
                    ImGui::Text("EnumerateMonsterMods: %zu", mmods.size());
                    for (const auto& m : mmods) {
                        ImGui::BulletText("%s | %s | 0x%04X / 0x%08X",
                            m.Id.c_str(),
                            m.Name.empty() ? "-" : m.Name.c_str(),
                            static_cast<unsigned>(m.Hash16),
                            static_cast<unsigned>(m.Hash32));
                        if (!m.Metadata.empty() && ImGui::IsItemHovered())
                            ImGui::SetTooltip("%s", m.Metadata.c_str());
                    }
                    ImGui::TreePop();
                }
                ImGui::TreePop();
            }

            ImGui::TreePop();
        }
    }
    if (shown == 0 && !snapshot.Entities.empty()) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No entities match filter");
    }
}

} // namespace Examples
