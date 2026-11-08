#include "ScreenHost.h"

#include "FeatureModuleRegistry.h"
#include "core/interfaceDefs.h"
#include "display.h"
#include "mtSequencer.h"
#include "songEditor/songEditor.h"
#include "titleBar.h"

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

    void openModePage() override
    {
        if (songEditor.getActiveModePage() != nullptr)
        {
            return;
        }
        if (featureModules.songModePageCount() == 0)
        {
            return;
        }
        songEditor.showModePage(featureModules.getSongModePage(0));
    }

    void closeModePage() override { songEditor.hideModePage(); }

    bool isModePageOpen() const override { return songEditor.isModePageOpen(); }

    void setEditorTitle(const char* title) override { songEditor.setModePageTitle(title); }

    void setEditorLine(uint8_t row, const char* text) override
    {
        songEditor.setModePageLine(row, text);
    }

    void setScreenLabel(uint8_t index,
                        const char* text,
                        SongEditorLabelState state,
                        const char* text2) override
    {
        songEditor.setModePageScreenLabel(
          index, text, static_cast<uint8_t>(state), text2);
    }

    void finishScreenLabels() override { songEditor.finishModePageLabels(); }

    void setEditorListCount(uint8_t count) override { songEditor.setModePageListCount(count); }

    void setEditorListRow(uint8_t index, const char* name, const char* value) override
    {
        songEditor.setModePageListRow(index, name, value);
    }

    void setEditorListFocus(uint8_t index, bool editingValue) override
    {
        songEditor.setModePageListFocus(index, editingValue);
    }

    void refreshEditor() override { songEditor.refreshModePage(); }
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

void notifySongPlaylistReplaced()
{
    if (SongListColumn* column = featureModules.getSongListColumn())
    {
        column->clearAll();
    }
}
