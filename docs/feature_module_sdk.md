# Feature Module SDK - Developer Guide

The **Feature Module SDK** (`core/modules/`) lets you add compile-time firmware features that live in **this repo, another directory, or a separate repository**. Modules own project sidecar files and optional Performance Mode pages without editing File Manager call sites.

This is the third extension path in Tracker CE:

| Extension path | Delivery | Typical use |
| :--- | :--- | :--- |
| **`.ptfx` Sample Editor plugins** | SD card binary | Offline sample DSP / generators |
| **PolySynth Custom Synth SDK** | Compile into `.ptf` | Real-time synth engines (Tracker+ / Mini) |
| **Feature Module SDK** | Compile into `.ptf` from external dirs | Project sidecars, Performance Mode pages, main-thread features |

There is **no SD-card code loader** for feature modules. A module is ordinary C++ compiled from `FEATURE_MODULE_DIRS` and linked into the firmware image.

---

## 1. Architecture

```text
FEATURE_MODULE_DIRS ──► module.mk + sources
        │
        ▼
 generate_registry.py ──► feature_modules_registry.cpp
        │
        ▼
 FeatureModuleRegistry ──► FileManager lifecycle
        │                  PerformancePage + PerformancePageHost
        ▼
 ModuleSidecar helper
 Workspace/<id>.bin
 <project>/<id>.bin
```

Modules are **not** registered by editing `core/` by hand. Each module directory has a `module.mk`; the build generates `registerFeatureModules()` so every listed instance is linked (needed because `libTracker.a` otherwise drops unreferenced objects).

---

## 2. External module layout

```text
my-module/                         # any path, any repo
├── module.mk
├── MyModule.h
└── MyModule.cpp
```

`module.mk`:

```makefile
MODULE_NAME     := my-module
MODULE_INSTANCE := myModule          # C++ object symbol
MODULE_HEADER   := MyModule.h
MODULE_SRCS     := MyModule.cpp      # optional; defaults to all *.cpp
```

### Interactive module picker

```bash
make select
# equivalent:
python3 scripts/build_with_modules.py
```

Scans [`feature-modules/`](../feature-modules/) for directories with `module.mk`, lets you toggle modules and pick `all` / `original` / `plus` / `mini`, then runs `make` with `FEATURE_MODULE_DIRS` set. Last selection is stored in `.feature-modules-selection`.

```bash
make list-modules
python3 scripts/build_with_modules.py --modules performance-fx-scenes --target plus
```

### Non-interactive build

```bash
# Default: every feature-modules/*/module.mk
make all

# Your own checkout(s), space-separated
make all FEATURE_MODULE_DIRS="$HOME/src/my-tracker-module /other/repo/another-module"

# Firmware only, no feature modules
make all FEATURE_MODULE_DIRS=
```

The top-level [`Makefile`](../Makefile) passes `FEATURE_MODULE_DIRS` into `core/`.

---

## 3. `FeatureModule` contract

Headers: [`FeatureModule.h`](../core/modules/FeatureModule.h), [`PerformanceHost.h`](../core/modules/PerformanceHost.h), [`ScreenHost.h`](../core/modules/ScreenHost.h), [`ModuleSidecar.h`](../core/modules/ModuleSidecar.h)

```cpp
class FeatureModule
{
  public:
    virtual const char* id() const = 0; // sidecar basename, e.g. "perfscenes"

    virtual void init(FileSystem* fs) {}
    virtual void onWorkspaceClear() {}
    virtual void onWorkspaceLoad() {}
    virtual void onProjectOpen(const char* projectPath, const char* projectName) {}
    virtual void onProjectSave(const char* projectPath, const char* projectName) {}
    virtual void update() {} // main thread only

    virtual PerformancePage* performancePage() { return nullptr; }
    virtual ScreenExtension* screenExtension() { return nullptr; }
};
```

| Hook | Called from |
| :--- | :--- |
| `init` | `cFileManager::init` after generated `registerFeatureModules` |
| `onWorkspaceClear` | New project / workspace clear |
| `onWorkspaceLoad` | Workspace project load |
| `onProjectOpen` | Project open |
| `onProjectSave` | Project save |
| `update` | `TrackerImpl::update` (main thread) |

Depend only on SDK + `Platform.h` / `cr_section_macros.h` for `__NOINIT(EXTERNAL_RAM)`. Do **not** include `performanceMode.h`, `songEditor.h`, `display.h`, or File Manager headers from an external module.

---

## 3b. Screen extensions (`ScreenHost.h`)

Feature modules extend **existing** `ModuleBase` screens through `screenExtension()`. They do not add top-level screens (slot `11` in `Interface::modules[]` remains reserved for firmware).

```cpp
enum class ScreenId : uint8_t { Song = 0 };

class ScreenExtension
{
  public:
    virtual ScreenId screen() const = 0;
    virtual SongListColumn* asSongListColumn() { return nullptr; }
    // Future screens add as*() accessors here.
};
```

### Song list column

`SongListColumn` is a focusable column on the Song playlist (between the pattern list and tempo). Firmware owns layout/navigation; the module owns header text, cell text, and edit/playback behavior via `SongScreenHost`.

| Module API | Role |
| :--- | :--- |
| `header()` / `formatCell()` | Drawn by `cSongPlayer` |
| `focusEncoderLabel()` | Pattern +/- label while the column is focused |
| `onEncoder` / `onClear` | Edit mode input |
| `onInserted` / `onRemoved` / `clearAll` | Stay aligned with Add/Delete Slot and MOD/IT import |
| `onSlotStart` | Main-thread hook when song playback enters a row (module owns recall + any UI refresh) |

`FeatureModuleRegistry::updateAll()` calls `pollSongListColumnSlotStarts()` (Song host) so Tracker does not latch playlist position itself. MOD/IT import calls `notifySongPlaylistReplaced()` instead of touching the column directly. `getSongListColumn()` returns the first Song `ScreenExtension` that exposes a column. `SongListColumn::kSongSlotCount` must match `SONG_MAX` (255).

To extend another screen later: add a host header + `as*()` on `ScreenExtension`, implement the host next to that screen (same pattern as `getSongScreenHost()` / Performance pages).

---

## 4. Project sidecars (`ModuleSidecar`)

Paths from `id()`:

* Workspace: `Workspace/<id>.bin`
* Project: `<projectPath>/<projectName>/<id>.bin`

Rules:

1. Magic + version header
2. Struct size **4-byte aligned** (FatFs DMA on IMX)
3. Bank in `__NOINIT(EXTERNAL_RAM)`, cleared explicitly in `init` / `onWorkspaceClear`
4. Bad magic/version → clear the bank

Keep filenames stable when migrating. Scenes uses `id() == "perfscenes"` → `perfscenes.bin`.

---

## 5. Performance Mode page + host

```cpp
class PerformancePage
{
  public:
    virtual const char* title() const = 0;
    virtual void onShow(PerformancePageHost& host) = 0;
    virtual void onHide(PerformancePageHost& host) = 0;
    virtual void onScreenButton(PerformancePageHost& host, uint8_t button, uint8_t state) = 0;
};
```

`PerformancePageHost` (implemented by firmware) is the only UI/engine API a page should call:

* `setPageTitle`, `setScreenLabel`, `finishScreenLabels`
* `restoreTrackLabels`, `refreshFxRack`, `isShowingFxRack`
* `isShiftPressed`
* `captureFxScene` / `recallFxScene` (`strPerfScene` — 12 FX slots)
* `capturePatternScene` / `recallPatternScene` (`strPatternScene` — per-track pattern cues)
* `captureMuteScene` / `recallMuteScene` (`strMuteScene` — sample + MIDI mute masks)

Controls:

* Desktop: **Shift + Performance** cycles registered pages in `MODULE_ORDER`, then returns to the track strip
* Tracker Mini: **Middle** cycles the same way
* Screen buttons `0`–`7` → `onScreenButton`

New top-level screens still use `ModuleBase` and the fixed `Interface::modules[]` table (slot `11` reserved).

---

## 6. Bundled modules

| Directory | Title | `id()` / sidecar | `MODULE_ORDER` |
| :--- | :--- | :--- | ---: |
| [`performance-fx-scenes/`](../feature-modules/performance-fx-scenes) | Scenes (+ Song FX column) | `perfscenes` | 10 |
| [`pattern-launch-scenes/`](../feature-modules/pattern-launch-scenes) | Patterns | `patternscenes` | 20 |
| [`mute-banks/`](../feature-modules/mute-banks) | Mutes | `mutebanks` | 30 |

All three use only SDK headers + `ModuleSidecar` + `PerformancePageHost`. Shift + screen button stores; screen button recalls. `performance-fx-scenes` also implements `ScreenExtension` / `SongListColumn` for the Song Mode FX column.

---

## 7. Skeleton module

```cpp
// MyModule.h
#pragma once
#include "FeatureModule.h"

class MyModule : public FeatureModule
{
  public:
    const char* id() const override { return "myfeature"; }
    void init(FileSystem* fs) override;
    void onWorkspaceClear() override;
    void onWorkspaceLoad() override;
    void onProjectOpen(const char* projectPath, const char* projectName) override;
    void onProjectSave(const char* projectPath, const char* projectName) override;
};

extern MyModule myModule;
```

```makefile
# module.mk
MODULE_NAME        := my-module
MODULE_TITLE       := My Module
MODULE_DESCRIPTION := Short blurb shown in make select
MODULE_ORDER       := 40              # Performance page cycle order (lower first)
MODULE_INSTANCE    := myModule
MODULE_HEADER      := MyModule.h
MODULE_SRCS        := MyModule.cpp
```

```bash
# Drop the folder into feature-modules/ and pick it with:
make select

# Or pass an absolute path from another repo:
make all FEATURE_MODULE_DIRS="/absolute/path/to/my-module"
```

---

## 8. Out of scope (this version)

* SD-card executable modules (use `.ptfx` for Sample Editor DSP)
* Audio-callback / ISR hooks
* Dynamic Performance FX id tables (`performanceFx.cpp` enums append-only)
* Synth engine authoring (PolySynth SDK)
