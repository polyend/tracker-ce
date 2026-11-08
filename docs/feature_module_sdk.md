# Feature Module SDK — Developer Guide

The **Feature Module SDK** (`core/modules/`) lets you add compile-time firmware features that live in **this repo, another directory, or a separate repository**. Modules are ordinary C++ compiled from `FEATURE_MODULE_DIRS` and linked into the `.ptf` image.

There is **no SD-card code loader** for feature modules (use `.ptfx` for Sample Editor DSP plugins).

This is the third extension path in Tracker CE:

| Extension path | Delivery | Typical use |
| :--- | :--- | :--- |
| **`.ptfx` Sample Editor plugins** | SD card binary | Offline sample DSP / generators |
| **PolySynth Custom Synth SDK** | Compile into `.ptf` | Real-time synth engines (Tracker+ / Mini) |
| **Feature Module SDK** | Compile into `.ptf` from external dirs | Sidecars, Performance pages, Song columns, pattern Step FX |

---

## 1. What a module can do

| Capability | SDK surface | Who owns what |
| :--- | :--- | :--- |
| **Project sidecar** | `ModuleSidecar` + lifecycle hooks | Module: file format + bank. Firmware: calls open/save/clear. |
| **Performance Mode page** | `PerformancePage` + `PerformancePageHost` | Module: UI labels + store/recall. Firmware: page cycle + screen buttons. |
| **Song Mode list column** | `ScreenExtension` / `SongListColumn` + `SongScreenHost` | Module: header, cells, edit, playback. Firmware: layout, navigation, draw dispatch. |
| **Pattern Step FX** | `PatternStepFxInfo` + `onPatternStepFx` | Module: letter, name, range, behavior. Firmware: runtime FX catalog + sequencer dispatch. |

**Boundary rule:** change core only to grow the **host/dispatcher** surface. Product behavior (scene banks, `"FX"` / `"Scene"` strings, recall logic) stays in the module.

Do **not** include from an external module: `performanceMode.h`, `songEditor.h`, `display.h`, File Manager headers, or sequencer internals. Depend on SDK headers + `Platform.h` / `cr_section_macros.h` for `__NOINIT(EXTERNAL_RAM)`.

---

## 2. Architecture

```text
FEATURE_MODULE_DIRS ──► module.mk + sources
        │
        ▼
 generate_registry.py ──► feature_modules_registry.cpp
        │
        ▼
 FeatureModuleRegistry
        ├── FileManager lifecycle (init / workspace / project)
        ├── PerformancePage cycle (Shift+Perf / Mini Middle)
        ├── Song list column (getSongListColumn, poll/notify helpers)
        └── Pattern Step FX catalog (merged into picker at init)
```

Modules are **not** registered by editing `core/` by hand. Each module directory has a `module.mk`; the build generates `registerFeatureModules()` so every listed instance is linked (needed because `libTracker.a` otherwise drops unreferenced objects).

SDK headers (all under [`core/modules/`](../core/modules/)):

| Header | Role |
| :--- | :--- |
| [`FeatureModule.h`](../core/modules/FeatureModule.h) | Root contract + `PerformancePage` |
| [`PerformanceHost.h`](../core/modules/PerformanceHost.h) | Performance Mode host + scene snapshot structs |
| [`ScreenHost.h`](../core/modules/ScreenHost.h) | Song (and future) screen extensions |
| [`PatternStepFx.h`](../core/modules/PatternStepFx.h) | Pattern Step FX export descriptor |
| [`ModuleSidecar.h`](../core/modules/ModuleSidecar.h) | Workspace/project `.bin` path + read/write |

---

## 3. External module layout

```text
my-module/                         # any path, any repo
├── module.mk
├── MyModule.h
└── MyModule.cpp
```

```makefile
MODULE_NAME        := my-module
MODULE_TITLE       := My Module
MODULE_DESCRIPTION := Short blurb shown in make select
MODULE_ORDER       := 40              # Performance page + FX catalog order (lower first)
MODULE_INSTANCE    := myModule        # C++ object symbol
MODULE_HEADER      := MyModule.h
MODULE_SRCS        := MyModule.cpp    # optional; defaults to all *.cpp
```

### Interactive picker

```bash
make select
# or:
python3 scripts/build_with_modules.py
```

Scans [`feature-modules/`](../feature-modules/) for `module.mk`, toggles modules, picks `all` / `original` / `plus` / `mini`, then runs `make` with `FEATURE_MODULE_DIRS`. Selection is stored in `.feature-modules-selection`.

```bash
make list-modules
python3 scripts/build_with_modules.py --modules performance-fx-scenes --target plus
```

### Non-interactive build

```bash
# Default: every feature-modules/*/module.mk
make all

# Explicit paths (space-separated)
make all FEATURE_MODULE_DIRS="$HOME/src/my-tracker-module /other/repo/another-module"

# Firmware only
make all FEATURE_MODULE_DIRS=
```

---

## 4. `FeatureModule` contract

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

    virtual uint8_t patternStepFxCount() const { return 0; }
    virtual const PatternStepFxInfo* patternStepFx(uint8_t index) const { return nullptr; }
    virtual bool onPatternStepFx(uint8_t typeId, uint8_t value) { return false; }
};
```

| Hook | Called from |
| :--- | :--- |
| `init` | `cFileManager::init` after `registerFeatureModules` (then FX catalog rebuild) |
| `onWorkspaceClear` | New project / workspace clear |
| `onWorkspaceLoad` | Workspace project load |
| `onProjectOpen` / `onProjectSave` | Project open / save |
| `update` | `TrackerImpl::update` via `featureModules.updateAll()` (main thread) |
| `performancePage` | Performance Mode page cycle |
| `screenExtension` | Song column lookup / host helpers |
| `patternStepFx*` | Catalog merge at init |
| `onPatternStepFx` | Sequencer step FX playback (main thread) |

All of `update`, `onSlotStart`, and `onPatternStepFx` are **main thread only**. Do not call into the audio callback / ISR.

---

## 5. Project sidecars (`ModuleSidecar`)

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

## 6. Performance Mode page

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

`PerformancePageHost` (firmware) is the only UI/engine API a page should call:

* `setPageTitle`, `setScreenLabel`, `finishScreenLabels`
* `restoreTrackLabels`, `refreshFxRack`, `isShowingFxRack`
* `isShiftPressed`
* `captureFxScene` / `recallFxScene` (`strPerfScene` — 12 FX slots)
* `capturePatternScene` / `recallPatternScene` (`strPatternScene`)
* `captureMuteScene` / `recallMuteScene` (`strMuteScene`)

Controls:

* Desktop: **Shift + Performance** cycles registered pages in `MODULE_ORDER`, then returns to the track strip
* Tracker Mini: **Middle** cycles the same way
* Screen buttons `0`–`7` → `onScreenButton`

Typical pattern: Shift + button stores; button alone recalls. Use `isShowingFxRack()` before refreshing the rack after an external recall (e.g. song-row or pattern FX).

New top-level screens still use `ModuleBase` and the fixed `Interface::modules[]` table (slot `11` reserved for firmware).

---

## 7. Screen extensions — Song list column

Feature modules extend **existing** `ModuleBase` screens through `screenExtension()`. They do not add top-level screens.

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

One optional focusable column on the Song playlist (between pattern list and tempo). Firmware owns layout/navigation and frame geometry; the module owns meaning.

| Module API | Role |
| :--- | :--- |
| `header()` / `formatCell()` | Drawn by `cSongPlayer` (cell text ≤ `kCellTextMax` = 4) |
| `focusEncoderLabel()` | Pattern +/- label while the column is focused |
| `onEncoder` / `onClear` | Edit mode input via `SongScreenHost` |
| `onInserted` / `onRemoved` / `clearAll` | Stay aligned with Add/Delete Slot and MOD/IT import |
| `onSlotStart` | Main-thread hook when song playback enters a row |

Firmware helpers (modules do not call these; Song/FileManager/registry do):

* `pollSongListColumnSlotStarts()` — from `updateAll()`; latches `playlistPos` and calls `onSlotStart`
* `notifySongPlaylistReplaced()` — MOD/IT import clears column data via `clearAll()`

`getSongListColumn()` returns the **first** Song extension that exposes a column (`MODULE_ORDER`). Today the host supports **zero or one** column; multi-column packing is a future host change (module `SongListColumn` API stays the same).

`SongListColumn::kSongSlotCount` must match `SONG_MAX` (255).

---

## 8. Pattern Step FX exports

Modules can add entries to the pattern editor FX list (alongside Roll, Slice, Position, …) **without** editing core `fxNames` / `fxIDs` tables.

```cpp
// PatternStepFx.h
static constexpr uint8_t kPatternStepFxModuleIdBase = 128;
static constexpr uint8_t kPatternStepFxModuleIdLast = 143; // 16 slots

struct PatternStepFxInfo
{
    uint8_t typeId;       // stored in the pattern; stable across builds
    char letter;          // picker glyph, e.g. 'N'
    const char* name;     // host formats "N - Perf FX Scene"
    const char* shortName;
    const char* description;
    uint8_t minValue;
    uint8_t maxValue;
    uint8_t defaultValue;
};
```

### Rules

* `typeId` must be in **`[128, 143]`**. Builtin sequencer FX stay `0..50` — do not reuse them.
* Pick a **stable** id for your module (e.g. `128` for Perf FX Scene). Patterns store this id; changing it orphans old steps.
* `letter` must be unique vs builtins and other modules (duplicates are skipped at init).
* `onPatternStepFx` runs on the **main thread** (same family as Tempo / Break), not in `playerEngine::seqFx`.
* Return `true` if you handled the `typeId` (even if the value was out of range); otherwise `false` so another module can try.
* After `registerFeatureModules` + `initAll`, firmware rebuilds the runtime FX catalog (module entries appear **before** the hidden Break pattern entry).

### Minimal export example

```cpp
static const PatternStepFxInfo kMyFx = {
    129,              // your reserved id in 128..143
    'O',
    "My Pattern FX",
    "My FX",
    "Does something on this step.",
    0,
    100,
    0,
};

uint8_t patternStepFxCount() const override { return 1; }
const PatternStepFxInfo* patternStepFx(uint8_t i) const override
{
    return i == 0 ? &kMyFx : nullptr;
}
bool onPatternStepFx(uint8_t typeId, uint8_t value) override
{
    if (typeId != 129)
        return false;
    // ... main-thread behavior using value ...
    return true;
}
```

---

## 9. Bundled modules

| Directory | What it adds | `id()` / sidecar | `MODULE_ORDER` |
| :--- | :--- | :--- | ---: |
| [`performance-fx-scenes/`](../feature-modules/performance-fx-scenes) | Performance page **Scenes**; Song column **FX**; pattern FX **N - Perf FX Scene** (id 128, values 1–8) | `perfscenes` | 10 |
| [`pattern-launch-scenes/`](../feature-modules/pattern-launch-scenes) | Performance page **Patterns** | `patternscenes` | 20 |
| [`mute-banks/`](../feature-modules/mute-banks) | Performance page **Mutes** | `mutebanks` | 30 |

All three use only SDK headers + `ModuleSidecar` + hosts. Shift + screen button stores; screen button recalls.

---

## 10. Skeleton module

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
MODULE_ORDER       := 40
MODULE_INSTANCE    := myModule
MODULE_HEADER      := MyModule.h
MODULE_SRCS        := MyModule.cpp
```

```bash
make select
# or:
make all FEATURE_MODULE_DIRS="/absolute/path/to/my-module"
```

---

## 11. Out of scope (this version)

* SD-card executable / dynamically loaded feature modules (use `.ptfx` for Sample Editor DSP)
* Audio-callback / ISR module hooks (sample `seqFx` stays builtin-only)
* Multiple Song list columns in the host UI (API is per-column; host still takes the first)
* Dynamic Performance Mode **rack** FX id tables (`performanceFx.cpp` remains append-only)
* Letting modules create new top-level `ModuleBase` screens
* Synth engine authoring (PolySynth SDK)
