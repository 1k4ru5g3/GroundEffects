#pragma once
// ============================================================================
// ComponentReader.h — v6 SDK
// ============================================================================
// Demonstrates calling ctx->Components.ReadXxx() on the addresses already
// available in Snapshot::Entities[i].Components. Replaces the v5 demo that
// went through ComponentCache.HasLife() / ReadLifeComponent() etc.
// ============================================================================

#include "sdk/PluginSDK.h"
#include <imgui.h>
#include "SdkStatus.h"
#include <algorithm>
#include <string>

namespace Examples {

inline void ShowComponentReaderDemo(const PluginSDK::Context* ctx,
                                    const PluginSDK::Snapshot& snapshot) {
    if (!ctx) return;
    if (snapshot.State != PluginSDK::GameState::InGame) {
        ImGui::TextDisabled("Not in game");
        return;
    }

    ImGui::Text("SDK v6 Component Reader Demo");
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Nearby Entity Health")) {
        int shown = 0;
        for (const auto& entity : snapshot.Entities) {
            if (!entity.Components.HasLife()) continue;

            auto life = ctx->Components.ReadLife(entity.Components.Life);
            if (!life.Valid) continue;

            float hpPct = ctx->Components.GetHealthPercent(entity.Components.Life);
            ImGui::Text("Entity %u: HP %d/%d (%.0f%%)",
                entity.Id, life.Health.Current, life.Health.Total, hpPct);

            if (++shown >= 20) {
                ImGui::TextDisabled("... (%d more with Life)",
                    static_cast<int>(snapshot.Entities.size()) - shown);
                break;
            }
        }
        if (shown == 0) ImGui::TextDisabled("No entities with Life component nearby");
    }

    if (ImGui::CollapsingHeader("Entity Positions")) {
        int shown = 0;
        for (const auto& entity : snapshot.Entities) {
            if (!entity.Components.HasRender()) continue;

            auto render = ctx->Components.ReadRender(entity.Components.Render);
            if (!render.Valid) continue;

            ImGui::Text("Entity %u: World(%.1f, %.1f, %.1f)",
                entity.Id, render.WorldX, render.WorldY, render.WorldZ);

            if (++shown >= 20) {
                ImGui::TextDisabled("... (%d more with Render)",
                    static_cast<int>(snapshot.Entities.size()) - shown);
                break;
            }
        }
        if (shown == 0) ImGui::TextDisabled("No entities with Render component nearby");
    }

    if (ImGui::CollapsingHeader("Items on Ground")) {
        // Ground items are WorldItem container entities. The inner item
        // entity (with Mods/Base/Stack/Sockets components) is reached via
        // ctx->Entities.GetWorldItemInner(containerAddr).

        int total = 0;
        int withMods = 0;
        for (const auto& e : snapshot.Entities) {
            if (e.EntityType != PluginSDK::EntityType::Item) continue;
            ++total;
            auto inner = ctx->Entities.GetWorldItemInner(e.Address);
            if (inner && inner->Components.HasMods()) ++withMods;
        }
        ImGui::Text("WorldItems: %d total | %d with Mods", total, withMods);
        if (total == 0) {
            ImGui::TextDisabled("Walk near a dropped item to see it here.");
        } else {
            ImGui::Separator();
            int shown = 0;
            for (const auto& entity : snapshot.Entities) {
                if (entity.EntityType != PluginSDK::EntityType::Item) continue;

                auto inner = ctx->Entities.GetWorldItemInner(entity.Address);
                if (!inner) {
                    ImGui::TextDisabled("id=%u  (inner not yet resolved)", entity.Id);
                    if (++shown >= 20) break;
                    continue;
                }

                // Truncate inner path to last 2 segments for readability.
                std::string narrowPath;
                narrowPath.reserve(inner->Path.size());
                for (wchar_t c : inner->Path)
                    narrowPath += (c < 128) ? static_cast<char>(c) : '?';
                size_t lastSlash = narrowPath.rfind('/');
                size_t prevSlash = (lastSlash != std::string::npos && lastSlash > 0)
                                       ? narrowPath.rfind('/', lastSlash - 1)
                                       : std::string::npos;
                std::string shortPath = (prevSlash != std::string::npos)
                                            ? narrowPath.substr(prevSlash + 1)
                                            : narrowPath;

                // Rarity colour: 0=Normal (white), 1=Magic (blue),
                // 2=Rare (yellow), 3=Unique (orange).
                int rarity = 0;
                int itemLevel = 0;
                bool isIdentified = false;
                bool isCorrupted = false;
                int implicitCount = 0;
                int explicitCount = 0;
                if (inner->Components.HasMods()) {
                    auto mods = ctx->Components.ReadMods(inner->Components.Mods);
                    if (mods.Valid) {
                        rarity        = mods.Rarity;
                        itemLevel     = mods.ItemLevel;
                        isIdentified  = mods.IsIdentified;
                        isCorrupted   = mods.IsCorrupted;
                    }
                    // InventoryService::ReadItemMods returns a default-constructed
                    // ItemMods on failure; empty vectors mean no mods.
                    PluginSDK::ItemMods modList =
                        ctx->Inventory.ReadItemMods(entity.Address);
                    implicitCount = static_cast<int>(modList.ImplicitMods.size());
                    explicitCount = static_cast<int>(modList.ExplicitMods.size());
                }

                // Item names — both auto-resolve WorldItem containers (Task 4),
                // so passing the container address directly works.
                std::string baseTypeName = ctx->Inventory.ReadItemBaseTypeName(entity.Address);
                std::string uniqueName   = ctx->Inventory.ReadItemUniqueName(entity.Address);

                ImVec4 colour =
                    rarity == 1 ? ImVec4(0.55f, 0.65f, 1.00f, 1.0f)  // magic
                  : rarity == 2 ? ImVec4(1.00f, 0.95f, 0.45f, 1.0f)  // rare
                  : rarity == 3 ? ImVec4(1.00f, 0.60f, 0.20f, 1.0f)  // unique
                  :               ImVec4(0.95f, 0.95f, 0.95f, 1.0f); // normal
                ImGui::PushStyleColor(ImGuiCol_Text, colour);
                // Line 1: rarity-coloured display name.
                //   Unique items get "Unique Name (Base Type)".
                //   Non-uniques just show the base type name.
                //   Fallback to truncated metadata path if names are empty
                //   (e.g. very early mid-spawn before names are streamed in).
                if (!uniqueName.empty()) {
                    ImGui::Text("%s %s", uniqueName.c_str(), baseTypeName.c_str());
                } else if (!baseTypeName.empty()) {
                    ImGui::TextUnformatted(baseTypeName.c_str());
                } else {
                    ImGui::TextUnformatted(shortPath.c_str());
                }
                ImGui::PopStyleColor();
                // Line 2: indented diagnostic row (uncoloured).
                ImGui::TextDisabled(
                    "    id=%u  iLvl=%d  rarity=%d  ident=%s  corr=%s  +%d/%dM",
                    entity.Id, itemLevel, rarity,
                    isIdentified ? "y" : "n",
                    isCorrupted  ? "y" : "n",
                    implicitCount, explicitCount);

                if (++shown >= 20) {
                    ImGui::TextDisabled("... %d more items", total - shown);
                    break;
                }
            }
        }
    }

    if (ImGui::CollapsingHeader("UI Navigation Demo")) {
        uintptr_t gameUi = ctx->Ui.GetGameUiRoot();
        if (gameUi != 0) {
            auto children = ctx->Ui.GetChildren(gameUi);
            ImGui::Text("GameUI root has %d children", static_cast<int>(children.size()));

            int limit = (static_cast<int>(children.size()) < 10) ? static_cast<int>(children.size()) : 10;
            for (int i = 0; i < limit; i++) {
                auto elem = ctx->Ui.Read(children[i]);
                if (!elem.Valid) continue;
                std::string sid = ctx->Ui.GetStringId(children[i]);

                ImGui::Text("  [%d] Type=0x%04X Visible=%s StringId=\"%s\"",
                    i, elem.ElementType,
                    elem.IsVisible ? "yes" : "no",
                    sid.c_str());
            }
            if (static_cast<int>(children.size()) > 10) {
                ImGui::TextDisabled("  ... %d more children",
                    static_cast<int>(children.size()) - 10);
            }
        } else {
            ImGui::TextDisabled("GameUI root not available (not in game?)");
        }
    }

    if (ImGui::CollapsingHeader("All Component Readers (per-method)")) {
        const auto& player = snapshot.Player;

        auto reportComponent = [&ctx](const char* name, uintptr_t addr,
                                        bool (*tryRead)(const PluginSDK::Context*,
                                                         uintptr_t)) {
            if (addr == 0) {
                Examples::StatusBadge(Examples::StatusLevel::Warn,
                    (std::string(name) + " - address not set on player").c_str());
                return;
            }
            if (tryRead(ctx, addr))
                Examples::StatusBadge(Examples::StatusLevel::Ok,
                    (std::string(name) + " - Valid").c_str());
            else
                Examples::StatusBadge(Examples::StatusLevel::Fail,
                    (std::string(name) + " - Read returned invalid").c_str());
        };

        reportComponent("Life",        player.Components.Life,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadLife(a).Valid; });
        reportComponent("Render",      player.Components.Render,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadRender(a).Valid; });
        reportComponent("Positioned",  player.Components.Positioned,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadPositioned(a).Valid; });
        reportComponent("Targetable",  player.Components.Targetable,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadTargetable(a).Valid; });
        reportComponent("Chest",       player.Components.Chest,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadChest(a).Valid; });
        reportComponent("Shrine",      player.Components.Shrine,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadShrine(a).Valid; });
        reportComponent("Stack",       player.Components.Stack,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadStack(a).Valid; });
        reportComponent("Charges",     player.Components.Charges,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadCharges(a).Valid; });
        reportComponent("Player",      player.Components.Player,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadPlayer(a).Valid; });
        reportComponent("Animated",    player.Components.Animated,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadAnimated(a).Valid; });
        reportComponent("Transitionable", player.Components.Transitionable,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadTransitionable(a).Valid; });
        reportComponent("TriggerableBlockage", player.Components.TriggerableBlockage,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadTriggerableBlockage(a).Valid; });
        reportComponent("MinimapIcon", player.Components.MinimapIcon,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadMinimapIcon(a).Valid; });
        reportComponent("StateMachine", player.Components.StateMachine,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadStateMachine(a).Valid; });
        reportComponent("Base",        player.Components.Base,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadBase(a).Valid; });
        reportComponent("Mods",        player.Components.Mods,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadMods(a).Valid; });
        reportComponent("Stats",       player.Components.Stats,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadStats(a).Valid; });
        reportComponent("Buffs",       player.Components.Buffs,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadBuffs(a).Valid; });
        reportComponent("Actor",       player.Components.Actor,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadActor(a).Valid; });
        reportComponent("Npc",         player.Components.Npc,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadNpc(a).Valid; });
        reportComponent("DiesAfterTime", player.Components.DiesAfterTime,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadDiesAfterTime(a).Valid; });
    }

    if (ImGui::CollapsingHeader("Enumerators")) {
        const auto& player = snapshot.Player;

        if (player.Components.HasBuffs()) {
            auto buffs = ctx->Components.EnumerateBuffs(player.Components.Buffs);
            ImGui::Text("EnumerateBuffs: %zu buffs", buffs.size());
            Examples::StatusBadge(Examples::StatusLevel::Ok, "EnumerateBuffs returned");
        } else {
            Examples::StatusBadge(Examples::StatusLevel::Warn, "Buffs component address is 0");
        }

        if (player.Components.HasActor()) {
            auto skills = ctx->Components.EnumerateActiveSkills(player.Components.Actor);
            ImGui::Text("EnumerateActiveSkills: %zu skills", skills.size());
            Examples::StatusBadge(Examples::StatusLevel::Ok, "EnumerateActiveSkills returned");
        } else {
            Examples::StatusBadge(Examples::StatusLevel::Warn, "Actor component address is 0");
        }

        if (player.Components.HasStats()) {
            auto stats = ctx->Components.EnumerateStats(player.Components.Stats);
            ImGui::Text("EnumerateStats: %zu stats", stats.size());
            Examples::StatusBadge(Examples::StatusLevel::Ok, "EnumerateStats returned");
        } else {
            Examples::StatusBadge(Examples::StatusLevel::Warn, "Stats component address is 0");
        }

        // Item mods enumeration requires an item entity; skipped here unless a
        // nearby item has Mods. The Item Mods (Nearby Items) header above covers it.
        ImGui::TextDisabled("EnumerateItemMods: see 'Item Mods (Nearby Items)' header");

        // Monster mods live on monster entities (ObjectMagicProperties / OMP),
        // not the player. See the per-entity "Monster Mods" node in the Entities
        // tab for a live ctx->Components.EnumerateMonsterMods(OMP) demo.
        ImGui::TextDisabled("EnumerateMonsterMods: see Entities tab -> 'Monster Mods'");
    }

    if (ImGui::CollapsingHeader("Convenience Helpers")) {
        const auto& player = snapshot.Player;
        if (player.Components.HasLife()) {
            ImGui::Text("Player alive: %s",
                ctx->Components.IsAlive(player.Components.Life) ? "yes" : "no");
            ImGui::Text("HP: %.1f%%", ctx->Components.GetHealthPercent(player.Components.Life));
            ImGui::Text("ES: %.1f%%", ctx->Components.GetEsPercent(player.Components.Life));
            ImGui::Text("MP: %.1f%%", ctx->Components.GetManaPercent(player.Components.Life));
        } else {
            ImGui::TextDisabled("Player Life component not available");
        }
        if (player.Components.HasPlayer()) {
            ImGui::Text("Name: %s",
                ctx->Components.GetPlayerName(player.Components.Player).c_str());
        }
        if (player.Components.HasRender()) {
            float wx, wy, wz;
            if (ctx->Components.GetWorldPosition(player.Components.Render, wx, wy, wz)) {
                ImGui::Text("Position: (%.1f, %.1f, %.1f)", wx, wy, wz);
            }
        }
        if (player.Components.HasChest()) {
            ImGui::Text("Chest opened: %s",
                ctx->Components.IsChestOpened(player.Components.Chest) ? "yes" : "no");
        }

        // Item helpers - only meaningful for items, not the player. Demonstrated
        // for the first nearby item entity that has Mods component.
        for (const auto& entity : snapshot.Entities) {
            if (!entity.Components.HasMods()) continue;
            if (!entity.Components.HasStack() && !entity.Components.HasBase()) continue;
            ImGui::Separator();
            ImGui::Text("Helpers for item entity %u:", entity.Id);
            if (entity.Components.HasMods()) {
                int rarity = ctx->Components.GetItemRarity(entity.Components.Mods);
                bool ident = ctx->Components.IsItemIdentified(entity.Components.Mods);
                ImGui::Text("  Rarity=%d  Identified=%s", rarity, ident ? "yes" : "no");
                Examples::StatusBadge(Examples::StatusLevel::Ok,
                    "GetItemRarity + IsItemIdentified");
            }
            if (entity.Components.HasStack()) {
                int stack = ctx->Components.GetStackCount(entity.Components.Stack);
                ImGui::Text("  Stack=%d", stack);
                Examples::StatusBadge(Examples::StatusLevel::Ok, "GetStackCount");
            }
            break;  // first matching item is enough
        }
    }
}

} // namespace Examples
