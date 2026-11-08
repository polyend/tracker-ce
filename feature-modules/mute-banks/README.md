# Mute Banks

Store and recall mute masks for sample and MIDI tracks.

| | |
| :--- | :--- |
| Page title | **Mutes** |
| Sidecar | `mutebanks.bin` (workspace + project) |
| Slots | 8 |
| Build order | `MODULE_ORDER` 30 (last in the default page cycle) |

## What it does

Performance Mode already lets you mute tracks (Shift + track screen buttons on the track strip). This module saves those mute states into eight banks so you can drop or bring back groups of tracks in one press—intros, breakdowns, full drops, MIDI-only sections, and so on.

Each bank stores:

- Mute on/off for the 8 sample tracks
- Mute on/off for the 8 MIDI tracks

Recalling a bank applies sample mutes through the audio engine and updates MIDI mute flags the rest of the firmware already respects.

## How to use it on the device

### Set up a mute arrangement

1. Enter **Performance Mode**.
2. On the track strip, mute/unmute tracks as usual (Shift + screen buttons on the track page).
3. Switch between sample and MIDI track pages if you need both.

### Open the Mutes page

1. Cycle module pages until the title bar shows **Mutes**:
   - **Tracker / Tracker+:** hold **Shift** and press **Performance**
   - **Tracker Mini:** press **Middle**

Order with all three modules built in: Scenes → Patterns → **Mutes** → track strip.

### Store a mute bank

1. On the **Mutes** page, hold **Shift** and press screen button **1–8**.
2. That slot fills and is named `Mute N`.

### Recall a mute bank

1. On the **Mutes** page, press screen button **1–8** (without Shift).
2. Sample and MIDI mutes jump to the stored mask.

Empty slots do nothing on recall. After you leave the page, the track strip mute colors reflect the current state.

### Active slot + live mute overview

- After a recall, the title shows which bank is live (`Mutes · 1`) and that soft button uses the **Active** label state.
- Each soft button’s secondary line shows live **Mute** / **Unmute** for the matching track on the current sample or MIDI page (`tracksPage()`).
- Muted tracks use muted label colors unless that bank is the active one.

## On disk

| Location | File |
| :--- | :--- |
| Workspace | `Workspace/mutebanks.bin` |
| Project | `Projects/<path>/<name>/mutebanks.bin` |

## Build

```bash
make select   # enable Mute Banks
```

Or include the folder path in `FEATURE_MODULE_DIRS` when building.
