# `FM4` Synth Engine - Four-Operator FM

`FM4` is an open-source four-operator FM synthesizer for **Polyend Tracker+** and **Polyend Tracker Mini**. It is built on the [Custom Synth Engine SDK](custom_synth_sdk.md) and lives in [`core/synth/engines/FourOpFMSynthVoice.cpp`](../core/synth/engines/FourOpFMSynthVoice.cpp).

It sits beside the factory engines (`ACD`, `FAT`, `VAP`, `WTFM`, `PERC`, `DIRT`) and `CHIP`. Unlike `WTFM`, which is a two-operator wavetable design, `FM4` is classic phase-modulation FM: four sine operators, eight routing algorithms, a dedicated modulator envelope and operator feedback.

---

## Signal Flow

Operators are numbered 1-4. A modulator always feeds an operator with a **lower** number, so operator 4 is the top of every chain. Only operator 4 has self-feedback.

| # | `Algorithm` | Heard operators | Character |
| :-: | :--- | :--- | :--- |
| 0 | `4>3>2>1` | 1 | Full serial stack, the brightest and most complex |
| 1 | `(4+3)>2>1` | 1 | Two modulators into one |
| 2 | `4>3>1 2>1` | 1 | Two modulation paths into the carrier |
| 3 | `4>3 2>1` | 1, 3 | Two independent two-operator pairs |
| 4 | `4>3>2 1` | 1, 2 | A three-operator chain plus a plain sine |
| 5 | `4>1 4>2 4>3` | 1, 2, 3 | One modulator shared by three carriers |
| 6 | `4>3 2 1` | 1, 2, 3 | One FM pair plus two plain sines |
| 7 | `1+2+3+4` | 1, 2, 3, 4 | Additive, no modulation (with `Feedback`: organ and saw-like tones) |

* Carriers are level-compensated (`1 / sqrt(carriers)`), so switching algorithm does not change the loudness much.
* **Operators whose frequency would reach Nyquist are muted** instead of aliasing. High notes combined with large ratios lose their top operators rather than turning to noise.

---

## Patch Editor Sections

| Section | Parameters |
| :--- | :--- |
| **Oscillator** | `Algorithm`, `Feedback` (operator 4), `Tune` (`-24`..`+24` st), `Fine` (`-100`..`+100` ct) |
| **Modulation** | `Op1`-`Op4 Ratio` (16 steps, `x0.25` to `x12`), `Vibrato Rate`, `Vibrato Depth` (`100 %` = +/- 1 semitone) |
| **Mixer** | `Op1`-`Op4 Level`. On a **carrier** this is its volume; on a **modulator** it is the modulation depth (squared, so the low end is easy to control) |
| **Envelope** | Modulator envelope: `Mod Env Amt`, `Mod Attack`, `Mod Decay`, `Mod Sustain`. It scales the depth of every modulator over time, giving the classic FM "bright attack that settles" |
| **Amplifier** | `Attack`, `Decay`, `Sustain`, `Release` |
| **Voice** | `Glide Mode` (`Always` / `Overlap` / `Legato`), `Glide Time`, `Volume`, `LP Cutoff`, `LP Resonance` |

Notes:

* `Mod Env Amt` at `0 %` keeps the modulators static; at `100 %` they follow the envelope fully. The modulator envelope uses the amplifier `Release` when the note ends.
* `LP Cutoff` at its maximum shows `Off` and bypasses the filter completely, which saves CPU. The filter is a topology-preserving state-variable low-pass, stable at any cutoff and resonance.
* `Glide Time` is a time constant: the pitch covers about 63 % of the distance in that time.

### Default Patch (`Init`)

Algorithm `4>3>2>1`, operator 1 at `x1` full level, operator 2 at `x2` with `40 %` level, operators 3 and 4 at `0 %`. That is a simple two-operator FM voice with a plucky modulator envelope; raise operators 3 and 4 to add more complexity.

### Default Macros

| Macro | Controls |
| :--- | :--- |
| `1 Brightness` | `Op2`-`Op4 Level` |
| `2 Feedback` | `Feedback` |
| `3 Mod Envelope` | `Mod Env Amt`, `Mod Decay` (bipolar) |
| `4 Env Decay / Rel` | `Decay`, `Release` (bipolar) |
| `5 Vibrato` | `Vibrato Depth`, `Vibrato Rate` |
| `6 Filter Cutoff` | `LP Cutoff`, `LP Resonance` (bipolar) |
| Velocity | `Volume` |

---

## Implementation Notes

* **Phase modulation**: each operator is `sin(phase + modulation)`, evaluated with the SDK's `sdk::fastSine`. Modulation depth is expressed in cycles, with `100 %` level equal to 1.5 cycles (about 9.4 rad). Feedback averages the last two samples of operator 4 for stability and tops out at 0.5 cycles.
* **Cost**: four `fastSine` calls per sample per voice plus envelope and optional filter. No heap allocations and no `std::sin`/`std::pow` on the audio thread, following the [CPU and memory guidelines](custom_synth_sdk.md#7-real-time-audio-cpu--memory-guidelines).
* **Output**: mono. A fixed `-4.4 dB` output trim and a final soft clip leave headroom for eight voices.
* **Compatibility**: `FM4` is appended at the end of `Synth::kSynthTypes` and as the last `InstrumentTypeId`, so existing projects and patches keep their engine selection.
* **Badge**: `FM4` currently reuses the `WTFM` badge bitmaps in the Synth Editor and Patch Editor. A dedicated badge is listed as follow-up work in the [SDK guide](custom_synth_sdk.md#step-6-add-ui-badges--wire-into-the-tracker-ui).

## Testing Status

The DSP was checked on a development host with a throwaway test harness: pitch accuracy, ratios and tuning, stability of all eight algorithms at maximum settings, release and voice retirement, modulator envelope behavior, Nyquist muting, glide and filter response. **It has not yet been built for or listened to on hardware**; flash a Mini build and review the sound and CPU use before relying on it.
