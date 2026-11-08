#ifndef MODULES_FEATUREMODULE_H_
#define MODULES_FEATUREMODULE_H_

#include "PatternStepFx.h"
#include "PerformanceHost.h"
#include "ScreenHost.h"

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

    // Optional extension for an existing ModuleBase screen. Return nullptr if none.
    virtual ScreenExtension* screenExtension() { return nullptr; }

    // Optional pattern Step FX exports (see PatternStepFx.h).
    virtual uint8_t patternStepFxCount() const { return 0; }
    virtual const PatternStepFxInfo* patternStepFx(uint8_t index) const
    {
        (void)index;
        return nullptr;
    }

    // Main thread only. Return true if this module handled typeId.
    virtual bool onPatternStepFx(uint8_t typeId, uint8_t value)
    {
        (void)typeId;
        (void)value;
        return false;
    }
};

#endif /* MODULES_FEATUREMODULE_H_ */
