#include "mtPadBoard.h"
#include "mtAudioEngine.h"
#include "mtSequencer.h"
#include "mtStructs.h"
#include "util_math.h"

cMtPadBoard mtPadBoard;

const uint8_t padBoard[48] = {
    36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
    12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,
};

uint8_t cMtPadBoard::convertPadToNote(uint8_t pad)
{
    if (pad > 47)
        pad = 47;
    return getNoteFromPad(pad);
}

void cMtPadBoard::configureInstrumentPlayer(uint8_t maxVoices)
{
    if (maxVoices > 8)
        maxVoices = 8;
    voicesCount = maxVoices;
}

void cMtPadBoard::startNoteWithoutInstrument(uint8_t note,
                                             std::vector<int16_t*> addres,
                                             uint32_t length,
                                             int8_t velocity,
                                             int16_t source)
{
    int8_t voiceToTake = getEmptyVoice();
    if (voiceToTake < 0)
        return;

    voices[voiceToTake] = source;

    instrumentPlayer[voiceToTake].noteOnWithoutInstrument(
      addres, length, mtSampleTypeWaveFile, note, velocity);
}

int8_t cMtPadBoard::getEmptyVoice()
{
    for (uint8_t i = 0; i < voicesCount; i++)
    {
        if (voices[i] == -1)
            return i;
    }
    return -1;
}

int8_t cMtPadBoard::getVoiceTakenByPad(int16_t source)
{
    for (uint8_t i = 0; i < voicesCount; i++)
    {
        if (voices[i] == source)
            return i;
    }
    return -1;
}

void cMtPadBoard::stopInstrument(int16_t source)
{
    int8_t voiceToClear = getVoiceTakenByPad(source);
    if (voiceToClear < 0)
        return;
    voices[voiceToClear] = -1;
    // mtPrint("stop: ");
    // mtPrintln(voiceToClear);
    instrumentPlayer[voiceToClear].noteOff(
      -3); // zmienione na cut ze wzgledu na to ze i tak nie jest uzywane nigdzie oprocz miejsc
           // gdzie nie ma envelopow
}

void cMtPadBoard::cutInstrument(uint8_t pad)
{
    int8_t voiceToClear = getVoiceTakenByPad(pad);
    if (voiceToClear < 0)
        return;
    voices[voiceToClear] = -1;
    // mtPrint("stop: ");
    // mtPrintln(voiceToClear);
    instrumentPlayer[voiceToClear].noteOff(-3);
}

void cMtPadBoard::clearVoice(uint8_t voice)
{
    if (voice < 0)
        return;
    voices[voice] = -1;
}

// wylacza WSZYSTKIE INSTRUMENTY
void cMtPadBoard::releaseAllInstruments()
{
    // todo: poniżej jest raczej bez sensu
    if (sequencer.getSeqState() == 1)
        return;

    for (uint8_t i = 0; i < 8; i++)
    {
        voices[i] = -1;
        instrumentPlayer[i].noteOff();
    }
}

void cMtPadBoard::releaseInstrumentsTakenByPads()
{
    for (uint8_t i = 0; i < 8; i++)
    {
        if (voices[i] >= 0)
        {
            voices[i] = -1;
            instrumentPlayer[i].noteOff();
        }
    }
}

void cMtPadBoard::cutAllInstrument()
{
    if (sequencer.getSeqState() == 1)
        return;

    for (uint8_t i = 0; i < 8; i++)
    {
        voices[i] = -1;
        instrumentPlayer[i].noteOff(-3);
    }
}

void cMtPadBoard::setPadNotes(uint8_t scale, uint8_t noteOffset, uint8_t rootNote)
{
    // Opcje w konfiguracji maja listy wyboru przesuniete, wiec musimy tutaj dostroic
    // wartosci z mtProject.values do surowych.
    noteOffset += 1;
    rootNote += 12;

    if (lastScale != scale || lastNoteOffset != noteOffset || lastRootNote != rootNote)
    {

        if (noteOffset > MAX_NOTE_OFFSET)
            noteOffset = MAX_NOTE_OFFSET;
        // noteOffset = constrain(noteOffset, MIN_NOTE_OFFSET, MAX_NOTE_OFFSET);

        uint16_t tempNote = rootNote;
        for (int8_t a = 0; a < 48; a++)
        {
            if ((a > 11) && (a % 12 == 0))
            {
                tempNote = rootNote + (a / 12) * noteOffset;
            }

            while (!isInScale(tempNote, rootNote, scale))
            {
                tempNote++;
            }
            padNotes[a] = clamp<int>(tempNote, MIN_NOTE, MAX_NOTE);
            if (tempNote < MAX_NOTE)
                tempNote++;
        }
        lastNoteOffset = noteOffset;
        lastScale = scale;
        lastRootNote = rootNote;
    }
}

uint8_t cMtPadBoard::countPadsWithNote(int8_t note, uint8_t* pads,
                                       [[maybe_unused]] bool drumScaleAware)
{
    uint8_t count = 0;
#ifdef FEATURE_SYNTH
    const bool useScaleDrum = drumScaleAware &&
                              isDrumSynthInstrument(mtProject.values.lastUsedInstrument);
#endif

    for (uint8_t i = 0; i < 48; i++)
    {
        uint8_t padNote;
#ifdef FEATURE_SYNTH
        if (useScaleDrum)
        {
            padNote = padNotes[padBoard[i]];
        }
        else
        {
            padNote = getNoteFromPad(i);
        }
#else
        padNote = getNoteFromPad(i);
#endif

        if (padNote == note)
        {
            pads[i] = 1;
            count++;
        }
        else
            pads[i] = 0;
    }

    return count;
}

uint8_t cMtPadBoard::getNoteFromPad(int8_t pad, [[maybe_unused]] bool isBeatSlice)
{
#ifndef TRACKER_MINI
    if (isBeatSlice)
    {
        return padToBeatNoteConverter.getNoteFromPad(pad);
    }
#endif

#ifdef FEATURE_SYNTH
    if (pad != INTERFACE_BUTTON_PREVIEW &&
        isDrumSynthInstrument(mtProject.values.lastUsedInstrument))
    {
        if (pad < 0)
            pad = 0;
        if (pad > 47)
            pad = 47;
        return FIRST_DRUM_SYNTH_NOTE + padBoard[pad];
    }
#endif

    if (pad == INTERFACE_BUTTON_PREVIEW)
        return 60;
    return padNotes[padBoard[pad]];
}

static uint8_t noteAtScaleDegree(uint8_t scale, uint8_t rootNote, uint8_t degree)
{
    uint8_t note = rootNote;

    for (uint8_t i = 0; i < degree; i++)
    {
        while (!isInScale(note, rootNote, scale))
        {
            note++;
        }
        note++;
    }

    while (!isInScale(note, rootNote, scale))
    {
        note++;
    }

    if (note > MAX_NOTE)
        note = MAX_NOTE;

    return note;
}

uint8_t cMtPadBoard::getSynthEditNoteFromPad(int8_t pad)
{
    if (pad < 0)
        pad = 0;
    if (pad > 47)
        pad = 47;

    int col = pad % 12 - 4;
    if (col < 0)
        col = 0;

#ifdef FEATURE_SYNTH
    if (isDrumSynthInstrument(mtProject.values.lastUsedInstrument))
    {
        static const uint8_t drumSmallPadBoard[32] = {
            64, 65, 66, 67, 68, 69, 70, 71, 56, 57, 58, 59, 60, 61, 62, 63,
            48, 49, 50, 51, 52, 53, 54, 55, 40, 41, 42, 43, 44, 45, 46, 47,
        };
        return drumSmallPadBoard[(pad / 12) * 8 + col];
    }
#endif

    int degree = (3 - pad / 12) * 8 + col;
    return noteAtScaleDegree(mtProject.values.padBoardScale,
                             mtProject.values.padBoardRootNote + 12,
                             degree);
}
