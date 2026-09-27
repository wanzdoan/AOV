# First Frost ImGui redesign

Reference: `C:/Users/Administrator/Downloads/menu/menu.html`.

The production menu is implemented in `jni/src/menu/menu.cpp`. This is a native
ImGui view, not a WebView or an HTML screenshot.

## Reference mapping

| HTML component | Native implementation |
| --- | --- |
| Dark 960 px shell, title and footer | Responsive 960 × 576 logical-pixel window, #09090b body, #111114 bars, rounded border |
| Five equally spaced sidebar tabs | Map / Aim & Auto / Skin / Settings / Status; monochrome vector icons, green active edge |
| Setting groups | Rounded rows, Vietnamese descriptions, green switches; camera, Aim and Punish settings attached inside their parent |
| Camera range | 0–30, using the existing 0.036186 conversion |
| Aim | Independent C1/C2/C3 cards; target modal; range 0–25; lead 0–5 in steps of 0.1; Elsu switch |
| Automatic actions | Existing Bộc Phá, Trừng Trị, Caesar/Rồng and blue/red buff flags |
| Skin controls | Full skin switch and two independent scrollable modals |
| Settings | 120 FPS and persistent “Lưu cấu hình” switch, with explicit save/error feedback |
| Status grid | 15 fields: resolver/touch readiness, FPS and frame time, module/supported game version, resolution/scale, menu uptime, enabled feature count, persistence/last-save time, both selected cosmetic names, unverified anti-ban status |
| Yellow / red dots | Yellow hides the entire menu including its header and leaves the launcher visible. Red also hides the launcher; its previous touch location can reopen the menu |
| Launcher | 84 logical-pixel touch area (previously 54), layered green rim, vector frost emblem and centered AOV label; draggable |
| Footer | FPS only; the “The First Frost Mod Menu” caption has been removed |
| Independent modal | Dim full-screen backdrop, bounded scrolling, selected check, close button, outside click and Escape |
| Responsive and touch behavior | Narrow sidebar and one-column status grid; screen-bound placement on rotation; drag title/launcher; swipe content and modal lists |

## Deliberate data differences

- The reference's 16 fictional skin names are replaced by the existing 74 button
  and 62 notification entries, with their actual non-contiguous game IDs.
  Index 0 is displayed as “Mặc Định” and still maps to ID 0.
- Existing feature defaults are retained, including all three skill toggles.
- The displayed version is v2.6.0. FPS is measured by ImGui, not randomized.
- Anti-ban protection is shown as “Chưa xác minh”; the interface does not claim
  that account safety was measured.
- A translucent dark modal backdrop replaces CSS backdrop blur. ImGui does not
  sample or blur the game's framebuffer.

## Verification and artifacts

Run from the project root:

```powershell
./scripts/run-menu-preview.ps1
./scripts/test-config-store.ps1
./scripts/package-first-frost.ps1
```

The desktop harness compiles the actual menu implementation and actual config
tables with the project's Dear ImGui sources. Only Android service includes are
replaced with test stubs. A hidden Windows OpenGL window renders the screenshots;
input tests send mouse and key events through ImGui.

Checks cover all five tabs, switches, camera endpoint, skill selection,
Aim/range/lead mirrors, Elsu, both auto actions and both Punish target switches,
modal selection, actual last skin IDs, list swipe without selection, isolated
background scrolling, full-screen modal capture bounds, outside-click/Escape
dismissal, scroll reset after tab changes, complete yellow-button hide with no
remaining header or captured menu rectangle, larger draggable launcher, red
hide/reopen, save toggle, and landscape/portrait/small-screen placement.

Screenshots: [Map](../build/ui_preview/01-map.png),
[expanded camera](../build/ui_preview/02-map-expanded.png),
[Aim](../build/ui_preview/03-aim.png),
[target modal](../build/ui_preview/04-target-modal.png),
[Punish](../build/ui_preview/05b-punish-expanded.png),
[Skin](../build/ui_preview/06-skin.png),
[button modal](../build/ui_preview/07-button-modal.png),
[Settings](../build/ui_preview/08-settings.png),
[Status](../build/ui_preview/09-status.png),
[Status details](../build/ui_preview/09b-status-details.png),
[hidden menu / new launcher](../build/ui_preview/10-hidden-launcher.png),
[portrait](../build/ui_preview/11-portrait.png),
[small screen](../build/ui_preview/12-small.png),
[high DPI](../build/ui_preview/13-high-dpi.png).

The module skill's structural validator reports no errors. It warns about the
existing three lifecycle scripts' first lines (their UTF-8 BOM precedes the
shebang); these files are outside this UI change and are preserved.
The packaging script checks required ZIP entries, forward-slash paths, and
the SHA-256 of the packaged arm64 library against the freshly built library.
The older `aov_zygisk_v2.6.0.zip` is preserved.
The updated package is `aov_zygisk_v2.6.0_first_frost_r2.zip`; the first
`aov_zygisk_v2.6.0_first_frost.zip` is also preserved.

## Persistent configuration

“Lưu cấu hình” is off by default. Turning it on immediately writes the current
20 exposed settings. Further toggle, selector and slider changes are saved when
the interaction ends, with no idle-frame disk writes. Turning it off persists a
disabled marker: it does not change the current session's feature values, but
the next process starts with defaults instead of restoring them.

Storage is `<app_data_dir>/files/first_frost.cfg`, where `app_data_dir` is supplied
by Zygisk rather than a hard-coded user-0 path. Initialization runs after app
specialization and before the feature/render threads. No root write or shared
storage permission is needed. Clearing the game's app data removes this file.

The versioned, bounded text format validates every field before applying any of
them. Unknown versions/fields, duplicates, missing entries, invalid numbers,
out-of-range indices and oversized files are rejected without partial restoration.
Cosmetic IDs are derived from the current tables and all Aim mirror variables
are restored along with their config fields.

On Android, save uses a private 0600 temporary file, flush/fsync and same-directory
rename. Failed writes preserve the previous snapshot and are reported in
Settings/Status. Automatic retries are limited to once per five seconds. Saving
and configuration capture remain on the render thread; initialization precedes it.
The desktop test uses Windows atomic replacement for the corresponding operation.

The persistence test script launches fresh processes for save, restore, disable
and disabled restore. It also tests automatic updates, absence of idle writes,
all exposed settings/mirrors, the highest cosmetic indices, a forced write failure,
corrupt/truncated/unsupported/oversized files and missing app-data paths.

No ADB device was connected during this work. Screenshots use Windows Segoe UI
regular/bold; Android loads its available Roboto/Noto/DroidSans fonts. The host
checks prove UI behavior, not in-game hook behavior or physical-device touch
mapping. No device installation was performed.
