#ifndef MODULES_FEATUREMODULE_H_
#define MODULES_FEATUREMODULE_H_

#include "PerformanceHost.h"

#include <stdint.h>

class FileSystem;

// Optional page hosted inside Performance Mode (not a top-level ModuleBase screen).
class PerformancePage
{
  public:
    virtual ~PerformancePage() {}

    virtual const char* title() const = 0;
    virtual void onShow(PerformancePageHost& host) = 0;
    virtual void onHide(PerformancePageHost& host) = 0;
    virtual void onScreenButton(PerformancePageHost& host, uint8_t button, uint8_t state) = 0;
};

// Compile-time feature module linked into the firmware image.
// External modules live in FEATURE_MODULE_DIRS and are registered from the
// generated registry (see docs/feature_module_sdk.md).
class FeatureModule
{
  public:
    virtual ~FeatureModule() {}

    // Sidecar basename without extension, e.g. "perfscenes" -> perfscenes.bin
    virtual const char* id() const = 0;

    virtual void init(FileSystem* fs) { (void)fs; }
    virtual void onWorkspaceClear() {}
    virtual void onWorkspaceLoad() {}
    virtual void onProjectOpen(const char* projectPath, const char* projectName)
    {
        (void)projectPath;
        (void)projectName;
    }
    virtual void onProjectSave(const char* projectPath, const char* projectName)
    {
        (void)projectPath;
        (void)projectName;
    }

    // Main thread only. Do not call from the audio callback.
    virtual void update() {}

    // Optional Performance Mode page. Return nullptr if the module has none.
    virtual PerformancePage* performancePage() { return nullptr; }
};

#endif /* MODULES_FEATUREMODULE_H_ */
