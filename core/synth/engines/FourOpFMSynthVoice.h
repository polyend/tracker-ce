#pragma once

#include "PolySynth/sdk/InstrumentType.h"
#include <memory>

namespace polysynth
{
/**
 * FM4: a four-operator phase-modulation synth with eight routing algorithms,
 * a modulator envelope, operator feedback, vibrato and a low-pass filter.
 * See docs/synth_fm4.md.
 */
std::unique_ptr<InstrumentType> createFourOpFMSynth();
} // namespace polysynth
