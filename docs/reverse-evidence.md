# Reverse-engineering evidence

Date: 2026-09-23  
Target: `com.garena.game.kgvn` 1.63.1.10, Unity 2022.3.5f1, arm64-v8a

## Scope

This pass investigated the broken map vision, enemy cooldown overlay, camera
pullback, and the packaging/UI surrounding the Zygisk library. The supplied
`dump.cs`, `Android-Mod-Menu-5.0`, and
`zygame-mods-v2026.09.19-release.zip` were treated as reference evidence.

No code or shell behavior was copied blindly from the reference ZIP. In
particular, its global property spoofing, private-DNS mutation, SELinux file
permission changes, Shizuku deletion, trace hiding, and anti-analysis Lua were
explicitly excluded.

## Reference hashes

| Artifact | SHA-256 |
|---|---|
| `zygame-mods-v2026.09.19-release.zip` | `BDDE415B96736306356E250290162574E11A85F7B7BF8CC68F192E855CF4CD21` |
| `libs/arm64-v8a/libZyGames.so` | `6D78C94ACC1FA0E188EEE0E6825A7EE8077355209D7A8DE5D6E91DB27925C598` |
| `libs/arm64-v8a/libinject.so` | `69AC1FF34EEA1F156EA66E8C11FB0A126E7E80919BC783DEABBBE3713E17C3D8` |
| `zygisk/arm64-v8a.so` | `7720B563C6CEBB44D8FC9592B75895F52CC4F639CE8A1A52B9C5746F155D470E` |

The arm64 reference payload is ELF64/AArch64. `libZyGames.so` imports GLESv3,
`dlopen`, `dlsym`, pthreads, and exports `JNI_OnLoad`; its strings confirm an
ImGui/OpenGL overlay and generic features such as `mShowCooldown`,
`mMinimapHeroIcon`, and `wideCamera`. Those strings establish broad design
precedent, but they do not establish AOV method offsets.

`Android-Mod-Menu-5.0` establishes the normal loader pattern: wait for
`libil2cpp.so`, resolve `base + RVA`, install Dobby hooks, and preserve original
function pointers. Its example offsets and APK-oriented Java menu are unrelated
to this AOV build and were not reused.

## Root causes and authoritative evidence

### ESP placement

`dump.cs` lines 111601-111603 place `_forward` at `0x160` and `_location` at
`0x16C`. The old implementation read `location - 8`, `location - 4`, and
`location`, which produced `_forward.y`, `_forward.z`, and `_location.x`.

The replacement calls `ActorLinker.get_position()` at RVA `0x8F902B0`, adds the
same 1.6-unit actor-height convention used by the game's enemy-icon component,
then calls Unity `Camera.WorldToScreenPoint(Vector3)` at RVA `0x976CFB0`.
Depth `z <= 0` is rejected and Unity's bottom-left Y axis is converted to
ImGui's top-left axis.

### Cooldowns

`SkillSlot._skillInfoData` begins at `0x18` and `SlotType` is at `0x80`
(`dump.cs` lines 373341-373344). `SkillStateData` is emitted with an `-8` value
type bias; normalizing it gives:

| Value within `SkillStateData` | Normalized offset | Offset within `SkillSlot` |
|---|---:|---:|
| `bCDReady` | `0x09` | `0x21` |
| `curSkillCD` | `0x38` | `0x50` |
| `curSkillMaxCD` | `0x3C` | `0x54` |

The old code guessed that `CrypticInt32` could be decrypted with XOR and filled
the first four non-null slots. The replacement reads the normalized plain state
fields, maps exact `SkillSlotType` values 0 through 3, sanity-checks milliseconds,
and rounds remaining time up to seconds.

### Enemy classification and visibility

The old code searched camp IDs 1 through 8 and cached the result across matches.
The replacement identifies the local actor with `mIsHostCtrlActor` and calls
`ActorLinker.IsEnemyCamp(ActorLinker)` at RVA `0x8F98DEC`.

Direct writes to `forceVisable` could not execute the game's refresh path and
could leave visibility damaged after disabling the option. The replacement uses
`SetForceVisable(bool)` at `0x8F993F8` and restores via `RefreshVisible()` at
`0x8F92588`. Minimap pointer forcing uses
`HudComponent3D.setPointerVisible(bool,bool)` at `0x86E7B24` with
`isLogicSet=false`, so it does not overwrite the game's own logical flag.

The previous `EnemyIconOutOfVisionComponent.IconNeedShow` hook was removed:
the declaring class and its screen-border fields show that it controls the
out-of-vision edge indicator, not the minimap.

### Camera pullback

The old implementation only learned the `Moba_Camera` instance if
`get_currentZoomAmount()` happened to run after the hook was installed. It then
wrote a cached field that the game could immediately overwrite.

The replacement hooks the final `CameraSystem.GetZoomRate()` method at RVA
`0x76CB968` and returns the original rate multiplied by the live UI setting.
Turning the option off immediately returns the untouched original value, so no
field restoration is required.

## Current validation state

- Native arm64 Release build: passed locally.
- Module structure and shell validation: performed during packaging.
- Runtime injection/touch: previously observed in logcat (`nativeInjectEvent
  HOOKED`, `AOV Zygisk loaded`, `ImGui ready`).
- Corrected map/cooldown/camera behavior: requires a connected test device and a
  live match; ADB was disconnected at the end of this static pass.

