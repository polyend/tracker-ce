# Feature Modules

External compile-time modules for Tracker CE. Each directory is a self-contained feature that uses the Feature Module SDK (`core/modules/`) and is linked into the firmware via `FEATURE_MODULE_DIRS`.

## Bundled modules

| Directory | UI | Sidecar | Order |
| :--- | :--- | :--- | ---: |
| [`performance-fx-scenes/`](performance-fx-scenes/README.md) | Perf page **Scenes** | `perfscenes.bin` | 10 |
| [`fx-list-scenes/`](fx-list-scenes/README.md) | Song FX column + editor | `fxlistscenes.bin` | 15 |
| [`pattern-launch-scenes/`](pattern-launch-scenes/README.md) | Perf page **Patterns** | `patternscenes.bin` | 20 |
| [`mute-banks/`](mute-banks/README.md) | Perf page **Mutes** | `mutebanks.bin` | 30 |

In Performance Mode, **Shift + Performance** (desktop) or **Middle** (Mini) cycles Performance pages in `MODULE_ORDER`, then returns to the track strip.

| Feature | Store / edit | Recall / apply |
| :--- | :--- | :--- |
| Scenes | Shift + screen button | Screen button — FX rack snapshot |
| FX List | Song FX column + Insert editor | Song row start / Pattern FX `N` |
| Patterns | Shift + screen button | Screen button — per-track pattern cues |
| Mutes | Shift + screen button | Screen button — sample + MIDI mute masks |

## Build

### Interactive picker (recommended)

```bash
make select
# or
python3 scripts/build_with_modules.py
```

Arrow keys / `j` `k` move, `space` toggles a module, `t` / left-right cycles the firmware target (`all` / `original` / `plus` / `mini`), `enter` builds. Selection is saved to `.feature-modules-selection` for the next run.

```bash
make list-modules
python3 scripts/build_with_modules.py --modules performance-fx-scenes pattern-launch-scenes --target plus
```

### Non-interactive

```bash
# Every module under feature-modules/ that has a module.mk
make all

# Explicit paths (local folders or other repos)
make all FEATURE_MODULE_DIRS="/path/to/my-module /path/to/another-module"

# No feature modules
make all FEATURE_MODULE_DIRS=
```

## Module layout

```text
my-module/
├── README.md              # How the module works on-device
├── module.mk              # MODULE_NAME, MODULE_TITLE, MODULE_DESCRIPTION, MODULE_ORDER, ...
├── MyModule.h
└── MyModule.cpp
```

`MODULE_ORDER` controls Performance page cycle order (lower first). Default is `100`.

See [`docs/feature_module_sdk.md`](../docs/feature_module_sdk.md).
