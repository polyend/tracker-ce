#ifndef MODULES_SCREENHOST_H_
#define MODULES_SCREENHOST_H_

#include <stdint.h>

// Named ModuleBase screens that feature modules may extend.
// FeatureModule::id() remains the sidecar basename — ScreenId selects the host UI.
enum class ScreenId : uint8_t
{
    Song = 0,
};

// Soft-button visual states for Song mode-page labels.
enum class SongEditorLabelState : uint8_t
{
    Empty = 0,
    Filled = 1,
    Active = 2,
};

// Host services for Song Mode list-column + mode-page extensions.
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

    // Mode-page chrome (hides playlist; shows param-value list + soft labels).
    virtual void openModePage() = 0;
    virtual void closeModePage() = 0;
    virtual bool isModePageOpen() const = 0;
    virtual void setEditorTitle(const char* title) = 0;
    virtual void setEditorLine(uint8_t row, const char* text) = 0;
    virtual void setScreenLabel(uint8_t index,
                                const char* text,
                                SongEditorLabelState state,
                                const char* text2 = nullptr) = 0;
    virtual void finishScreenLabels() = 0;
    // Param name + value rows. Call setEditorListCount then rows, then focus.
    virtual void setEditorListCount(uint8_t count) = 0;
    virtual void setEditorListRow(uint8_t index, const char* name, const char* value) = 0;
    // editingValue=false frames the name column; true frames the value column.
    virtual void setEditorListFocus(uint8_t index, bool editingValue) = 0;
    virtual void refreshEditor() = 0;
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

// Optional Song Mode page: Mini-menu mode + full-page UI/input (PerformancePage-style).
// Option 0 is reserved for menu-dismiss preserve; playlist uses option 1.
// Feature modules return startOption() >= 2.
class SongModePage
{
  public:
    virtual ~SongModePage() {}

    // Value passed to cSongEditor::start() when this mode is selected.
    virtual uint32_t startOption() const = 0;

    virtual const char* title() const = 0;
    // Return false to refuse open.
    virtual bool canOpen(SongScreenHost& host) const = 0;
    virtual void onShow(SongScreenHost& host) = 0;
    virtual void onHide(SongScreenHost& host) = 0;
    virtual void onEncoder(SongScreenHost& host, int8_t delta) = 0;
    virtual void onScreenButton(SongScreenHost& host, uint8_t button, uint8_t state) = 0;
    // Up/Down while the page is open.
    virtual void onNavigate(SongScreenHost& host, int8_t delta) { (void)host; (void)delta; }
    // Left/Right while the page is open.
    virtual void onHorizontalNavigate(SongScreenHost& host, int8_t delta)
    {
        (void)host;
        (void)delta;
    }
    // Delete while the page is open.
    virtual void onClearFocused(SongScreenHost& host) { (void)host; }
    virtual void onBack(SongScreenHost& host) = 0;
};

// Root guest contract for extending an existing ModuleBase screen.
// Add new as*() accessors when other screens gain hosts.
class ScreenExtension
{
  public:
    virtual ~ScreenExtension() {}

    virtual ScreenId screen() const = 0;
    virtual SongListColumn* asSongListColumn() { return nullptr; }
    virtual SongModePage* asSongModePage() { return nullptr; }
};

// Firmware-provided host for Song Mode list columns / mode pages.
SongScreenHost& getSongScreenHost();

// Main-thread only. Latches song playlist position during playback and
// dispatches SongListColumn::onSlotStart. Called from FeatureModuleRegistry::updateAll.
void pollSongListColumnSlotStarts();

// Main-thread only. Clears Song list-column data when firmware replaces the
// playlist wholesale (e.g. MOD/IT import).
void notifySongPlaylistReplaced();

#endif /* MODULES_SCREENHOST_H_ */
