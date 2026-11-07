# Feature Modules

External compile-time modules for Tracker CE. Each directory is a self-contained feature that uses the Feature Module SDK (`core/modules/`) and is linked into the firmware via `FEATURE_MODULE_DIRS`.

## Bundled example

| Directory | Module | Sidecar |
| :--- | :--- | :--- |
| [`performance-fx-scenes/`](performance-fx-scenes/) | Performance FX Scenes | `perfscenes.bin` |

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
python3 scripts/build_with_modules.py --modules performance-fx-scenes --target plus
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
├── module.mk              # MODULE_NAME, MODULE_TITLE, MODULE_DESCRIPTION, ...
├── MyModule.h
└── MyModule.cpp
```

See [`docs/feature_module_sdk.md`](../docs/feature_module_sdk.md).
