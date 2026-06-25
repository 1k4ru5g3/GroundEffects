# GroundEffectMarker für POE2Fixer SDK v6

Dieses Plugin markiert frei konfigurierbare GroundEffect-Entities mit farbigen Kreisen im Overlay.

## Einbau

1. `https://github.com/POEFixer/ExamplePlugin` klonen oder als ZIP herunterladen.
2. `GroundEffectMarker.cpp` in den Projektordner kopieren.
3. Entweder `ExamplePlugin.cpp` durch `GroundEffectMarker.cpp` ersetzen, oder die `.vcxproj` so ändern, dass nur dieses Plugin den `CreatePlugin`/`DestroyPlugin`-Entry-Point enthält.
4. In Visual Studio 2022 `Release | x64` bauen.
5. Die DLL nach `POE2Fixer/Plugins/GroundEffectMarker/` kopieren und im Plugins-Tab aktivieren.

## Nutzung

- Im Plugin-Settings-Fenster mit `+ Effekt hinzufügen` neue Regeln anlegen.
- Als Name einen Substring aus `Entity Path` oder `TgtPath` eintragen.
- Farbe inklusive Alpha/Deckkraft über den Color Picker wählen.
- Pro Effekt per Checkbox aktivieren/deaktivieren.
- Zum Finden der richtigen Namen zuerst den Entity Explorer aus dem ExamplePlugin verwenden.

## Performance-Design

- Scan läuft standardmäßig nur alle 100 ms, nicht jedes Frame.
- Pro Frame werden nur gecachte Treffer neu auf den Screen projiziert.
- Skip in Stadt/Hideout und bei nicht fokussiertem Spiel ist standardmäßig aktiv.
- Der Entity-Type-Filter überspringt offensichtliche Nicht-GroundEffects wie Items, Monster, NPCs, Chests und AreaTransitions.
- Falls ein Effekt nicht gefunden wird, den Entity-Type-Filter testweise deaktivieren.
