# Performance FX Scenes

Store and recall snapshots of the Performance FX rack during a live set.

| | |
| :--- | :--- |
| Page title | **Scenes** |
| Sidecar | `perfscenes.bin` (workspace + project) |
| Slots | 8 |
| Build order | `MODULE_ORDER` 10 (first in the page cycle) |
| Song column | Scene number per playlist row |
| Pattern Step FX | `N` / **Perf FX Scene** (values 1–8) |

## What it does

In Performance Mode you have 12 FX slots (filters, sends, macros, stutter, and so on). This module saves the whole rack layout into named scene slots:

- Which FX is in each of the 12 places
- Selected value row per slot
- Four stored values per slot
- Live values currently applied

Recalling a scene restores that rack state on the armed tracks.

Scenes can also be triggered from:

- **Song Mode** playlist rows (assigned scene number)
- **Pattern Step FX** `N` (Perf FX Scene 1–8)

## How to use it on the device

### Open the Scenes page

1. Enter **Performance Mode**.
2. Cycle module pages until the title bar shows **Scenes**:
   - **Tracker / Tracker+:** hold **Shift** and press **Performance**
   - **Tracker Mini:** press **Middle**

Each press advances: Scenes → Patterns → Mutes → back to the track strip (only pages for modules you built in are shown).

### Store a scene

1. Set up the FX rack the way you want it (edit mode, pads, values).
2. On the **Scenes** page, hold **Shift** and press screen button **1–8**.
3. That slot lights as filled and is named `Scene N`.

### Recall a scene

1. On the **Scenes** page, press screen button **1–8** (without Shift).
2. The rack updates immediately and the FX display refreshes.

Empty slots do nothing on recall.

### Active slot UI

After a recall (button, Song row, or pattern Step FX), the title shows which scene is live (`Scenes · 3`) and that soft button uses the **Active** label state.

### Song Mode bindings

When this module is built in, Song Mode playlist rows can carry a scene number (`---` / none, or `1`–`8`). Starting that playlist slot recalls the matching Performance FX scene. Inserting, removing, or clearing playlist rows keeps the bindings in sync inside `perfscenes.bin`.

### Pattern Step FX

In the pattern Step FX picker, **Perf FX Scene** (`N`) appears when this module is linked. Value `1`–`8` recalls that scene when the step plays.

## On disk

| Location | File |
| :--- | :--- |
| Workspace | `Workspace/perfscenes.bin` |
| Project | `Projects/<path>/<name>/perfscenes.bin` |

Saved with the project; loaded when you open it. Older v1 files (scenes only, no song bindings) still load and upgrade on the next save.

## Build

Included by default with `make all` / `make select` when this folder is under `feature-modules/`.

```bash
make select   # toggle Performance FX Scenes on or off
```
