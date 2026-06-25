# GroundEffects

`GroundEffects` is a POE2Fixer SDK v6 plugin that highlights hostile Path of Exile 2 `GroundEffect` components with configurable colored world-space circles.

This build intentionally removes all explosion / entity hazard / `ServerEffect` handling. It only reads real `GroundEffect` components through:

```cpp
ctx()->Components.ReadGroundEffect(entity.Address)
```

## What this version does

- Highlights real hostile GroundEffects such as:
  - `ShockedGround`
  - `ChilledGround`
  - `IgnitedGround`
  - `CausticCloud`
- Uses `GroundEffect.TypeId` for matching.
- Uses the actual `GroundEffect.Radius` for circle size.
- Filters friendly/player-owned effects through `Positioned.IsFriendly == false` by default.
- Ignores explosions and `Metadata/Effects/ServerEffect` until the SDK exposes reliable data for them.

## Performance changes

This version is intentionally conservative to avoid POE2Fixer's `DrawUI > 50ms` watchdog:

- No per-frame circle projection. Projected screen points are cached during the scan step.
- Cheap wide-string path prefilter before UTF-8 conversion.
- Scan interval defaults to `150 ms`.
- Circle quality defaults to `16` segments.
- Max markers defaults to `80`.
- A `Max scan time (ms)` setting limits scan work per DrawUI call.
- Discovery mode and discovery file logging are reset to disabled on load. Re-enable manually only when needed.
- Old explosion / hazard rules are removed from the loaded config.

## Installation

1. Clone or copy `POEFixer/ExamplePlugin`.
2. Copy `GroundEffects.cpp` into the project.
3. Replace `ExamplePlugin.cpp`, or make sure `GroundEffects.cpp` is the only source file exporting `CreatePlugin` and `DestroyPlugin`.
4. Build `Release | x64` in Visual Studio 2022.
5. Copy the resulting DLL to:

```text
POE2Fixer/Plugins/GroundEffects/
```

6. Enable the plugin in POE2Fixer.

## Configuration

The config file is stored at:

```text
POE2Fixer/Plugins/GroundEffects/config/GroundEffects.ini
```

After installing this rollback version, open the plugin settings once and press `Save`. This rewrites the config without old explosion settings.

If POE2Fixer still disables the plugin, try these settings:

```text
Scan interval: 200 ms
Max scan time: 5 ms
Max markers: 48
Circle quality: 12
Discovery mode: off
Append discovery log: off
```

## Discovery Mode

Discovery Mode only lists real `GroundEffect` components. It does not discover explosions or `ServerEffect` hazards.

The table shows:

- TypeId
- Radius
- IsFriendly
- EndEffect
- BuffVisual1
- BuffVisual2
- AoFile
- Entity path

Use the `Copy` button to copy the `TypeId` into a rule.
