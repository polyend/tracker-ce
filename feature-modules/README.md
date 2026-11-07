# Feature Modules

External compile-time modules for Tracker CE. Each directory is a self-contained feature that uses the Feature Module SDK (`core/modules/`) and is linked into the firmware via `FEATURE_MODULE_DIRS`.

## Bundled example

| Directory | Module | Sidecar |
| :--- | :--- | :--- |
| [`performance-fx-scenes/`](performance-fx-scenes/) | Performance FX Scenes | `perfscenes.bin` |

## Build

Default build includes the bundled Scenes module:

```bash
make all
```

Point at any local folders or other checkouts:

```bash
make all FEATURE_MODULE_DIRS="/path/to/my-module /path/to/another-module"
```

## Module layout

```text
my-module/
├── module.mk              # MODULE_NAME, MODULE_INSTANCE, MODULE_HEADER, MODULE_SRCS
├── MyModule.h
└── MyModule.cpp
```

See [`docs/feature_module_sdk.md`](../docs/feature_module_sdk.md).
