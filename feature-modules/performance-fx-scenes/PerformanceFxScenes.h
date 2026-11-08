#ifndef PERFORMANCE_FX_SCENES_H_
#define PERFORMANCE_FX_SCENES_H_

#include "FeatureModule.h"
#include "PerformanceHost.h"

#include <stdint.h>

class FileSystem;

static constexpr uint8_t kPerfScenesCount = 8;
static constexpr uint16_t kPerfScenesFileVersion = 3;
static constexpr uint16_t kPerfScenesFileVersionV2 = 2;
static constexpr uint16_t kPerfScenesFileVersionV1 = 1;

struct strPerfScenesFile
{
    char magic[4];
    uint16_t version;
    uint16_t count;
    strPerfScene scenes[kPerfScenesCount];
};

static_assert(sizeof(strPerfScenesFile) % 4 == 0,
              "strPerfScenesFile must be 4-byte aligned for FatFs DMA");

// Legacy v2: scenes + song-slot bindings (song bindings ignored on load).
struct strPerfScenesFileV2
{
    char magic[4];
    uint16_t version;
    uint16_t count;
    strPerfScene scenes[kPerfScenesCount];
    uint8_t songSlotScene[255 + 1];
};

static_assert(sizeof(strPerfScenesFileV2) % 4 == 0,
              "strPerfScenesFileV2 must be 4-byte aligned for FatFs DMA");

// Performance Mode page: store/recall 12-slot FX rack snapshots.
class PerformanceFxScenes : public FeatureModule, public PerformancePage
{
  public:
    const char* id() const override { return "perfscenes"; }

    void init(FileSystem* fileSystem) override;
    void onWorkspaceClear() override;
    void onWorkspaceLoad() override;
    void onProjectOpen(const char* projectPath, const char* projectName) override;
    void onProjectSave(const char* projectPath, const char* projectName) override;
    PerformancePage* performancePage() override { return this; }

    const char* title() const override { return "Scenes"; }
    void onShow(PerformancePageHost& host) override;
    void onHide(PerformancePageHost& host) override;
    void onScreenButton(PerformancePageHost& host, uint8_t button, uint8_t state) override;

    bool isFilled(uint8_t idx) const;

  private:
    void clearBank();
    bool readFile(const char* path);
    void writeFile(const char* path);
    bool validateHeader() const;
    void saveToWorkspace();
    void storeScene(PerformancePageHost& host, uint8_t idx, const char* name);
    bool recallScene(PerformancePageHost& host, uint8_t idx);
    void setActiveSlot(uint8_t idx);
    void refreshPage(PerformancePageHost& host);

    FileSystem* fileSystem = nullptr;
    uint8_t activeSlot = 0xFF; // 0..7, or 0xFF if none
};

extern PerformanceFxScenes performanceFxScenes;

#endif /* PERFORMANCE_FX_SCENES_H_ */
