#ifndef PERFORMANCE_FX_SCENES_H_
#define PERFORMANCE_FX_SCENES_H_

#include "FeatureModule.h"
#include "PerformanceHost.h"
#include "ScreenHost.h"

#include <stdint.h>

class FileSystem;

static constexpr uint8_t kPerfScenesCount = 8;
static constexpr uint8_t kPerfScenesNone = 0;
static constexpr uint16_t kPerfScenesFileVersion = 2;
static constexpr uint16_t kPerfScenesFileVersionV1 = 1;
// Stable pattern Step FX id in the module-reserved range [128,143].
static constexpr uint8_t kPerfFxScenePatternStepFxId = kPatternStepFxModuleIdBase;

struct strPerfScenesFileV1
{
    char magic[4];
    uint16_t version;
    uint16_t count;
    strPerfScene scenes[kPerfScenesCount];
};

static_assert(sizeof(strPerfScenesFileV1) % 4 == 0,
              "strPerfScenesFileV1 must be 4-byte aligned for FatFs DMA");

struct strPerfScenesFile
{
    char magic[4];
    uint16_t version;
    uint16_t count;
    strPerfScene scenes[kPerfScenesCount];
    // 0 = none, 1..kPerfScenesCount = scene slot. One pad byte for DMA alignment.
    uint8_t songSlotScene[SongListColumn::kSongSlotCount + 1];
};

static_assert(sizeof(strPerfScenesFile) % 4 == 0,
              "strPerfScenesFile must be 4-byte aligned for FatFs DMA");
static_assert(SongListColumn::kSongSlotCount == 255,
              "SongListColumn::kSongSlotCount must match SONG_MAX");

// External feature module: Performance FX Scenes + Song list column.
// Lives outside core/ and only depends on the Feature Module SDK headers.
class PerformanceFxScenes : public FeatureModule,
                            public PerformancePage,
                            public ScreenExtension,
                            public SongListColumn
{
  public:
    const char* id() const override { return "perfscenes"; }

    void init(FileSystem* fileSystem) override;
    void onWorkspaceClear() override;
    void onWorkspaceLoad() override;
    void onProjectOpen(const char* projectPath, const char* projectName) override;
    void onProjectSave(const char* projectPath, const char* projectName) override;
    PerformancePage* performancePage() override { return this; }
    ScreenExtension* screenExtension() override { return this; }

    const char* title() const override { return "Scenes"; }
    void onShow(PerformancePageHost& host) override;
    void onHide(PerformancePageHost& host) override;
    void onScreenButton(PerformancePageHost& host, uint8_t button, uint8_t state) override;

    ScreenId screen() const override { return ScreenId::Song; }
    SongListColumn* asSongListColumn() override { return this; }

    const char* header() const override { return "FX"; }
    const char* focusEncoderLabel() const override { return "Scene"; }
    void formatCell(uint8_t slot, char* out, uint8_t outSize) const override;
    void onEncoder(SongScreenHost& host, uint8_t slot, int8_t delta) override;
    void onClear(SongScreenHost& host, uint8_t slot) override;
    void onInserted(uint8_t index, bool duplicate) override;
    void onRemoved(uint8_t index) override;
    void clearAll() override;
    void onSlotStart(uint8_t slot) override;

    uint8_t patternStepFxCount() const override { return 1; }
    const PatternStepFxInfo* patternStepFx(uint8_t index) const override;
    bool onPatternStepFx(uint8_t typeId, uint8_t value) override;

    bool isFilled(uint8_t idx) const;

  private:
    void clearBank();
    bool readFile(const char* path);
    void writeFile(const char* path);
    bool validateHeader() const;
    void saveToWorkspace();
    void storeScene(PerformancePageHost& host, uint8_t idx, const char* name);
    bool recallScene(PerformancePageHost& host, uint8_t idx);
    void clampSongSlotScenes();
    void assignSlot(uint8_t slot, uint8_t scene);

    FileSystem* fileSystem = nullptr;
};

extern PerformanceFxScenes performanceFxScenes;

#endif /* PERFORMANCE_FX_SCENES_H_ */
