#ifndef MODULES_SCREENHOST_H_
#define MODULES_SCREENHOST_H_

#include <stdint.h>

// Named ModuleBase screens that feature modules may extend.
// FeatureModule::id() remains the sidecar basename — ScreenId selects the host UI.
enum class ScreenId : uint8_t
{
    Song = 0,
};

// Host services for a Song Mode list-column extension.
// Implemented by the Song editor inside firmware — modules depend only on this
// header, not on songEditor.h / display internals.
class SongScreenHost
{
  public:
    virtual ~SongScreenHost() {}

    virtual uint8_t selectedSlot() const = 0;
    virtual uint8_t songLength() const = 0;
    virtual bool isEditMode() const = 0;
    virtual bool isShiftPressed() const = 0;
    virtual void refreshColumn() = 0;
};

// Optional focusable column on the Song playlist list (header + per-row cell).
class SongListColumn
{
  public:
    static constexpr uint8_t kSongSlotCount = 255;
    static constexpr uint8_t kCellTextMax = 4;

    virtual ~SongListColumn() {}

    virtual const char* header() const = 0;
    // Shown on Pattern +/- button labels while the column is focused.
    virtual const char* focusEncoderLabel() const = 0;
    virtual void formatCell(uint8_t slot, char* out, uint8_t outSize) const = 0;

    virtual void onEncoder(SongScreenHost& host, uint8_t slot, int8_t delta) = 0;
    virtual void onClear(SongScreenHost& host, uint8_t slot) = 0;

    // Keep column data aligned when the playlist gains or loses a slot.
    // duplicate=true copies the source slot onto the newly inserted row.
    virtual void onInserted(uint8_t index, bool duplicate) = 0;
    virtual void onRemoved(uint8_t index) = 0;
    virtual void clearAll() = 0;

    // Main thread only. Called when song playback enters this playlist slot.
    virtual void onSlotStart(uint8_t slot) = 0;
};

// Root guest contract for extending an existing ModuleBase screen.
// Add new as*() accessors when other screens gain hosts.
class ScreenExtension
{
  public:
    virtual ~ScreenExtension() {}

    virtual ScreenId screen() const = 0;
    virtual SongListColumn* asSongListColumn() { return nullptr; }
};

// Firmware-provided host for Song Mode list columns.
SongScreenHost& getSongScreenHost();

// Main-thread only. Latches song playlist position during playback and
// dispatches SongListColumn::onSlotStart. Called from FeatureModuleRegistry::updateAll.
void pollSongListColumnSlotStarts();

// Main-thread only. Clears Song list-column data when firmware replaces the
// playlist wholesale (e.g. MOD/IT import).
void notifySongPlaylistReplaced();

#endif /* MODULES_SCREENHOST_H_ */
