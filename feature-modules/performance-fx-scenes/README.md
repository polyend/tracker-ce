# Performance FX Scenes

Store and recall snapshots of the Performance FX rack during a live set.

| | |
| :--- | :--- |
| Page title | **Scenes** |
| Sidecar | `perfscenes.bin` (workspace + project) |
| Slots | 8 |
| Build order | `MODULE_ORDER` 10 (first in the page cycle) |

Song-row FX launches and Pattern Step FX scene triggers live in the separate [`fx-list-scenes`](../fx-list-scenes/) module.

## What it does

In Performance Mode you have 12 FX slots. This module saves the whole rack layout into named scene slots:

- Which FX is in each of the 12 places
- Selected value row per slot
- Four stored values per slot
- Live values currently applied

Recalling a scene restores that rack state on the armed tracks.

## How to use it on the device

### Open the Scenes page

1. Enter **Performance Mode**.
2. Cycle module pages until the title bar shows **Scenes**:
   - **Tracker / Tracker+:** hold **Shift** and press **Performance**
   - **Tracker Mini:** press **Middle**

### Store / recall

- **Store:** Shift + screen button **1–8**
- **Recall:** screen button **1–8**

After a recall, the title shows `Scenes - N` and that soft button uses the **Active** label state.

## On disk

| Location | File |
| :--- | :--- |
| Workspace | `Workspace/perfscenes.bin` |
| Project | `Projects/<path>/<name>/perfscenes.bin` |

File version **v3** is scenes-only. Older v1/v2 files still load; **v2 song-slot bindings are ignored** (not migrated into `fxlistscenes.bin`). Reassign playlist rows in Song with [`fx-list-scenes`](../fx-list-scenes/) if you used experimental v2 row bindings on this branch.

## Build

```bash
make select   # toggle Performance FX Scenes
```
