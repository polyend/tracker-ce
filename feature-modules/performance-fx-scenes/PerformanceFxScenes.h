#ifndef PERFORMANCE_FX_SCENES_H_
#define PERFORMANCE_FX_SCENES_H_

#include "FeatureModule.h"
#include "PerformanceHost.h"

#include <stdint.h>

class FileSystem;

static constexpr uint8_t kPerfScenesCount = 8;
static constexpr uint16_t kPerfScenesFileVersion = 1;

struct strPerfScenesFile
{
    char magic[4];
    uint16_t version;
    uint16_t count;
    strPerfScene scenes[kPerfScenesCount];
};

static_assert(sizeof(strPerfScenesFile) % 4 == 0,
              "strPerfScenesFile must be 4-byte aligned for FatFs DMA");

// Example external feature module: Performance FX Scenes.
// Lives outside core/ and only depends on the Feature Module SDK headers.
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

    FileSystem* fileSystem = nullptr;
};

extern PerformanceFxScenes performanceFxScenes;

#endif /* PERFORMANCE_FX_SCENES_H_ */
