# Song FX Scenes

Independent banks of Performance-engine FX type + value pairs. Edited from the **Song** FX page, assigned per playlist row via the Song **FX** column, and launched with Pattern Step FX `N`.

| | |
| :--- | :--- |
| Song column | **FX** (`-` / `1`–`8`) |
| Editor | `SongModePage` (Mini menu dots: playlist / FX; Song host only dispatches) |
| Sidecar | `songfxscenes.bin` |
| Pattern Step FX | `N` / **FX Scene** (values 1–8) |
| Performance page | none |

This is **not** Performance FX Scenes (rack snapshots). It does not share `perfscenes.bin` or call `captureFxScene` / `recallFxScene`.

## What it does

Each of 8 scenes holds up to 8 entries `{fxId, value}` chosen from the live Performance engine FX catalog (Volume, filters, sends, stutter, …).

When a song playlist row starts (or Pattern Step FX `N` fires), all Performance-engine FX are cleared first, then every filled entry in the scene is applied (`applyPerformanceFx`) with **no display refresh**.

A row with FX `-` still clears leftover FX from the previous scene. A row start and a step-1 `N` on the same moment can both fire — that is intentional.

## How to use it on the device

### Assign a scene to a song row

1. Open **Song** (playlist page).
2. Enable **Rec** (edit/record mode).
3. Focus the **FX** column (between pattern list and tempo) with Left/Right.
4. Encoder sets `-` or scene `1`–`8` for the selected row.

The FX column is only selectable (red frame) while Rec is on. It is hidden on the Song FX mode page.

### Edit a scene’s FX list

1. From the Mini Song menu, select the second mode page (FX). Insert also opens the page.
2. Soft buttons **1–8** select which scene bank you are editing (active bank highlighted).
3. The main list shows FX name + value rows for that scene:
   - **Up** / **Down**: move focus between rows
   - **Left** / **Right**: select FX type (name) or value field (frame moves)
   - **+1** / **−1** or encoder: cycle the selected field
   - **+10** / **−10**: jump by 10 (host: `]` / `[`)
   - **Delete**: clear the focused entry
4. **Insert**, or switch back to the playlist Song page, to leave the editor

If the selected playlist row already has a scene assigned, that scene opens first.

### Pattern Step FX

In the pattern FX picker, **FX Scene** (`N`) recalls scene 1–8 the same way as a song-row start.

## On disk

| Location | File |
| :--- | :--- |
| Workspace | `Workspace/songfxscenes.bin` |
| Project | `Projects/<path>/<name>/songfxscenes.bin` |

## Build

```bash
make select   # enable Song FX Scenes
```
