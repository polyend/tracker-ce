# MIDI Remote Control (Tracker Mini)

This is an experimental Mini-only MIDI remote-control implementation.
**Back up your projects and configuration before installing a custom build.**
The firmware changes have not been hardware-tested yet.

## Setup

1. On the Mini select **Config > MIDI > Remote control**.
2. Select **Input: USB**, **Channel: 1**, **Profile: Launchpad X**,
   then **Enabled: On**.
3. Put the Launchpad X into **Programmer Mode** (not Note or Custom Mode).
   Connect it directly to the Mini as a USB MIDI controller.
4. The eight-by-eight pad grid is deliberately *not* mapped to remote
   commands; it retains its normal MIDI note-entry behavior.
5. To turn remote control off, set **Enabled: Off**. Normal MIDI
   functionality then operates exactly as before.

## Built-in Launchpad X controls

| Tracker action | Launchpad X Programmer Mode CC |
| --- | ---: |
| Up / Down / Left / Right | 91 / 92 / 93 / 94 |
| Select | 96 |
| Menu (hold to open) | 95 |
| Copy / Paste | 79 / 69 |
| Insert / Delete | 29 / 39 |
| Play / Stop / Record | 89 / 49 / 19 |
| Pattern / Song / Instrument | 97 / 59 / 98 |

These are **command mappings**, not an automatic Novation LED-driver.
The Launchpad X must be in Programmer Mode; the Mini does not yet switch
its modes or update LEDs through SysEx.

## Custom controller profiles and MIDI Learn

The Mini saves three user-editable profiles in an append-only, Mini-only
versioned configuration section: **Custom 1**, **Custom 2**, **Custom 3**. The built-in
**Launchpad X** profile is read-only.

To customize a controller:
1. Select one of the Custom profiles.
2. Optionally choose **Copy Launchpad preset** to initialize it.
3. Choose **Action to learn** in the menu.
4. Choose **Learn next input**, then press the corresponding controller
   control. CC or MIDI Note On is captured, and the profile is saved.
5. To remove an assignment, select its action and use
   **Clear assignment**. Use **Cancel MIDI Learn** to disarm learning.

Mappings match the configured **Input** and MIDI **Channel** (All or 1-16).
Only matching messages are consumed. All other note messages continue
to the existing sequencer and instrument playback handlers. A controller
with dedicated CC buttons and a note grid can therefore use both
simultaneously.

**Limitation:** This initial version saves three numeric Custom slots,
not externally named SD-card files. MIDI Learn provides no on-screen
"waiting" indicator yet; it arms on selection and completes on the next
matching message. Continuous-control faders trigger one action per
nonzero message, so assign momentary buttons rather than faders to
navigation. Hardware Shift combinations, MIDI LED feedback, controller
auto-detection, and generic SysEx are not implemented.

## Development notes

- Routing: `core/source/mtMidi.cpp`, `core/source/mtMidiRemote.h`
- Menu: `core/interface/configEditor/configMenu.cpp`
- Versioned config: `core/source/mtStructs.h`, `core/source/mtConfig.cpp`
- Pattern paste shortcut: `core/interface/patternEditor/fPatternEdit.cpp`

The on-device MIDI remote is opt-in and is deliberately separate from
existing Notes In, CC In, performance, and mixer mappings.

### Test checklist

- Factory firmware configuration migrates without changing existing settings.
- Remote disabled: all existing notes, CC, clock and transport unaffected.
- Launchpad X Programmer Mode: Up/Down/Left/Right and Select operate.
- Both short and held Menu buttons dismiss correctly.
- Copy and Paste operate only as appropriate in pattern edit mode.
- The Launchpad X's pad grid continues playing / recording musical notes.
- Editing custom profiles, saving settings and rebooting preserves mappings.
- CC collision: mapped controls are consumed; other CCs still reach the
  instrument and mixer logic.
- TRS MIDI remote selection and channel filtering work as configured.
- Begin MIDI Learn on Custom 1, then switch to Launchpad X and transmit a
  control: learning cancels without writing out of bounds.
- Begin MIDI Learn on Custom 1 then change the chosen action; the previous
  assignment is not overwritten.
- Hold Menu via the controller, disable Remote Control from the physical
  Mini controls, then release: the menu must not remain stuck open.
- Original Tracker and Tracker+ still build without Mini-only paste code.

## Contribution and validation notes

**Scope:** One opt-in MIDI remote-control feature, with an onboard UI,
learnable user profiles, and a Launchpad X factory preset. The existing
navigation, note-entry, and project workflows remain the defaults.

**Targets:** Mini only for the feature (`PLATFORM=IMX PRODUCT=MINI`).
Original Tracker (`PLATFORM=MK66 PRODUCT=ORIGINAL`) and Tracker+
(`PLATFORM=IMX PRODUCT=PLUS`) must still build successfully and retain
the old configuration layout. The new Mini settings live in an appended
size-prefixed configuration section rather than shifting existing fields.

**Memory:** The extra persistent configuration is approximately 116 bytes
on Mini; no large data buffers are introduced. Actual linker headroom in
`SRAM_DTC`, RAM, and `EXTERNAL_RAM` still requires a full firmware build
and inspection of the `-print-memory-usage` output.

**Build commands required before upstream submission:**

```sh
make all
make ptfx
```

`make all` includes the `.psyn` synth plugin build. Check all three
firmware outputs, memory usage, and the reference editor effects build.
There is an existing `workflow_dispatch` trigger in
`.github/workflows/build.yml`, which can be manually run against this
branch on GitHub Actions without opening a pull request.

**Hardware testing status:** No Mini, Tracker+, or Original hardware test
has been performed for this feature. USB MIDI host enumeration and
Programmer Mode reception with Launchpad X are particularly important.

**Third-party code:** None added. Launchpad CC mappings are protocol
constants, not copied third-party source.

**Upstream discussion:** The existing Polyend Backstage request for
external MIDI control covers the motivation for this feature:
https://backstage.polyend.com/t/additional-midi-cc-in-controls-external-controller-as-a-full-featured-control-surface/11478
