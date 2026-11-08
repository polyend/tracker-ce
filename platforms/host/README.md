# Tracker CE Host Emulator

Desktop build of the instrument code (`core/`) for macOS/Linux. Use it to try firmware changes without flashing hardware.

Built with `TRACKER_MINI` so the Module Menu (hold Menu on device; toggle with **F10** or **\`** on host) works. Factory synth libraries (`synth/lib/`) are not linked.

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

Maps every Tracker Mini hardware button (plus a few direct module shortcuts).

| Key | Mini button |
| --- | --- |
| Space | Play |
| R | Rec |
| C | Copy (Shift+C pastes) |
| Shift | Shift |
| Arrows | D-pad |
| Enter | Insert / Home |
| Backspace / Delete | Delete |
| N / I / O / P | Note / Instrument / FX1 / FX2 |
| `,` `.` `;` `'` (or F9 / F11 / F12) | Function 1–4 |
| F10 or \` | Menu (toggle on host) |
| / | Middle (D-pad center / cycle option pages) |
| 1–8 | Soft keys under the screen |
| `-` / `=` | Big **−1** / **+1** |
| `[` / `]` | Big **−10** / **+10** |
| Page Up / Page Down | Jog wheel |
| F1–F8 | Song, Performance, File, Config, Sample Play, Sample Edit, Instr, Master |
| F13–F17 | Pattern, Sample Rec, Sample Load, Params, Game |
| A–Z | Pads (except C/R/N/I/O/P which are reserved above) |
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

Button names: `PLAY`, `SHIFT`, `REC`, `COPY`, `UP`, `DOWN`, `LEFT`, `RIGHT`, `ENTER`/`INSERT`, `DELETE`, `MENU`, `MIDDLE`, `FN1`–`FN4`, `NOTE`, `INSTR`, `FX1`/`VOL`, `FX2`/`FX`, `SOFT0`–`SOFT7`, `PLUS1`/`MINUS1`/`PLUS10`/`MINUS10`, `SONG`, `PERFORMANCE`, `FILE`, `CONFIG`, `PATTERN`, `SAMPLEPLAY`, `SAMPLEEDIT`, `SAMPLEREC`, `SAMPLELOAD`, `MASTER`, `PARAMS`, `GAME`.

Screenshots are written as PPM (rename `.png` requests to `.ppm`).

## Limits

- Display rasterizer covers the FT812 commands this UI uses; unknown commands are counted and printed.
- Audio timing and memory budgets still need a real device check.
- USB, FM radio, battery gauge, and the on-chip bootloader are stubs.
