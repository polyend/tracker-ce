#include "ScreenHost.h"

#include "FeatureModuleRegistry.h"
#include "core/interfaceDefs.h"
#include "mtSequencer.h"
#include "mtStructs.h"
#include "songEditor/songEditor.h"

class SongScreenHostImpl : public SongScreenHost
{
  public:
    uint8_t selectedSlot() const override { return songEditor.selectedSongPosition; }

    uint8_t songLength() const override { return static_cast<uint8_t>(songEditor.songLength); }

    bool isEditMode() const override { return songEditor.editMode != 0; }

    bool isShiftPressed() const override
    {
        return songEditor.buttons != nullptr &&
               songEditor.buttons->isButtonPressed(interfaceButtonShift);
    }

    void refreshColumn() override { songEditor.refreshSongPlayerSelectedSlot(); }
};

static SongScreenHostImpl s_songScreenHost;

SongScreenHost& getSongScreenHost()
{
    return s_songScreenHost;
}

void pollSongListColumnSlotStarts()
{
    static bool wasPlayingSong = false;
    static int16_t lastRecalledSongSlot = -1;

    SongListColumn* column = featureModules.getSongListColumn();
    // Performance Mode reports SEQ_STATE_PLAY_PERFORMANCE even while songMode is on.
    const bool playingSong = sequencer.isPlay() && sequencer.isSongMode();

    if (column != nullptr && playingSong)
    {
        const uint8_t songSlot = mtProject.song.playlistPos;
        if (!wasPlayingSong || lastRecalledSongSlot != static_cast<int16_t>(songSlot))
        {
            lastRecalledSongSlot = songSlot;
            column->onSlotStart(songSlot);
        }
        wasPlayingSong = true;
        return;
    }

    wasPlayingSong = false;
    lastRecalledSongSlot = -1;
}
