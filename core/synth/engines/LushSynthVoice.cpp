#include "config.h"
#ifdef FEATURE_SYNTH

#include "LushSynthVoice.h"
#include "PolySynth/sdk/SynthVoiceSDK.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace polysynth
{

namespace
{

enum class LushParamId : int
{
    kWave = 0,
    kDetune,
    kDrift,
    kSub,
    kChorusMix,
    kChorusRate,
    kVibRate,
    kVibDepth,
    kAmpAttack,
    kAmpDecay,
    kAmpSustain,
    kAmpRelease,
    kFilterCutoff,
    kFilterResonance,
    kFilterEnvAmount,
    kWidth,
    kGlideTime,
    kGlideMode,
    kVolume,
};

// enumutil::EnumMax scans __PRETTY_FUNCTION__. GCC, which builds the firmware, reports 18.
#ifndef __clang__
static_assert(enumutil::EnumMax<LushParamId>::value == static_cast<int>(LushParamId::kVolume),
              "LushParamId must stay contiguous from zero");
#endif

enum class LushWave : int
{
    kSaw = 0,
    kPulse = 1,
    kSawPulse = 2,
};

static constexpr std::array<const char*, 3> kWaveNames = { "Saw", "Pulse", "Saw + Pulse" };
static constexpr std::array<const char*, 3> kGlideNames = { "Always", "Overlap", "Legato" };

static constexpr int kChorusLength = 1024;
static constexpr float kChorusBaseSamples = 700.0f;
static constexpr float kChorusDepthSamples = 220.0f;
static constexpr float kPulseDuty = 0.35f;

inline float polyBlep(float t, float dt)
{
    if (dt <= 0.0f)
    {
        return 0.0f;
    }
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

inline float wrapPhase(float x)
{
    return (x >= 1.0f) ? x - 1.0f : x;
}

// Knob time is the time to cover about 99% of the remaining envelope gap.
inline float envelopeCoef(float timeSec, float sampleRate)
{
    if (timeSec <= 0.001f)
    {
        return 1.0f;
    }
    const float steps = timeSec * sampleRate;
    return 1.0f - sdk::fastPow2(-6.643856f / steps);
}

inline float oscDt(float centerFreq, float semi, float invSr)
{
    const float freq = std::clamp(centerFreq * sdk::detuneMult(semi), 20.0f, 14000.0f);
    return std::min(freq * invSr, 0.45f);
}

inline float renderWave(float& phase, float dt, LushWave wave)
{
    phase += dt;
    if (phase >= 1.0f)
    {
        phase -= 1.0f;
    }

    const float saw = (2.0f * phase - 1.0f) - polyBlep(phase, dt);
    if (wave == LushWave::kSaw)
    {
        return saw;
    }

    float pulse = (phase < kPulseDuty) ? 1.0f : -1.0f;
    pulse += polyBlep(phase, dt);
    pulse -= polyBlep(wrapPhase(phase - kPulseDuty + 1.0f), dt);
    pulse -= (2.0f * kPulseDuty - 1.0f);
    if (wave == LushWave::kPulse)
    {
        return pulse;
    }
    return 0.5f * (saw + pulse);
}

class LushSynthVoice : public sdk::CustomSynthVoice<LushSynthVoice, LushParamId>
{
  public:
    LushSynthVoice() = default;

    bool isActive() final { return envStage != EnvStage::kIdle; }

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

        if (!isLegatoStep || envStage == EnvStage::kIdle || envStage == EnvStage::kRelease)
        {
            const bool wasIdle = envStage == EnvStage::kIdle;
            envStage = EnvStage::kAttack;
            if (wasIdle)
            {
                svfLowL = 0.0f;
                svfBandL = 0.0f;
                svfLowR = 0.0f;
                svfBandR = 0.0f;
            }
        }
    }

    void onNoteOff(const InstrumentVoiceContext& /*ctx*/, note_t /*note*/) final
    {
        if (envStage != EnvStage::kIdle)
        {
            envStage = EnvStage::kRelease;
        }
    }

    void renderVoiceAudio(const InstrumentVoiceContext& /*ctx*/,
                          [[maybe_unused]] std::optional<AudioBufferStereoRef> inputBuffer,
                          AudioBufferStereoRef outputBuffer,
                          std::span<AudioBufferRef> /*auxOutBuffer*/) final
    {
        auto left = outputBuffer[0];
        auto right = outputBuffer[1];
        if (envStage == EnvStage::kIdle)
        {
            std::fill(left.begin(), left.end(), 0.0f);
            std::fill(right.begin(), right.end(), 0.0f);
            return;
        }

        const float sr = sdk::kSampleRate;
        const float invSr = 1.0f / sr;
        const float attackCoef = envelopeCoef(ampAttackSec, sr);
        const float decayCoef = envelopeCoef(ampDecaySec, sr);
        const float releaseCoef = envelopeCoef(ampReleaseSec, sr);
        const float glideStep = (glideTimeSec > 0.001f) ? (1.0f / (glideTimeSec * sr)) : 1.0f;

        const float detuneSemi = detuneCents * 0.01f;
        const float driftScale = driftAmount * 0.15f;
        const float nearGain = 0.5f + 0.5f * width;
        const float farGain = 0.5f - 0.5f * width;
        const float vibInc = vibRateHz * invSr;
        const float vibDepthSemi = vibDepth * 0.5f;
        const bool hasVibrato = vibDepth > 0.0f;
        const float driftInc0 = 0.07f * invSr;
        const float driftInc1 = 0.11f * invSr;
        const float driftInc2 = 0.17f * invSr;

        const bool hasFilterEnv = std::abs(filterEnvAmount) > 0.0001f;
        const float qDamper = std::clamp(1.0f - 0.85f * filterResonance, 0.12f, 1.0f);
        float cachedEnv = -1.0f;
        float envCutoff = std::clamp(filterCutoffHz, 40.0f, 18000.0f);
        bool filterOn = (envCutoff < 17500.0f || filterResonance > 0.02f);
        float svfF =
          filterOn ? std::min(0.85f, 2.0f * sdk::fastSine(0.5f * envCutoff * invSr)) : 0.0f;

        for (size_t i = 0; i < left.size(); ++i)
        {
            switch (envStage)
            {
                case EnvStage::kAttack:
                    envLevel += (1.0f - envLevel) * attackCoef;
                    if (envLevel >= 0.995f)
                    {
                        envLevel = 1.0f;
                        envStage = EnvStage::kDecay;
                    }
                    break;
                case EnvStage::kDecay:
                    envLevel += (ampSustainLevel - envLevel) * decayCoef;
                    if (std::abs(envLevel - ampSustainLevel) <= 0.002f)
                    {
                        envLevel = ampSustainLevel;
                        envStage = (ampSustainLevel <= 0.0001f) ? EnvStage::kIdle
                                                                : EnvStage::kSustain;
                    }
                    break;
                case EnvStage::kSustain:
                    envLevel = ampSustainLevel;
                    break;
                case EnvStage::kRelease:
                    envLevel += (0.0f - envLevel) * releaseCoef;
                    if (envLevel <= 0.0008f)
                    {
                        envLevel = 0.0f;
                        envStage = EnvStage::kIdle;
                    }
                    break;
                case EnvStage::kIdle:
                    break;
            }

            if (envStage == EnvStage::kIdle)
            {
                left[i] = 0.0f;
                right[i] = 0.0f;
                continue;
            }

            if (std::abs(currentGlideNote - targetGlideNote) > 0.001f)
            {
                const float diff = targetGlideNote - currentGlideNote;
                const float step = std::min(std::abs(diff), glideStep * 24.0f);
                currentGlideNote += (diff > 0.0f) ? step : -step;
            }
            else
            {
                currentGlideNote = targetGlideNote;
            }

            driftPhase0 += driftInc0;
            driftPhase1 += driftInc1;
            driftPhase2 += driftInc2;
            if (driftPhase0 >= 1.0f)
            {
                driftPhase0 -= 1.0f;
            }
            if (driftPhase1 >= 1.0f)
            {
                driftPhase1 -= 1.0f;
            }
            if (driftPhase2 >= 1.0f)
            {
                driftPhase2 -= 1.0f;
            }

            vibPhase += vibInc;
            if (vibPhase >= 1.0f)
            {
                vibPhase -= 1.0f;
            }
            const float vibSemi = hasVibrato ? (sdk::fastSine(vibPhase) * vibDepthSemi) : 0.0f;
            const float baseNote = currentGlideNote + pitchBendSemitones + vibSemi;
            const float centerFreq = std::clamp(
              8.1757989f * sdk::fastPow2(baseNote * (1.0f / 12.0f)), 20.0f, 14000.0f);

            const float drift0 = sdk::fastSine(driftPhase0) * driftScale;
            const float drift1 = sdk::fastSine(driftPhase1) * driftScale;
            const float drift2 = sdk::fastSine(driftPhase2) * driftScale;

            // Three oscillators on this one note. Detune and pan are the unison.
            const float osc0 = renderWave(phase0, oscDt(centerFreq, drift0, invSr), wave);
            const float osc1 =
              renderWave(phase1, oscDt(centerFreq, -detuneSemi + drift1, invSr), wave);
            const float osc2 =
              renderWave(phase2, oscDt(centerFreq, detuneSemi + drift2, invSr), wave);

            subPhase += oscDt(centerFreq, 0.0f, invSr) * 0.5f;
            if (subPhase >= 1.0f)
            {
                subPhase -= 1.0f;
            }
            const float sub = sdk::fastSine(subPhase) * subMix;

            float sampleL = (0.5f * osc0 + nearGain * osc1 + farGain * osc2 + sub) * 0.45f;
            float sampleR = (0.5f * osc0 + farGain * osc1 + nearGain * osc2 + sub) * 0.45f;

            if (hasFilterEnv && envLevel != cachedEnv)
            {
                cachedEnv = envLevel;
                envCutoff = std::clamp(
                  filterCutoffHz * sdk::fastPow2(filterEnvAmount * 3.0f * envLevel),
                  40.0f,
                  18000.0f);
                filterOn = (envCutoff < 17500.0f || filterResonance > 0.02f);
                if (filterOn)
                {
                    svfF = std::min(0.85f, 2.0f * sdk::fastSine(0.5f * envCutoff * invSr));
                }
            }

            if (filterOn)
            {
                svfLowL += svfF * svfBandL;
                const float highL = sampleL - svfLowL - qDamper * svfBandL;
                svfBandL += svfF * highL;
                svfBandL = std::clamp(svfBandL, -2.0f, 2.0f);
                svfLowL = std::clamp(svfLowL, -2.0f, 2.0f);
                sampleL = svfLowL;

                svfLowR += svfF * svfBandR;
                const float highR = sampleR - svfLowR - qDamper * svfBandR;
                svfBandR += svfF * highR;
                svfBandR = std::clamp(svfBandR, -2.0f, 2.0f);
                svfLowR = std::clamp(svfLowR, -2.0f, 2.0f);
                sampleR = svfLowR;
            }

            left[i] = sampleL * envLevel * volume;
            right[i] = sampleR * envLevel * volume;
        }
    }

    void renderPostProcessAudio(const InstrumentVoiceContext& /*ctx*/,
                                AudioBufferStereoRef outputBuffer) override
    {
        auto left = outputBuffer[0];
        auto right = outputBuffer[1];
        const float mix = std::clamp(chorusMix, 0.0f, 1.0f);
        const float inc = chorusRateHz / sdk::kSampleRate;
        const float dryKeep = 1.0f - 0.45f * mix;

        for (size_t i = 0; i < left.size(); ++i)
        {
            const float dryL = left[i];
            const float dryR = right[i];
            chorusBuffer[chorusPos] = 0.5f * (dryL + dryR);

            chorusPhase += inc;
            if (chorusPhase >= 1.0f)
            {
                chorusPhase -= 1.0f;
            }
            const float lfo = sdk::fastSine(chorusPhase);
            const float wetL = readChorus(kChorusBaseSamples + lfo * kChorusDepthSamples);
            const float wetR = readChorus(kChorusBaseSamples - lfo * kChorusDepthSamples);
            chorusPos = (chorusPos + 1) & (kChorusLength - 1);

            left[i] = dryL * dryKeep + wetL * mix;
            right[i] = dryR * dryKeep + wetR * mix;
        }
    }

    static void createMetadata(const Patch& /*patch*/, Patch::Metadata& metadata)
    {
        using namespace polysynth::sdk;

        metadata.params.resize(enumutil::EnumMax<LushParamId>::value + 1);
        metadata.params.shrink_to_fit();

        if (paramHandlers.empty())
        {
            paramHandlers.resize(enumutil::EnumMax<LushParamId>::value + 1);
        }

        ParamHelper<LushSynthVoice> helper(metadata, paramHandlers);

        helper.addParam(
          *LushParamId::kWave,
          {
            .displayName = "Wave",
            .defaultValue = 0,
            .minValue = 0,
            .maxValue = static_cast<Patch::paramvalue_short_t>(kWaveNames.size() - 1),
            .isEnum = true,
            .midiCC = *MidiCCs::kOscParam0,
            .valueFormatter =
              [](const Patch& /*patch*/, Patch::paramvalue_t value)
            {
                const int idx =
                  std::clamp<int>(static_cast<int>(std::lround(value)), 0, kWaveNames.size() - 1);
                return std::string(kWaveNames[idx]);
            },
          },
          [](auto& voice, Patch::optional_paramvalue_t val)
          {
              voice.wave = static_cast<LushWave>(
                std::clamp<int>(static_cast<int>(std::lround(*val)), 0, kWaveNames.size() - 1));
          });

        helper.addParam(
          *LushParamId::kDetune,
          {
            .displayName = "Detune",
            .defaultValue = 12.0f,
            .minValue = 0.0f,
            .maxValue = 40.0f,
            .midiCC = *MidiCCs::kOscParam0 + 1,
            .valueFormatter = createCentsFormatter(),
          },
          [](auto& voice, Patch::optional_paramvalue_t val) { voice.detuneCents = *val; });

        helper.addParam(
          *LushParamId::kDrift,
          {
            .displayName = "Drift",
            .defaultValue = 0.35f,
            .minValue = 0.0f,
            .maxValue = 1.0f,
            .midiCC = *MidiCCs::kOscParam0 + 2,
            .valueFormatter = createPercentFormatter(),
          },
          [](auto& voice, Patch::optional_paramvalue_t val) { voice.driftAmount = *val; });

        helper.addParam(
          *LushParamId::kSub,
          {
            .displayName = "Sub Osc",
            .defaultValue = 0.2f,
            .minValue = 0.0f,
            .maxValue = 1.0f,
            .midiCC = *MidiCCs::kOscParam0 + 3,
            .valueFormatter = createPercentFormatter(),
          },
          [](auto& voice, Patch::optional_paramvalue_t val) { voice.subMix = *val; });

        helper.addParam(
          *LushParamId::kChorusMix,
          {
            .displayName = "Chorus",
            .defaultValue = 0.35f,
            .minValue = 0.0f,
            .maxValue = 1.0f,
            .midiCC = *MidiCCs::kOscParam0 + 4,
            .valueFormatter = createPercentFormatter(),
          },
          [](auto& voice, Patch::optional_paramvalue_t val) { voice.chorusMix = *val; });

        helper.addParam(
          *LushParamId::kChorusRate,
          {
            .displayName = "Chorus Rate",
            .defaultValue = 0.3f,
            .minValue = 0.05f,
            .maxValue = 2.0f,
            .editCurve = Patch::Metadata::Param::Curve::kExponential,
            .midiCC = *MidiCCs::kOscParam0 + 5,
            .valueFormatter = createFrequencyFormatter(),
          },
          [](auto& voice, Patch::optional_paramvalue_t val) { voice.chorusRateHz = *val; });

        helper.addParam(
          *LushParamId::kVibRate,
          {
            .displayName = "Vibrato Rate",
            .defaultValue = 4.0f,
            .minValue = 0.2f,
            .maxValue = 8.0f,
            .editCurve = Patch::Metadata::Param::Curve::kExponential,
            .midiCC = *MidiCCs::kLfo1Frequency,
            .valueFormatter = createFrequencyFormatter(),
          },
          [](auto& voice, Patch::optional_paramvalue_t val) { voice.vibRateHz = *val; });

        helper.addParam(
          *LushParamId::kVibDepth,
          {
            .displayName = "Vibrato Depth",
            .defaultValue = 0.0f,
            .minValue = 0.0f,
            .maxValue = 1.0f,
            .midiCC = *MidiCCs::kOscParam0 + 6,
            .valueFormatter = createPercentFormatter(),
          },
          [](auto& voice, Patch::optional_paramvalue_t val) { voice.vibDepth = *val; });

        helper.addParam(
          *LushParamId::kAmpAttack,
          {
            .displayName = "Attack",
            .defaultValue = 0.8f,
            .minValue = 0.0f,
            .maxValue = 8.0f,
            .editCurve = Patch::Metadata::Param::Curve::kExponential,
            .midiCC = *MidiCCs::kAmpEnvAttack,
            .valueFormatter = createTimeFormatter(),
          },
          [](auto& voice, Patch::optional_paramvalue_t val) { voice.ampAttackSec = *val; });

        helper.addParam(
          *LushParamId::kAmpDecay,
          {
            .displayName = "Decay",
            .defaultValue = 1.2f,
            .minValue = 0.01f,
            .maxValue = 8.0f,
            .editCurve = Patch::Metadata::Param::Curve::kExponential,
            .midiCC = *MidiCCs::kAmpEnvDecay,
            .valueFormatter = createTimeFormatter(),
          },
          [](auto& voice, Patch::optional_paramvalue_t val) { voice.ampDecaySec = *val; });

        helper.addParam(
          *LushParamId::kAmpSustain,
          {
            .displayName = "Sustain",
            .defaultValue = 0.85f,
            .minValue = 0.0f,
            .maxValue = 1.0f,
            .midiCC = *MidiCCs::kAmpEnvSustain,
            .valueFormatter = createPercentFormatter(),
          },
          [](auto& voice, Patch::optional_paramvalue_t val) { voice.ampSustainLevel = *val; });

        helper.addParam(
          *LushParamId::kAmpRelease,
          {
            .displayName = "Release",
            .defaultValue = 2.5f,
            .minValue = 0.01f,
            .maxValue = 10.0f,
            .editCurve = Patch::Metadata::Param::Curve::kExponential,
            .midiCC = *MidiCCs::kAmpEnvRelease,
            .valueFormatter = createTimeFormatter(),
          },
          [](auto& voice, Patch::optional_paramvalue_t val) { voice.ampReleaseSec = *val; });

        helper.addParam(
          *LushParamId::kFilterCutoff,
          {
            .displayName = "Cutoff",
            .defaultValue = 1200.0f,
            .minValue = 80.0f,
            .maxValue = 18000.0f,
            .editCurve = Patch::Metadata::Param::Curve::kExponentialSteep,
            .midiCC = *MidiCCs::kFilterCutoff,
            .valueFormatter = createFrequencyFormatter(18000.0f),
          },
          [](auto& voice, Patch::optional_paramvalue_t val) { voice.filterCutoffHz = *val; });

        helper.addParam(
          *LushParamId::kFilterResonance,
          {
            .displayName = "Resonance",
            .defaultValue = 0.12f,
            .minValue = 0.0f,
            .maxValue = 1.0f,
            .midiCC = *MidiCCs::kFilterResonance,
            .valueFormatter = createPercentFormatter(),
          },
          [](auto& voice, Patch::optional_paramvalue_t val) { voice.filterResonance = *val; });

        helper.addParam(
          *LushParamId::kFilterEnvAmount,
          {
            .displayName = "Env Amt",
            .defaultValue = 0.45f,
            .minValue = -1.0f,
            .maxValue = 1.0f,
            .midiCC = *MidiCCs::kFilterEnvAmount,
            .valueFormatter = createPercentFormatter(),
          },
          [](auto& voice, Patch::optional_paramvalue_t val) { voice.filterEnvAmount = *val; });

        helper.addParam(
          *LushParamId::kWidth,
          {
            .displayName = "Width",
            .defaultValue = 0.7f,
            .minValue = 0.0f,
            .maxValue = 1.0f,
            .midiCC = *MidiCCs::kOscParam0 + 7,
            .valueFormatter = createPercentFormatter(),
          },
          [](auto& voice, Patch::optional_paramvalue_t val) { voice.width = *val; });

        helper.addParam(
          *LushParamId::kGlideTime,
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
          *LushParamId::kGlideMode,
          {
            .displayName = "Glide Mode",
            .defaultValue = 0,
            .minValue = 0,
            .maxValue = 2,
            .isEnum = true,
            .valueFormatter =
              [](const Patch& /*patch*/, Patch::paramvalue_t value)
            {
                const int idx =
                  std::clamp<int>(static_cast<int>(std::lround(value)), 0, kGlideNames.size() - 1);
                return std::string(kGlideNames[idx]);
            },
          },
          [](auto& voice, Patch::optional_paramvalue_t val)
          {
              const int mode =
                std::clamp<int>(static_cast<int>(std::lround(*val)), 0, kGlideNames.size() - 1);
              voice.playMode = static_cast<PlayMode>(mode);
          });

        helper.addParam(
          *LushParamId::kVolume,
          {
            .displayName = "Volume",
            .defaultValue = 1.0f,
            .minValue = 0.0f,
            .maxValue = 2.0f,
            .macroOperation = Patch::Metadata::Param::MacroOperation::kMult,
            .valueFormatter = createPercentFormatter(),
          },
          [](auto& voice, Patch::optional_paramvalue_t val) { voice.volume = *val; });

        metadata.layoutFlat = Patch::Metadata::EditorLayout{
            .sections = {
              {
                .name = "Oscillator",
                .shortName = "Osc",
                .elements = {
                  { .type = EditorElement::Type::kParam, .id = *LushParamId::kWave },
                  { .type = EditorElement::Type::kParam, .id = *LushParamId::kDetune },
                  { .type = EditorElement::Type::kParam, .id = *LushParamId::kDrift },
                  { .type = EditorElement::Type::kParam, .id = *LushParamId::kSub },
                },
              },
              {
                .name = "Modulation",
                .shortName = "Mod",
                .elements = {
                  { .type = EditorElement::Type::kParam, .id = *LushParamId::kChorusMix },
                  { .type = EditorElement::Type::kParam, .id = *LushParamId::kChorusRate },
                  { .type = EditorElement::Type::kParam, .id = *LushParamId::kVibRate },
                  { .type = EditorElement::Type::kParam, .id = *LushParamId::kVibDepth },
                },
              },
              {
                .name = "Amplifier",
                .shortName = "Amp",
                .elements = {
                  { .type = EditorElement::Type::kParam, .id = *LushParamId::kAmpAttack },
                  { .type = EditorElement::Type::kParam, .id = *LushParamId::kAmpDecay },
                  { .type = EditorElement::Type::kParam, .id = *LushParamId::kAmpSustain },
                  { .type = EditorElement::Type::kParam, .id = *LushParamId::kAmpRelease },
                },
              },
              {
                .name = "Filter",
                .shortName = "Filter",
                .elements = {
                  { .type = EditorElement::Type::kParam, .id = *LushParamId::kFilterCutoff },
                  { .type = EditorElement::Type::kParam, .id = *LushParamId::kFilterResonance },
                  { .type = EditorElement::Type::kParam, .id = *LushParamId::kFilterEnvAmount },
                },
              },
              {
                .name = "Voice",
                .shortName = "Voice",
                .elements = {
                  { .type = EditorElement::Type::kParam, .id = *LushParamId::kWidth },
                  { .type = EditorElement::Type::kParam, .id = *LushParamId::kGlideTime },
                  { .type = EditorElement::Type::kParam, .id = *LushParamId::kGlideMode },
                  { .type = EditorElement::Type::kParam, .id = *LushParamId::kVolume },
                },
              },
            },
        };

        metadata.layout23 = metadata.layoutFlat;

        metadata.initDefaultMacros = [](auto& macros)
        {
            macros[0] = Patch::Metadata::Macro{
                .displayName = "Detune / Width",
                .modifiers = {
                  { *LushParamId::kDetune, 0.6f },
                  { *LushParamId::kWidth, 0.5f },
                },
                .bipolar = true,
            };
            macros[1] = Patch::Metadata::Macro{
                .displayName = "Filter",
                .modifiers = {
                  { *LushParamId::kFilterCutoff, 1.0f },
                  { *LushParamId::kFilterResonance, 0.3f },
                },
                .bipolar = true,
            };
            macros[2] = Patch::Metadata::Macro{
                .displayName = "Attack / Release",
                .modifiers = {
                  { *LushParamId::kAmpAttack, 0.7f },
                  { *LushParamId::kAmpRelease, 0.7f },
                },
                .bipolar = true,
            };
            macros[3] = Patch::Metadata::Macro{
                .displayName = "Chorus",
                .modifiers = {
                  { *LushParamId::kChorusMix, 0.8f },
                  { *LushParamId::kChorusRate, 0.4f },
                },
                .bipolar = true,
            };
            macros[4] = Patch::Metadata::Macro{
                .displayName = "Drift",
                .modifiers = {
                  { *LushParamId::kDrift, 1.0f },
                },
                .bipolar = true,
            };
            macros[5] = Patch::Metadata::Macro{
                .displayName = "Bloom",
                .modifiers = {
                  { *LushParamId::kFilterEnvAmount, 0.8f },
                  { *LushParamId::kSub, 0.4f },
                },
                .bipolar = true,
            };

            macros[*Patch::SystemMacroId::kVelocity].modifiers = {
                { *LushParamId::kVolume, 1.0f },
            };
        };

        metadata.instrumentTypeId = static_cast<int>(InstrumentTypeId::kLushSynth);
        metadata.outputChannels = Patch::Metadata::OutputChannels::kStereo;
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

    float readChorus(float delaySamples) const
    {
        const float delay = std::clamp(delaySamples, 1.0f, static_cast<float>(kChorusLength - 2));
        const int index = static_cast<int>(delay);
        const float frac = delay - static_cast<float>(index);
        const int i0 = (chorusPos - index) & (kChorusLength - 1);
        const int i1 = (chorusPos - index - 1) & (kChorusLength - 1);
        return chorusBuffer[i0] * (1.0f - frac) + chorusBuffer[i1] * frac;
    }

    LushWave wave = LushWave::kSaw;
    float detuneCents = 12.0f;
    float driftAmount = 0.35f;
    float subMix = 0.2f;
    float chorusMix = 0.35f;
    float chorusRateHz = 0.3f;
    float vibRateHz = 4.0f;
    float vibDepth = 0.0f;

    float ampAttackSec = 0.8f;
    float ampDecaySec = 1.2f;
    float ampSustainLevel = 0.85f;
    float ampReleaseSec = 2.5f;

    float filterCutoffHz = 1200.0f;
    float filterResonance = 0.12f;
    float filterEnvAmount = 0.45f;

    float width = 0.7f;
    float glideTimeSec = 0.0f;
    float volume = 1.0f;

    EnvStage envStage = EnvStage::kIdle;
    float envLevel = 0.0f;
    bool hasPlayedAnyNote = false;
    float currentGlideNote = 60.0f;
    float targetGlideNote = 60.0f;

    float phase0 = 0.0f;
    float phase1 = 0.15f;
    float phase2 = 0.4f;
    float subPhase = 0.0f;
    float vibPhase = 0.0f;
    float driftPhase0 = 0.0f;
    float driftPhase1 = 0.3f;
    float driftPhase2 = 0.6f;

    float svfLowL = 0.0f;
    float svfBandL = 0.0f;
    float svfLowR = 0.0f;
    float svfBandR = 0.0f;

    std::array<float, kChorusLength> chorusBuffer{};
    int chorusPos = 0;
    float chorusPhase = 0.0f;
};

} // namespace

std::unique_ptr<InstrumentType> createLushSynth()
{
    return std::make_unique<InstrumentType>(
      InstrumentType{ .name = "LUSH",
                      .createMetadata = &LushSynthVoice::createMetadata,
                      .createVoice = [](int /*voiceIdx*/) -> std::unique_ptr<InstrumentVoice>
                      { return std::make_unique<LushSynthVoice>(); } });
}

} // namespace polysynth

#endif
