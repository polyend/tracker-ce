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

// Optional song-playlist bindings owned by a feature module.
// Slot indices match the song playlist (0 .. kSongSlotCount-1).
class SongSlotBinding
{
  public:
    static constexpr uint8_t kSongSlotCount = 255;
    static constexpr uint8_t kSceneNone = 0;
    static constexpr uint8_t kMaxSceneIndex = 8; // values 1..8

    virtual ~SongSlotBinding() {}

    // One byte per playlist slot: 0 = none, 1..kMaxSceneIndex = scene.
    virtual const uint8_t* values() const = 0;

    // Wrap through none and scenes 1..kMaxSceneIndex for the given playlist slot.
    virtual void cycle(uint8_t slot, int8_t delta) = 0;

    // Set an absolute assignment (0 = none, 1..kMaxSceneIndex = scene).
    virtual void assign(uint8_t slot, uint8_t scene) = 0;

    // Keep assignments aligned when the playlist gains or loses a slot.
    // duplicate=true copies the source slot onto the newly inserted row.
    virtual void onInserted(uint8_t index, bool duplicate) = 0;
    virtual void onRemoved(uint8_t index) = 0;

    // Clear every assignment (e.g. after MOD/IT import replaces the playlist).
    virtual void clearAll() = 0;

    // Main thread only. Called when song playback enters this playlist slot.
    virtual void onSlotStart(uint8_t slot) = 0;
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

    // Optional song-slot binding. Return nullptr if the module has none.
    virtual SongSlotBinding* songSlotBinding() { return nullptr; }
};

#endif /* MODULES_FEATUREMODULE_H_ */
