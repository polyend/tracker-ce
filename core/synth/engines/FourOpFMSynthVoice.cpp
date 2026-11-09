#include "config.h"
#ifdef FEATURE_SYNTH

#include "FourOpFMSynthVoice.h"
#include "PolySynth/sdk/SynthVoiceSDK.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace polysynth
{

namespace
{

// Keep contiguous and zero-based: CustomSynthVoice sizes its handler table from the last value.
enum class Fm4ParamId : int
{
    // Oscillator
    kAlgorithm = 0,
    kFeedback,
    kTune,
    kFine,
    // Modulation (operator ratios, vibrato)
    kRatio1,
    kRatio2,
    kRatio3,
    kRatio4,
    kVibRate,
    kVibDepth,
    // Mixer (operator levels)
    kLevel1,
    kLevel2,
    kLevel3,
    kLevel4,
    // Envelope (modulator envelope)
    kModAmount,
    kModAttack,
    kModDecay,
    kModSustain,
    // Amplifier
    kAmpAttack,
    kAmpDecay,
    kAmpSustain,
    kAmpRelease,
    // Voice
    kPlayMode,
    kGlideTime,
    kVolume,
    kCutoff,
    kResonance,
};

constexpr int kOpCount = 4;

/**
 * Operators are numbered 1..4 for the user and stored as bits 0..3 (op1 = bit 0).
 * A modulation route always runs from a higher operator to a lower one, so evaluating
 * operators 4, 3, 2, 1 in that order needs no delay line (only op4 has self-feedback).
 */
struct Algorithm
{
    std::array<uint8_t, kOpCount> modSources; // per destination op: mask of ops that modulate it
    uint8_t carriers;                         // mask of ops that are heard
    const char* name;
};

constexpr std::array<Algorithm, 8> kAlgorithms = { {
  { { 0b0010, 0b0100, 0b1000, 0b0000 }, 0b0001, "4>3>2>1" },
  { { 0b0010, 0b1100, 0b0000, 0b0000 }, 0b0001, "(4+3)>2>1" },
  { { 0b0110, 0b0000, 0b1000, 0b0000 }, 0b0001, "4>3>1 2>1" },
  { { 0b0010, 0b0000, 0b1000, 0b0000 }, 0b0101, "4>3 2>1" },
  { { 0b0000, 0b0100, 0b1000, 0b0000 }, 0b0011, "4>3>2 1" },
  { { 0b1000, 0b1000, 0b1000, 0b0000 }, 0b0111, "4>1 4>2 4>3" },
  { { 0b0000, 0b0000, 0b1000, 0b0000 }, 0b0111, "4>3 2 1" },
  { { 0b0000, 0b0000, 0b0000, 0b0000 }, 0b1111, "1+2+3+4" },
} };

constexpr uint8_t modulatorMask(const Algorithm& alg)
{
    uint8_t mask = 0;
    for (const auto sources : alg.modSources)
    {
        mask |= sources;
    }
    return mask;
}

// Every route must run downward, and no operator may be both heard and a modulator.
constexpr bool algorithmsAreValid()
{
    for (const auto& alg : kAlgorithms)
    {
        for (int dst = 0; dst < kOpCount; ++dst)
        {
            if (alg.modSources[dst] >> (dst + 1) << (dst + 1) != alg.modSources[dst])
            {
                return false;
            }
        }
        if ((alg.carriers & modulatorMask(alg)) != 0)
        {
            return false;
        }
    }
    return true;
}
static_assert(algorithmsAreValid(), "FM4 algorithm table has an upward route or a dual-role op");

constexpr std::array<float, 16> kRatios = {
    0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 12.0f,
};

constexpr std::array<const char*, 3> kPlayModeNames = { "Always", "Overlap", "Legato" };

// Carrier loudness compensation, 1 / sqrt(number of carriers), indexed by carrier count.
constexpr std::array<float, 5> kCarrierNorm = { 0.0f, 1.0f, 0.70710678f, 0.57735027f, 0.5f };

// Modulation depth in cycles of phase at operator level 100 % (about 9.4 rad).
constexpr float kModDepthCycles = 1.5f;
// Operator 4 feedback depth in cycles at 100 %.
constexpr float kFeedbackCycles = 0.5f;
// Operators whose pitch would pass this fraction of the sample rate are muted.
constexpr float kMaxPhaseInc = 0.45f;
// Fixed output trim leaving headroom for several voices.
constexpr float kOutputTrim = 0.6f;

constexpr float kFilterBypassHz = 17999.0f;

class FourOpFMSynthVoice : public sdk::CustomSynthVoice<FourOpFMSynthVoice, Fm4ParamId>
{
  public:
    FourOpFMSynthVoice() = default;

    bool isActive() final { return ampStage != EnvStage::kIdle; }

    void onNoteOn(const InstrumentVoiceContext& /*ctx*/,
                  note_t note,
                  float /*velocity*/,
                  bool isLegatoStep) final
    {
        const float targetNote = static_cast<float>(note);
        if (!hasPlayedAnyNote || glideTimeSec <= 0.001f ||
            (playMode == PlayMode::kLegato && !isLegatoStep))
        {
            currentGlideNote = targetNote;
        }
        targetGlideNote = targetNote;
        hasPlayedAnyNote = true;

        if (!isLegatoStep || ampStage == EnvStage::kIdle || ampStage == EnvStage::kRelease)
        {
            if (ampStage == EnvStage::kIdle)
            {
                // Fresh voice: start every note from the same phase, with clean state.
                phase.fill(0.0f);
                feedback1 = 0.0f;
                feedback2 = 0.0f;
                filterIc1 = 0.0f;
                filterIc2 = 0.0f;
                ampLevel = 0.0f;
                modLevel = 0.0f;
            }
            ampStage = EnvStage::kAttack;
            modStage = EnvStage::kAttack;
        }
    }

    void onNoteOff(const InstrumentVoiceContext& /*ctx*/, note_t /*note*/) final
    {
        if (ampStage != EnvStage::kIdle)
        {
            ampStage = EnvStage::kRelease;
            modStage = EnvStage::kRelease;
        }
    }

    void renderVoiceAudio(const InstrumentVoiceContext& /*ctx*/,
                          [[maybe_unused]] std::optional<AudioBufferStereoRef> inputBuffer,
                          AudioBufferStereoRef outputBuffer,
                          std::span<AudioBufferRef> /*auxOutBuffer*/) final
    {
        auto out = outputBuffer[0];
        if (ampStage == EnvStage::kIdle)
        {
            std::fill(out.begin(), out.end(), 0.0f);
            return;
        }

        constexpr float sr = sdk::kSampleRate;
        constexpr float invSr = 1.0f / sr;
        const Algorithm& alg = kAlgorithms[algorithmIdx];
        const uint8_t modMask = modulatorMask(alg);

        // Glide, stepped once per block.
        if (glideTimeSec > 0.001f)
        {
            const float blockCoef =
              1.0f -
              sdk::fastPow2(-1.442695f * static_cast<float>(out.size()) / (glideTimeSec * sr));
            currentGlideNote += (targetGlideNote - currentGlideNote) * blockCoef;
        }
        else
        {
            currentGlideNote = targetGlideNote;
        }

        // Pitch and per-operator gain and phase increment, once per block.
        const float baseFreq =
          sdk::noteToFreq(kNoteA4,
                          currentGlideNote - static_cast<float>(kNoteA4) + tuneSemitones +
                            fineCents * 0.01f + pitchBendSemitones);
        const float carrierNorm = kCarrierNorm[__builtin_popcount(alg.carriers)];

        std::array<float, kOpCount> baseGain{};
        std::array<float, kOpCount> inc{};
        for (int op = 0; op < kOpCount; ++op)
        {
            const float opInc = baseFreq * kRatios[ratioIdx[op]] * invSr;
            if (opInc >= kMaxPhaseInc)
            {
                continue; // muted above Nyquist, gain and increment stay 0
            }
            inc[op] = opInc;
            const float lvl = level[op];
            baseGain[op] = ((modMask >> op) & 1) ? kModDepthCycles * lvl * lvl : lvl * carrierNorm;
        }

        // Envelope increments per sample.
        const float ampAttackInc = (ampAttackSec > 0.001f) ? 1.0f / (ampAttackSec * sr) : 1.0f;
        const float ampDecayDec =
          (ampDecaySec > 0.002f) ? (1.0f - ampSustainLevel) / (ampDecaySec * sr) : 1.0f;
        const float releaseDec = (ampReleaseSec > 0.002f) ? 1.0f / (ampReleaseSec * sr) : 1.0f;
        const float modAttackInc = (modAttackSec > 0.001f) ? 1.0f / (modAttackSec * sr) : 1.0f;
        const float modDecayDec =
          (modDecaySec > 0.002f) ? (1.0f - modSustainLevel) / (modDecaySec * sr) : 1.0f;

        // Vibrato.
        const bool vibActive = vibDepthSemitones > 0.001f;
        const float vibInc = vibRateHz * invSr;

        // Low-pass filter (topology-preserving SVF, stable at any cutoff).
        const bool filterOn = filterCutoffHz < kFilterBypassHz;
        float a1 = 0.0f;
        float a2 = 0.0f;
        float a3 = 0.0f;
        if (filterOn)
        {
            const float fc = std::clamp(filterCutoffHz, 20.0f, 0.45f * sr);
            const float g = std::tan(std::numbers::pi_v<float> * fc * invSr);
            const float k = 2.0f - 1.9f * filterResonance;
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }
        else
        {
            filterIc1 = 0.0f;
            filterIc2 = 0.0f;
        }

        for (auto& sample : out)
        {
            advanceEnvelope(ampStage,
                            ampLevel,
                            ampSustainLevel,
                            ampAttackInc,
                            ampDecayDec,
                            releaseDec,
                            true);
            if (ampStage == EnvStage::kIdle)
            {
                sample = 0.0f;
                continue;
            }
            advanceEnvelope(modStage,
                            modLevel,
                            modSustainLevel,
                            modAttackInc,
                            modDecayDec,
                            releaseDec,
                            false);
            const float modScale = (1.0f - modAmount) + modAmount * modLevel;

            float vibMult = 1.0f;
            if (vibActive)
            {
                vibPhase += vibInc;
                if (vibPhase >= 1.0f)
                {
                    vibPhase -= 1.0f;
                }
                vibMult = sdk::fastPow2(vibDepthSemitones * sdk::fastSine(vibPhase) *
                                        (1.0f / static_cast<float>(kOneOctaveNotes)));
            }

            std::array<float, kOpCount> opOut{};
            float mix = 0.0f;
            for (int op = kOpCount - 1; op >= 0; --op)
            {
                if (baseGain[op] > 0.0f)
                {
                    float pm = 0.0f;
                    const uint8_t sources = alg.modSources[op];
                    for (int src = op + 1; src < kOpCount; ++src)
                    {
                        if ((sources >> src) & 1)
                        {
                            pm += opOut[src];
                        }
                    }
                    if (op == kOpCount - 1)
                    {
                        pm += feedbackCycles * 0.5f * (feedback1 + feedback2);
                    }

                    const float raw = sdk::fastSine(phase[op] + pm);
                    if (op == kOpCount - 1)
                    {
                        feedback2 = feedback1;
                        feedback1 = raw;
                    }

                    const float gain = ((modMask >> op) & 1) ? baseGain[op] * modScale : baseGain[op];
                    opOut[op] = raw * gain;
                    if ((alg.carriers >> op) & 1)
                    {
                        mix += opOut[op];
                    }
                }
                else if (op == kOpCount - 1)
                {
                    feedback1 = 0.0f;
                    feedback2 = 0.0f;
                }

                phase[op] += inc[op] * vibMult;
                if (phase[op] >= 1.0f)
                {
                    phase[op] -= 1.0f;
                }
            }

            float x = mix * ampLevel;
            if (filterOn)
            {
                const float v3 = x - filterIc2;
                const float v1 = a1 * filterIc1 + a2 * v3;
                const float v2 = filterIc2 + a2 * filterIc1 + a3 * v3;
                filterIc1 = 2.0f * v1 - filterIc1;
                filterIc2 = 2.0f * v2 - filterIc2;
                x = v2;
            }
            sample = x * volumeMult * kOutputTrim;
        }

        sdk::saturateSoftClip(out, 0.0f);
    }

    static void createMetadata(const Patch& /*patch*/, Patch::Metadata& metadata)
    {
        using namespace polysynth::sdk;

        metadata.params.resize(enumutil::EnumMax<Fm4ParamId>::value + 1);
        metadata.params.shrink_to_fit();

        if (paramHandlers.empty())
        {
            paramHandlers.resize(enumutil::EnumMax<Fm4ParamId>::value + 1);
        }

        ParamHelper<FourOpFMSynthVoice> helper(metadata, paramHandlers);

        helper.addParam(
          *Fm4ParamId::kAlgorithm,
          {
            .displayName = "Algorithm",
            .defaultValue = 0,
            .minValue = 0,
            .maxValue = static_cast<Patch::paramvalue_short_t>(kAlgorithms.size() - 1),
            .isEnum = true,
            .midiCC = *MidiCCs::kOscParam0,
            .valueFormatter =
              [](const Patch& /*patch*/, Patch::paramvalue_t value)
            {
                const int idx =
                  std::clamp<int>(static_cast<int>(std::lround(value)), 0, kAlgorithms.size() - 1);
                return std::string(kAlgorithms[idx].name);
            },
          },
          [](auto& voice, Patch::optional_paramvalue_t val)
          {
              voice.algorithmIdx =
                std::clamp<int>(static_cast<int>(std::lround(*val)), 0, kAlgorithms.size() - 1);
          });

        helper.addParam(
          *Fm4ParamId::kFeedback,
          {
            .displayName = "Feedback",
            .defaultValue = 0.0f,
            .minValue = 0.0f,
            .maxValue = 1.0f,
            .midiCC = *MidiCCs::kOscParam0 + 1,
            .valueFormatter = createPercentFormatter(),
          },
          [](auto& voice, Patch::optional_paramvalue_t val)
          { voice.feedbackCycles = *val * kFeedbackCycles; });

        helper.addParam(
          *Fm4ParamId::kTune,
          {
            .displayName = "Tune",
            .defaultValue = 0.0f,
            .minValue = -24,
            .maxValue = 24,
            .midiCC = *MidiCCs::kOscParam0 + 10,
            .valueFormatter = createSemitoneFormatter(),
          },
          [](auto& voice, Patch::optional_paramvalue_t val)
          { voice.tuneSemitones = std::round(*val); });

        helper.addParam(
          *Fm4ParamId::kFine,
          {
            .displayName = "Fine",
            .defaultValue = 0.0f,
            .minValue = -100,
            .maxValue = 100,
            .midiCC = *MidiCCs::kOscParam0 + 11,
            .valueFormatter = createCentsFormatter(),
          },
          [](auto& voice, Patch::optional_paramvalue_t val) { voice.fineCents = *val; });

        // Operator ratios: a 16-step table from x0.25 to x12.
        const auto ratioFormatter = [](const Patch& /*patch*/, Patch::paramvalue_t value)
        {
            const int idx = std::clamp<int>(static_cast<int>(std::lround(value)), 0, kRatios.size() - 1);
            return fmt::format("x{:g}", kRatios[idx]);
        };
        const auto ratioMax = static_cast<Patch::paramvalue_short_t>(kRatios.size() - 1);

        helper.addParam(
          *Fm4ParamId::kRatio1,
          {
            .displayName = "Op1 Ratio",
            .defaultValue = 3, // x1
            .minValue = 0,
            .maxValue = ratioMax,
            .isEnum = true,
            .midiCC = *MidiCCs::kOscParam0 + 2,
            .valueFormatter = ratioFormatter,
          },
          [](auto& voice, Patch::optional_paramvalue_t val)
          {
              voice.ratioIdx[0] =
                std::clamp<int>(static_cast<int>(std::lround(*val)), 0, kRatios.size() - 1);
          });

        helper.addParam(
          *Fm4ParamId::kRatio2,
          {
            .displayName = "Op2 Ratio",
            .defaultValue = 5, // x2
            .minValue = 0,
            .maxValue = ratioMax,
            .isEnum = true,
            .midiCC = *MidiCCs::kOscParam0 + 3,
            .valueFormatter = ratioFormatter,
          },
          [](auto& voice, Patch::optional_paramvalue_t val)
          {
              voice.ratioIdx[1] =
                std::clamp<int>(static_cast<int>(std::lround(*val)), 0, kRatios.size() - 1);
          });

        helper.addParam(
          *Fm4ParamId::kRatio3,
          {
            .displayName = "Op3 Ratio",
            .defaultValue = 3, // x1
            .minValue = 0,
            .maxValue = ratioMax,
            .isEnum = true,
            .midiCC = *MidiCCs::kOscParam0 + 4,
            .valueFormatter = ratioFormatter,
          },
          [](auto& voice, Patch::optional_paramvalue_t val)
          {
              voice.ratioIdx[2] =
                std::clamp<int>(static_cast<int>(std::lround(*val)), 0, kRatios.size() - 1);
          });

        helper.addParam(
          *Fm4ParamId::kRatio4,
          {
            .displayName = "Op4 Ratio",
            .defaultValue = 7, // x3
            .minValue = 0,
            .maxValue = ratioMax,
            .isEnum = true,
            .midiCC = *MidiCCs::kOscParam0 + 5,
            .valueFormatter = ratioFormatter,
          },
          [](auto& voice, Patch::optional_paramvalue_t val)
          {
              voice.ratioIdx[3] =
                std::clamp<int>(static_cast<int>(std::lround(*val)), 0, kRatios.size() - 1);
          });

        helper.addParam(
          *Fm4ParamId::kVibRate,
          {
            .displayName = "Vibrato Rate",
            .defaultValue = 5.5f,
            .minValue = 0.5f,
            .maxValue = 20.0f,
            .editCurve = Patch::Metadata::Param::Curve::kExponential,
            .midiCC = *MidiCCs::kLfo1Frequency,
            .valueFormatter = createFrequencyFormatter(),
          },
          [](auto& voice, Patch::optional_paramvalue_t val) { voice.vibRateHz = *val; });

        helper.addParam(
          *Fm4ParamId::kVibDepth,
          {
            .displayName = "Vibrato Depth",
            .defaultValue = 0.0f,
            .minValue = 0.0f,
            .maxValue = 1.0f,
            .valueFormatter = createPercentFormatter(),
          },
          [](auto& voice, Patch::optional_paramvalue_t val)
          { voice.vibDepthSemitones = *val; }); // 100 % = +/- 1 semitone

        helper.addParam(
          *Fm4ParamId::kLevel1,
          {
            .displayName = "Op1 Level",
            .defaultValue = 1.0f,
            .minValue = 0.0f,
            .maxValue = 1.0f,
            .midiCC = *MidiCCs::kOscParam0 + 6,
            .valueFormatter = createPercentFormatter(),
          },
          [](auto& voice, Patch::optional_paramvalue_t val) { voice.level[0] = *val; });

        helper.addParam(
          *Fm4ParamId::kLevel2,
          {
            .displayName = "Op2 Level",
            .defaultValue = 0.4f,
            .minValue = 0.0f,
            .maxValue = 1.0f,
            .midiCC = *MidiCCs::kOscParam0 + 7,
            .valueFormatter = createPercentFormatter(),
          },
          [](auto& voice, Patch::optional_paramvalue_t val) { voice.level[1] = *val; });

        helper.addParam(
          *Fm4ParamId::kLevel3,
          {
            .displayName = "Op3 Level",
            .defaultValue = 0.0f,
            .minValue = 0.0f,
            .maxValue = 1.0f,
            .midiCC = *MidiCCs::kOscParam0 + 8,
            .valueFormatter = createPercentFormatter(),
          },
          [](auto& voice, Patch::optional_paramvalue_t val) { voice.level[2] = *val; });

        helper.addParam(
          *Fm4ParamId::kLevel4,
          {
            .displayName = "Op4 Level",
            .defaultValue = 0.0f,
            .minValue = 0.0f,
            .maxValue = 1.0f,
            .midiCC = *MidiCCs::kOscParam0 + 9,
            .valueFormatter = createPercentFormatter(),
          },
          [](auto& voice, Patch::optional_paramvalue_t val) { voice.level[3] = *val; });

        helper.addParam(
          *Fm4ParamId::kModAmount,
          {
            .displayName = "Mod Env Amt",
            .defaultValue = 0.7f,
            .minValue = 0.0f,
            .maxValue = 1.0f,
            .valueFormatter = createPercentFormatter(),
          },
          [](auto& voice, Patch::optional_paramvalue_t val) { voice.modAmount = *val; });

        helper.addParam(
          *Fm4ParamId::kModAttack,
          {
            .displayName = "Mod Attack",
            .defaultValue = 0.0f,
            .minValue = 0.0f,
            .maxValue = 5.0f,
            .editCurve = Patch::Metadata::Param::Curve::kExponential,
            .midiCC = *MidiCCs::kUserEnv1Attack,
            .valueFormatter = createTimeFormatter(),
          },
          [](auto& voice, Patch::optional_paramvalue_t val) { voice.modAttackSec = *val; });

        helper.addParam(
          *Fm4ParamId::kModDecay,
          {
            .displayName = "Mod Decay",
            .defaultValue = 0.3f,
            .minValue = 0.01f,
            .maxValue = 5.0f,
            .editCurve = Patch::Metadata::Param::Curve::kExponential,
            .midiCC = *MidiCCs::kUserEnv1Decay,
            .valueFormatter = createTimeFormatter(),
          },
          [](auto& voice, Patch::optional_paramvalue_t val) { voice.modDecaySec = *val; });

        helper.addParam(
          *Fm4ParamId::kModSustain,
          {
            .displayName = "Mod Sustain",
            .defaultValue = 0.2f,
            .minValue = 0.0f,
            .maxValue = 1.0f,
            .midiCC = *MidiCCs::kUserEnv1Sustain,
            .valueFormatter = createPercentFormatter(),
          },
          [](auto& voice, Patch::optional_paramvalue_t val) { voice.modSustainLevel = *val; });

        helper.addParam(
          *Fm4ParamId::kAmpAttack,
          {
            .displayName = "Attack",
            .defaultValue = 0.002f,
            .minValue = 0.0f,
            .maxValue = 5.0f,
            .editCurve = Patch::Metadata::Param::Curve::kExponential,
            .midiCC = *MidiCCs::kAmpEnvAttack,
            .valueFormatter = createTimeFormatter(),
          },
          [](auto& voice, Patch::optional_paramvalue_t val) { voice.ampAttackSec = *val; });

        helper.addParam(
          *Fm4ParamId::kAmpDecay,
          {
            .displayName = "Decay",
            .defaultValue = 0.5f,
            .minValue = 0.01f,
            .maxValue = 5.0f,
            .editCurve = Patch::Metadata::Param::Curve::kExponential,
            .midiCC = *MidiCCs::kAmpEnvDecay,
            .valueFormatter = createTimeFormatter(),
          },
          [](auto& voice, Patch::optional_paramvalue_t val) { voice.ampDecaySec = *val; });

        helper.addParam(
          *Fm4ParamId::kAmpSustain,
          {
            .displayName = "Sustain",
            .defaultValue = 0.6f,
            .minValue = 0.0f,
            .maxValue = 1.0f,
            .midiCC = *MidiCCs::kAmpEnvSustain,
            .valueFormatter = createPercentFormatter(),
          },
          [](auto& voice, Patch::optional_paramvalue_t val) { voice.ampSustainLevel = *val; });

        helper.addParam(
          *Fm4ParamId::kAmpRelease,
          {
            .displayName = "Release",
            .defaultValue = 0.25f,
            .minValue = 0.01f,
            .maxValue = 5.0f,
            .editCurve = Patch::Metadata::Param::Curve::kExponential,
            .midiCC = *MidiCCs::kAmpEnvRelease,
            .valueFormatter = createTimeFormatter(),
          },
          [](auto& voice, Patch::optional_paramvalue_t val) { voice.ampReleaseSec = *val; });

        helper.addParam(
          *Fm4ParamId::kPlayMode,
          {
            .displayName = "Glide Mode",
            .defaultValue = 0,
            .minValue = 0,
            .maxValue = 2,
            .isEnum = true,
            .valueFormatter =
              [](const Patch& /*patch*/, Patch::paramvalue_t value)
            {
                return std::string(
                  kPlayModeNames.at(std::clamp<int>(static_cast<int>(std::lround(value)), 0, 2)));
            },
          },
          [](auto& voice, Patch::optional_paramvalue_t val)
          {
              voice.playMode =
                static_cast<PlayMode>(std::clamp<int>(static_cast<int>(std::lround(*val)), 0, 2));
          });

        helper.addParam(
          *Fm4ParamId::kGlideTime,
          {
            .displayName = "Glide Time",
            .defaultValue = 0.0f,
            .minValue = 0.0f,
            .maxValue = 2.0f,
            .editCurve = Patch::Metadata::Param::Curve::kExponential,
            .valueFormatter = createTimeFormatter(),
          },
          [](auto& voice, Patch::optional_paramvalue_t val) { voice.glideTimeSec = *val; });

        helper.addParam(
          *Fm4ParamId::kVolume,
          {
            .displayName = "Volume",
            .defaultValue = 1.0f,
            .minValue = 0.0f,
            .maxValue = 2.0f,
            .macroOperation = Patch::Metadata::Param::MacroOperation::kMult,
            .valueFormatter = createPercentFormatter(),
          },
          [](auto& voice, Patch::optional_paramvalue_t val) { voice.volumeMult = *val; });

        helper.addParam(
          *Fm4ParamId::kCutoff,
          {
            .displayName = "LP Cutoff",
            .defaultValue = 18000.0f, // fully open: filter bypassed
            .minValue = 80.0f,
            .maxValue = 18000.0f,
            .editCurve = Patch::Metadata::Param::Curve::kExponentialSteep,
            .midiCC = *MidiCCs::kFilterCutoff,
            .valueFormatter = createFrequencyFormatter(18000.0f),
          },
          [](auto& voice, Patch::optional_paramvalue_t val) { voice.filterCutoffHz = *val; });

        helper.addParam(
          *Fm4ParamId::kResonance,
          {
            .displayName = "LP Resonance",
            .defaultValue = 0.15f,
            .minValue = 0.0f,
            .maxValue = 1.0f,
            .midiCC = *MidiCCs::kFilterResonance,
            .valueFormatter = createPercentFormatter(),
          },
          [](auto& voice, Patch::optional_paramvalue_t val) { voice.filterResonance = *val; });

        // Section names are matched by substring to pick the tile icon in the Patch Editor.
        metadata.layoutFlat = Patch::Metadata::EditorLayout{
            .sections = {
              {
                .name = "Oscillator",
                .shortName = "Osc",
                .elements = {
                  { .type = EditorElement::Type::kParam, .id = *Fm4ParamId::kAlgorithm },
                  { .type = EditorElement::Type::kParam, .id = *Fm4ParamId::kFeedback },
                  { .type = EditorElement::Type::kParam, .id = *Fm4ParamId::kTune },
                  { .type = EditorElement::Type::kParam, .id = *Fm4ParamId::kFine },
                },
              },
              {
                .name = "Modulation",
                .shortName = "Mod",
                .elements = {
                  { .type = EditorElement::Type::kParam, .id = *Fm4ParamId::kRatio1 },
                  { .type = EditorElement::Type::kParam, .id = *Fm4ParamId::kRatio2 },
                  { .type = EditorElement::Type::kParam, .id = *Fm4ParamId::kRatio3 },
                  { .type = EditorElement::Type::kParam, .id = *Fm4ParamId::kRatio4 },
                  { .type = EditorElement::Type::kParam, .id = *Fm4ParamId::kVibRate },
                  { .type = EditorElement::Type::kParam, .id = *Fm4ParamId::kVibDepth },
                },
              },
              {
                .name = "Mixer",
                .shortName = "Mixer",
                .elements = {
                  { .type = EditorElement::Type::kParam, .id = *Fm4ParamId::kLevel1 },
                  { .type = EditorElement::Type::kParam, .id = *Fm4ParamId::kLevel2 },
                  { .type = EditorElement::Type::kParam, .id = *Fm4ParamId::kLevel3 },
                  { .type = EditorElement::Type::kParam, .id = *Fm4ParamId::kLevel4 },
                },
              },
              {
                .name = "Envelope",
                .shortName = "Env",
                .elements = {
                  { .type = EditorElement::Type::kParam, .id = *Fm4ParamId::kModAmount },
                  { .type = EditorElement::Type::kParam, .id = *Fm4ParamId::kModAttack },
                  { .type = EditorElement::Type::kParam, .id = *Fm4ParamId::kModDecay },
                  { .type = EditorElement::Type::kParam, .id = *Fm4ParamId::kModSustain },
                },
              },
              {
                .name = "Amplifier",
                .shortName = "Amp",
                .elements = {
                  { .type = EditorElement::Type::kParam, .id = *Fm4ParamId::kAmpAttack },
                  { .type = EditorElement::Type::kParam, .id = *Fm4ParamId::kAmpDecay },
                  { .type = EditorElement::Type::kParam, .id = *Fm4ParamId::kAmpSustain },
                  { .type = EditorElement::Type::kParam, .id = *Fm4ParamId::kAmpRelease },
                },
              },
              {
                .name = "Voice",
                .shortName = "Voice",
                .elements = {
                  { .type = EditorElement::Type::kParam, .id = *Fm4ParamId::kPlayMode },
                  { .type = EditorElement::Type::kParam, .id = *Fm4ParamId::kGlideTime },
                  { .type = EditorElement::Type::kParam, .id = *Fm4ParamId::kVolume },
                  { .type = EditorElement::Type::kParam, .id = *Fm4ParamId::kCutoff },
                  { .type = EditorElement::Type::kParam, .id = *Fm4ParamId::kResonance },
                },
              },
            },
        };

        metadata.layout23 = metadata.layoutFlat;

        metadata.initDefaultMacros = [](auto& macros)
        {
            macros[0] = Patch::Metadata::Macro{
                .displayName = "Brightness",
                .modifiers = {
                  { *Fm4ParamId::kLevel2, 0.6f },
                  { *Fm4ParamId::kLevel3, 0.5f },
                  { *Fm4ParamId::kLevel4, 0.4f },
                },
                .bipolar = false,
            };
            macros[1] = Patch::Metadata::Macro{
                .displayName = "Feedback",
                .modifiers = {
                  { *Fm4ParamId::kFeedback, 1.0f },
                },
                .bipolar = false,
            };
            macros[2] = Patch::Metadata::Macro{
                .displayName = "Mod Envelope",
                .modifiers = {
                  { *Fm4ParamId::kModAmount, 1.0f },
                  { *Fm4ParamId::kModDecay, 0.6f },
                },
                .bipolar = true,
            };
            macros[3] = Patch::Metadata::Macro{
                .displayName = "Env Decay / Rel",
                .modifiers = {
                  { *Fm4ParamId::kAmpDecay, 0.8f },
                  { *Fm4ParamId::kAmpRelease, 0.6f },
                },
                .bipolar = true,
            };
            macros[4] = Patch::Metadata::Macro{
                .displayName = "Vibrato",
                .modifiers = {
                  { *Fm4ParamId::kVibDepth, 1.0f },
                  { *Fm4ParamId::kVibRate, 0.3f },
                },
                .bipolar = false,
            };
            macros[5] = Patch::Metadata::Macro{
                .displayName = "Filter Cutoff",
                .modifiers = {
                  { *Fm4ParamId::kCutoff, 1.0f },
                  { *Fm4ParamId::kResonance, 0.3f },
                },
                .bipolar = true,
            };

            macros[*Patch::SystemMacroId::kVelocity].modifiers = {
                { *Fm4ParamId::kVolume, 1.0f },
            };
        };

        metadata.instrumentTypeId = static_cast<int>(InstrumentTypeId::kFourOpFMSynth);
        metadata.outputChannels = Patch::Metadata::OutputChannels::kMono;
    }

  private:
    enum class EnvStage : uint8_t
    {
        kIdle = 0,
        kAttack,
        kDecay,
        kSustain,
        kRelease,
    };

    // One ADSR step. `retireVoice` lets the amplitude envelope end the voice at the end of release.
    static void advanceEnvelope(EnvStage& stage,
                                float& level,
                                float sustain,
                                float attackInc,
                                float decayDec,
                                float releaseDec,
                                bool retireVoice)
    {
        switch (stage)
        {
            case EnvStage::kAttack:
                level += attackInc;
                if (level >= 1.0f)
                {
                    level = 1.0f;
                    stage = EnvStage::kDecay;
                }
                break;
            case EnvStage::kDecay:
                level -= decayDec;
                if (level <= sustain)
                {
                    level = sustain;
                    stage = EnvStage::kSustain;
                }
                break;
            case EnvStage::kSustain:
                level = sustain;
                break;
            case EnvStage::kRelease:
                level -= releaseDec;
                if (level <= 0.0f)
                {
                    level = 0.0f;
                    if (retireVoice)
                    {
                        stage = EnvStage::kIdle;
                    }
                }
                break;
            case EnvStage::kIdle:
                break;
        }
    }

    // Parameters (written from onParamChange).
    int algorithmIdx = 0;
    float feedbackCycles = 0.0f;
    float tuneSemitones = 0.0f;
    float fineCents = 0.0f;
    std::array<int, kOpCount> ratioIdx = { 3, 5, 3, 7 };
    std::array<float, kOpCount> level = { 1.0f, 0.4f, 0.0f, 0.0f };
    float vibRateHz = 5.5f;
    float vibDepthSemitones = 0.0f;

    float modAmount = 0.7f;
    float modAttackSec = 0.0f;
    float modDecaySec = 0.3f;
    float modSustainLevel = 0.2f;

    float ampAttackSec = 0.002f;
    float ampDecaySec = 0.5f;
    float ampSustainLevel = 0.6f;
    float ampReleaseSec = 0.25f;

    float glideTimeSec = 0.0f;
    float volumeMult = 1.0f;
    float filterCutoffHz = 18000.0f;
    float filterResonance = 0.15f;

    // Voice state.
    EnvStage ampStage = EnvStage::kIdle;
    EnvStage modStage = EnvStage::kIdle;
    float ampLevel = 0.0f;
    float modLevel = 0.0f;
    bool hasPlayedAnyNote = false;
    float currentGlideNote = 60.0f;
    float targetGlideNote = 60.0f;

    std::array<float, kOpCount> phase = {};
    float feedback1 = 0.0f;
    float feedback2 = 0.0f;
    float vibPhase = 0.0f;
    float filterIc1 = 0.0f;
    float filterIc2 = 0.0f;
};

} // namespace

std::unique_ptr<InstrumentType> createFourOpFMSynth()
{
    return std::make_unique<InstrumentType>(
      InstrumentType{ .name = "FM4",
                      .createMetadata = &FourOpFMSynthVoice::createMetadata,
                      .createVoice = [](int /*voiceIdx*/) -> std::unique_ptr<InstrumentVoice>
                      { return std::make_unique<FourOpFMSynthVoice>(); } });
}

} // namespace polysynth

#endif
