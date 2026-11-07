#ifndef PATTERN_LAUNCH_SCENES_H_
#define PATTERN_LAUNCH_SCENES_H_

#include "FeatureModule.h"
#include "PerformanceHost.h"

#include <stdint.h>

class FileSystem;

static constexpr uint8_t kPatternScenesCount = 8;
static constexpr uint16_t kPatternScenesFileVersion = 1;

struct strPatternScenesFile
{
    char magic[4];
    uint16_t version;
    uint16_t count;
    strPatternScene scenes[kPatternScenesCount];
};

static_assert(sizeof(strPatternScenesFile) % 4 == 0,
              "strPatternScenesFile must be 4-byte aligned for FatFs DMA");

class PatternLaunchScenes : public FeatureModule, public PerformancePage
{
  public:
    const char* id() const override { return "patternscenes"; }

    void init(FileSystem* fileSystem) override;
    void onWorkspaceClear() override;
    void onWorkspaceLoad() override;
    void onProjectOpen(const char* projectPath, const char* projectName) override;
    void onProjectSave(const char* projectPath, const char* projectName) override;
    PerformancePage* performancePage() override { return this; }

    const char* title() const override { return "Patterns"; }
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

extern PatternLaunchScenes patternLaunchScenes;

#endif /* PATTERN_LAUNCH_SCENES_H_ */
