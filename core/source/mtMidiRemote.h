#ifndef MT_MIDI_REMOTE_H
#define MT_MIDI_REMOTE_H

#include <stdint.h>

// MIDI remote control is an optional mapping layer on top of the ordinary
// USB / TRS MIDI handling. Note events which do not match a binding pass
// through unchanged to the sampler and sequencer.
enum MidiRemoteAction : uint8_t
{
    remoteUp,
    remoteDown,
    remoteLeft,
    remoteRight,
    remoteSelect,
    remoteMenu,
    remoteCopy,
    remotePaste,
    remoteInsert,
    remoteDelete,
    remotePlay,
    remoteStop,
    remoteRecord,
    remotePattern,
    remoteSong,
    remoteInstrument,
    remoteValueMinus,
    remoteValuePlus
};

enum MidiRemoteMessage : uint8_t
{
    remoteUnassigned,
    remoteCC,
    remoteNote
};
enum MidiRemoteSource : uint8_t
{
    remoteUSB,
    remoteJack,
    remoteBoth
};

// Returns true if the incoming message was consumed by MIDI remote control.
bool processMidiRemote(uint8_t source, uint8_t channel, uint8_t type,
                       uint8_t number, uint8_t value);
void startMidiRemoteLearn();
void clearMidiRemoteAssignment();
void copyLaunchpadPresetToUserProfile();
void cancelMidiRemoteLearn();

// Implemented alongside the Pattern editor's normal Shift+Copy handler.
void pastePatternFromMidiRemote();

#endif
