# Pattern Launch Scenes

Store and recall which pattern each track will jump to in Performance Mode.

| | |
| :--- | :--- |
| Page title | **Patterns** |
| Sidecar | `patternscenes.bin` (workspace + project) |
| Slots | 8 |
| Build order | `MODULE_ORDER` 20 |

## What it does

In Performance Mode, each track (sample and MIDI) can point at a different pattern via the track strip under the screen. This module snapshots those per-track pattern cues into eight named banks.

Recalling a bank writes the cues back and tells the sequencer to load those patterns on the tracks—useful for arrangement changes mid-set without leaving Performance Mode.

It does **not** change the Song Mode playlist. It only affects Performance’s live per-track pattern assignments.

## How to use it on the device

### Set up what you want to store

1. Enter **Performance Mode**.
2. On the normal track strip (not a module page), choose pattern numbers for each track the way you already do in Performance (screen buttons / encoder / pads—whatever you use to set `perf` pattern cues).

### Open the Patterns page

1. Cycle module pages until the title bar shows **Patterns**:
   - **Tracker / Tracker+:** hold **Shift** and press **Performance**
   - **Tracker Mini:** press **Middle**

Order with all three modules built in: Scenes → **Patterns** → Mutes → track strip.

### Store a pattern bank

1. On the **Patterns** page, hold **Shift** and press screen button **1–8**.
2. That slot fills and is named `Pat N`.
3. The bank captures the current pattern cue for every Performance track.

### Recall a pattern bank

1. On the **Patterns** page, press screen button **1–8** (without Shift).
2. Tracks get the stored pattern numbers and load them now.

Empty slots do nothing on recall.

### Active slot UI

After a recall, the title shows which bank is live (`Patterns - 2`) and that soft button uses the **Active** label state.

## On disk

| Location | File |
| :--- | :--- |
| Workspace | `Workspace/patternscenes.bin` |
| Project | `Projects/<path>/<name>/patternscenes.bin` |

## Build

```bash
make select   # enable Pattern Launch Scenes
```

Or include the folder path in `FEATURE_MODULE_DIRS` when building.
