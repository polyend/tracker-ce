#ifndef MUTE_BANKS_H_
#define MUTE_BANKS_H_

#include "FeatureModule.h"
#include "PerformanceHost.h"

#include <stdint.h>

class FileSystem;

static constexpr uint8_t kMuteBanksCount = 8;
static constexpr uint16_t kMuteBanksFileVersion = 1;

struct strMuteBanksFile
{
    char magic[4];
    uint16_t version;
    uint16_t count;
    strMuteScene scenes[kMuteBanksCount];
};

static_assert(sizeof(strMuteBanksFile) % 4 == 0,
              "strMuteBanksFile must be 4-byte aligned for FatFs DMA");

class MuteBanks : public FeatureModule, public PerformancePage
{
  public:
    const char* id() const override { return "mutebanks"; }

    void init(FileSystem* fileSystem) override;
    void onWorkspaceClear() override;
    void onWorkspaceLoad() override;
    void onProjectOpen(const char* projectPath, const char* projectName) override;
    void onProjectSave(const char* projectPath, const char* projectName) override;
    PerformancePage* performancePage() override { return this; }

    const char* title() const override { return "Mutes"; }
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

extern MuteBanks muteBanks;

#endif /* MUTE_BANKS_H_ */
