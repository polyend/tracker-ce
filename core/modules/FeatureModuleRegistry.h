#ifndef MODULES_FEATUREMODULEREGISTRY_H_
#define MODULES_FEATUREMODULEREGISTRY_H_

#include "FeatureModule.h"

#include <stdint.h>

class FileSystem;

class FeatureModuleRegistry
{
  public:
    static constexpr uint8_t kMaxModules = 8;

    void add(FeatureModule* module);
    void clear();

    void initAll(FileSystem* fs);
    void onWorkspaceClear();
    void onWorkspaceLoad();
    void onProjectOpen(const char* projectPath, const char* projectName);
    void onProjectSave(const char* projectPath, const char* projectName);
    void updateAll();

    uint8_t count() const { return moduleCount; }
    FeatureModule* get(uint8_t index) const;

    uint8_t performancePageCount() const;
    PerformancePage* getPerformancePage(uint8_t index) const;

    // First Song list-column extension, or nullptr.
    SongListColumn* getSongListColumn() const;

  private:
    FeatureModule* modules[kMaxModules] = {};
    uint8_t moduleCount = 0;
};

void registerFeatureModules(FeatureModuleRegistry& registry);

extern FeatureModuleRegistry featureModules;

#endif /* MODULES_FEATUREMODULEREGISTRY_H_ */
