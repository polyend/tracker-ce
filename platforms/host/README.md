# Tracker CE Host Emulator

Desktop build of the **Original Tracker** instrument code (`core/`) for macOS/Linux. Use it to try firmware changes without flashing hardware.

Factory synth libraries (`synth/lib/`) are not linked. Tracker+ / Mini are out of scope for this host target.

## Dependencies

```bash
brew install sdl2   # macOS
```

## Build

From the repository root:

```bash
make host
```

Output: `build/tracker-host`

## Run

```bash
# Interactive window (800x480). Folder ./sdcard is the fake SD card.
./build/tracker-host --sd ./sdcard

# Record audio while running
./build/tracker-host --sd ./sdcard --wav out.wav

# Headless scripted check (virtual time, no window)
./build/tracker-host --sd ./sdcard --script platforms/host/scripts/smoke.txt --wav smoke.wav
```

Create `Projects/` and `Workspace/` under the SD folder (the host creates empty stubs on first run).

## Keys

| Key | Action |
| --- | --- |
| Space | Play |
| Shift | Shift |
| Arrows | D-pad |
| Enter | Insert |
| Backspace / Delete | Delete |
| Page Up / Page Down | Jog wheel |
| F1–F8 | Song, Performance, File, Config, Sample Play, Sample Edit, Instr, Master |
| A–Z / 0–9 | Pads |
| Esc | Quit |

## Script commands

```text
wait 300          # advance virtual time (ms)
tap PLAY          # press+release a named button
press SHIFT
release SHIFT
pad 12 tap        # pad index 0..47
enc 3             # jog wheel detents
screenshot out.ppm
quit
```

Button names: `PLAY`, `SHIFT`, `REC`, `UP`, `DOWN`, `LEFT`, `RIGHT`, `PERFORMANCE`, `SONG`, `FILE`, `CONFIG`, `ENTER`, `INSERT`, `DELETE`, `COPY`.

Screenshots are written as PPM (rename `.png` requests to `.ppm`).

## Limits

- Display rasterizer covers the FT812 commands this UI uses; unknown commands are counted and printed.
- Audio timing and memory budgets still need a real device check.
- USB, FM radio, battery gauge, and the on-chip bootloader are stubs.
