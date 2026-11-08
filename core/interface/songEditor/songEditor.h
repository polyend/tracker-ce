#ifndef INTERFACE_SONGEDITOR_H_
#define INTERFACE_SONGEDITOR_H_

#include "core/modulesBase.h"
#include "ScreenHost.h"

#include "mtStructs.h"

typedef enum
{
    iconPlay,
    iconLoop,
    iconPerformance,
} icon_t;

typedef enum
{
    playerUp,
    playerDown,
    playerLeft,
    playerRight,
    playerHome,
} player_direction_t;

typedef enum
{
    selDirNone = 0x00U,
    selDirLeft = 0x01U,
    selDirRight = 0x02U,
    selDirUp = 0x04U,
    selDirDown = 0x08U,

} select_direction_t;

class cSongEditor : public ModuleBase
{

  public:
    virtual void update();
    virtual void start(uint32_t options);
    virtual void stop();

    virtual void initDisplayControls();
    virtual void destroyDisplayControls() {}
    virtual uint8_t getCurrentModeIdx() override { return songViewPage; }
    // Playlist + feature-module SongModePage entries (Mini menu dots).
    virtual uint8_t menuModeCount() override;
    virtual uint32_t menuModeOption(uint8_t index) override;

    cSongEditor() {}
    virtual ~cSongEditor() {}

    void showDefaultScreen();

    void activateLabelsBorder();

    void showTempoValue();
    void refreshTempoField();
    void refreshAddSlotLabel(bool isButtonPressed);

    localList_t patternsList;

    strFrameData frameData;
    strLabelData doubleArrowRed;
    strLabelData labelDataRed;

    hControl label[8] = { -1 };
    hControl barControl = -1;
    hControl songPlayerControl = -1;
    hControl modePageListControl = -1;

    hControl frameControl = -1;
    hControl bgLabel;

    static constexpr uint8_t kModePageListRows = 8;
    static constexpr uint8_t kModePageListNameLen = 24;
    static constexpr uint8_t kModePageListValueLen = 12;
    strParamValueList modePageList{};
    char modePageParamNames[kModePageListRows][kModePageListNameLen]{};
    char modePageParamValues[kModePageListRows][kModePageListValueLen]{};
    char* modePageParamNamePtrs[kModePageListRows]{};
    char* modePageParamValuePtrs[kModePageListRows]{};

    uint8_t lastSelectedPlace = 1;
    uint8_t selectedPlace = 0;

    void setDefaultScreenFunct();

    void changeSongPositionSelection(int16_t value);
    void changeSelectedPlayListPattern(int value);
    void changeSelectedListColumn(int delta);

    void readSong();

    void changeGlobalTempo(float value);
    void setDefaultGlobalTempo();

    bool hasListColumn() const;
    uint8_t listColumnPlace() const { return 2; }
    uint8_t tempoPlace() const;
    void configureFramePlaces();
    void refreshListColumnButtonLabels();
    void wireListColumnDrawData();

    bool isModePageOpen() const { return activeModePage != nullptr; }
    SongModePage* getActiveModePage() const { return activeModePage; }
    void showModePage(SongModePage* page);
    void hideModePage();
    void showModePageScreen();
    void hideModePageScreen();
    void setModePageTitle(const char* title);
    void setModePageLine(uint8_t row, const char* text);
    void setModePageScreenLabel(uint8_t index,
                                const char* text,
                                uint8_t state,
                                const char* text2);
    void finishModePageLabels();
    void setModePageListCount(uint8_t count);
    void setModePageListRow(uint8_t index, const char* name, const char* value);
    void setModePageListFocus(uint8_t index, bool editingValue);
    void refreshModePage();
    void bindModePageButtons();
    void handleModePageEncoder(int8_t delta);

    //--------------------------------------------------------------

    void refreshButtonsLabels();

    uint8_t playButtonsState;

    //--------------------------------------------------------------
    // patterns
    void refreshPatternsList();
    void cleanPatternsListResources();

    uint16_t songLength = 1;
    uint8_t selectedSongPosition = 0;

    //--------------------------------------------------------------
    //  global params
    char globalTempoVal[10];
    char patternLengthVal[6];

    //--------------------------------------------------------------

    void showIcon(icon_t iconType, uint8_t position);
    void hideIcon();
    void handleEntryIcon();
    //--------------------------------------------------------------

    void switchToNewPattern();

    uint8_t exitOnButtonRelease;
    bool pressedSongButton = false;

    player_data_t songPlayerData;
    uint32_t* patternBitmask;

    void refreshSongPlayerControl();
    void refreshSongPlayerSelectedSlot();
    void startSongPlayer();
    void stopSongPlayer();
    void refreshSongPlayer(uint8_t source);

    void stopPlayingSelection();

    void walkOnSongPlayer(player_direction_t dir);
    void selectSpecificTrack(uint8_t track, uint8_t songPosition);
    void selectSongPosition(uint8_t songPosition);

    elapsedMillis songPlayerRefreshTimer;

    song_selection_t copyCurrentData;
    uint8_t isSomethingCopied = 0;
    uint8_t isCopyingInProgress = 0;
    bool clearSelectionAfterCopying = false;
    uint8_t copyElementMax;
    uint8_t currentCopyElement;
    uint8_t currSelDirection;
    uint8_t currSelDirection2;

    uint8_t isBusy = 0;

    uint8_t isDeletingInProgress = 0;
    uint8_t deleteElementMax;
    uint8_t currentDeleteElement;

    void tapTempo_tap();
    void tapTempo_update();
    void tapTempo_end();
    bool tapTempo_still_going();
    void tapTempo_set(float value);

    // GREEN/RED EDIT MODE
    uint8_t editMode;
    void refreshEditMode();
    int16_t lastSongListPos = -1;
    void lightUpPadBoard();

    SongModePage* activeModePage = nullptr;
    // 0 = playlist; 1+ = feature SongModePage menu index.
    uint8_t songViewPage = 0;

  private:
    uint8_t tapTempoCount = 0;
    char tapText[12] = { 0 };
    bool tapTempoActive = 0;
    float tapTempoSum = 0.0;
    elapsedMillis tapTempoTimer = 0;

    char** patternNames = nullptr;
    static constexpr char kEmptyPatternNameText[2] = "";
};

extern cSongEditor songEditor;

void cbkSongPlaybackModeToggle(void);

#endif /* INTERFACE_SONGEDITOR_H_ */
