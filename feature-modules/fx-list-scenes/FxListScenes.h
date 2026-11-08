#ifndef FX_LIST_SCENES_H_
#define FX_LIST_SCENES_H_

#include "FeatureModule.h"
#include "PerformanceHost.h"
#include "ScreenHost.h"

#include <stdint.h>

class FileSystem;

static constexpr uint8_t kFxListScenesCount = 8;
static constexpr uint8_t kFxListEntriesPerScene = 8;
static constexpr uint8_t kFxListScenesNone = 0;
static constexpr uint16_t kFxListScenesFileVersion = 1;
static constexpr uint8_t kFxListSceneNameLength = 16;
static constexpr uint8_t kFxListScenePatternStepFxId = kPatternStepFxModuleIdBase;

struct strFxListEntry
{
    uint8_t fxId;     // 0 = empty; else Performance engine FX id
    uint8_t reserved; // packing
    int16_t value;
};

static_assert(sizeof(strFxListEntry) == 4, "strFxListEntry must be exactly 4 bytes");

struct strFxListScene
{
    char name[kFxListSceneNameLength];
    strFxListEntry entries[kFxListEntriesPerScene];
};

static_assert(sizeof(strFxListScene) % 4 == 0, "strFxListScene must be 4-byte aligned");

struct strFxListScenesFile
{
    char magic[4];
    uint16_t version;
    uint16_t count;
    strFxListScene scenes[kFxListScenesCount];
    uint8_t songSlotScene[SongListColumn::kSongSlotCount + 1];
};

static_assert(sizeof(strFxListScenesFile) % 4 == 0,
              "strFxListScenesFile must be 4-byte aligned for FatFs DMA");
static_assert(SongListColumn::kSongSlotCount == 255,
              "SongListColumn::kSongSlotCount must match SONG_MAX");

// Song Mode FX list scenes: column + mode page + pattern Step FX launcher.
class FxListScenes : public FeatureModule,
                     public ScreenExtension,
                     public SongListColumn,
                     public SongModePage
{
  public:
    const char* id() const override { return "fxlistscenes"; }

    void init(FileSystem* fileSystem) override;
    void onWorkspaceClear() override;
    void onWorkspaceLoad() override;
    void onProjectOpen(const char* projectPath, const char* projectName) override;
    void onProjectSave(const char* projectPath, const char* projectName) override;

    ScreenExtension* screenExtension() override { return this; }
    ScreenId screen() const override { return ScreenId::Song; }
    SongListColumn* asSongListColumn() override { return this; }
    SongModePage* asSongModePage() override { return this; }
    uint32_t startOption() const override { return 2; }

    const char* header() const override { return "FX"; }
    const char* focusEncoderLabel() const override { return "Scene"; }
    void formatCell(uint8_t slot, char* out, uint8_t outSize) const override;
    void onEncoder(SongScreenHost& host, uint8_t slot, int8_t delta) override;
    void onClear(SongScreenHost& host, uint8_t slot) override;
    void onInserted(uint8_t index, bool duplicate) override;
    void onRemoved(uint8_t index) override;
    void clearAll() override;
    void onSlotStart(uint8_t slot) override;

    const char* title() const override { return "FX Scene"; }
    bool canOpen(SongScreenHost& host) const override;
    void onShow(SongScreenHost& host) override;
    void onHide(SongScreenHost& host) override;
    void onEncoder(SongScreenHost& host, int8_t delta) override;
    void onScreenButton(SongScreenHost& host, uint8_t button, uint8_t state) override;
    void onNavigate(SongScreenHost& host, int8_t delta) override;
    void onHorizontalNavigate(SongScreenHost& host, int8_t delta) override;
    void onClearFocused(SongScreenHost& host) override;
    void onBack(SongScreenHost& host) override;

    uint8_t patternStepFxCount() const override { return 1; }
    const PatternStepFxInfo* patternStepFx(uint8_t index) const override;
    bool onPatternStepFx(uint8_t typeId, uint8_t value) override;

  private:
    void clearBank();
    bool readFile(const char* path);
    void writeFile(const char* path);
    bool validateHeader() const;
    void saveToWorkspace();
    void clampSongSlotScenes();
    void assignSlot(uint8_t slot, uint8_t scene);
    uint8_t sceneForSlot(uint8_t slot) const;
    void applyScene(uint8_t sceneIndex); // 0..7
    void refreshEditorUi(SongScreenHost& host);
    void formatEntryName(uint8_t entry, char* out, uint8_t outSize) const;
    void formatEntryValue(uint8_t entry, char* out, uint8_t outSize) const;
    bool sceneHasEntries(uint8_t sceneIndex) const;
    uint8_t nextFxId(uint8_t current, int8_t delta) const;
    int16_t clampFxValue(uint8_t fxId, int16_t value) const;
    void selectEditingScene(SongScreenHost& host, uint8_t sceneIndex);
    void changeFocusedFxType(SongScreenHost& host, int8_t delta);
    void changeFocusedFxValue(SongScreenHost& host, int8_t delta);

    FileSystem* fileSystem = nullptr;
    uint8_t editingScene = 0; // 0..7 while editor open
    uint8_t focusedEntry = 0;
    uint8_t focusedField = 0; // 0 = FX type, 1 = value
};

extern FxListScenes fxListScenes;

#endif /* FX_LIST_SCENES_H_ */
