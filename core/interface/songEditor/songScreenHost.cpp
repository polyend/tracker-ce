#include "ScreenHost.h"

#include "core/interfaceDefs.h"
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
