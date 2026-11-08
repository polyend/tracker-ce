#include "PerformanceFxScenes.h"

#include "ModuleSidecar.h"
#include "Platform.h"

#include <cr_section_macros.h>
#include <cstdio>
#include <cstring>

__NOINIT(EXTERNAL_RAM) static strPerfScenesFile externalRamPerfScenesBank;

PerformanceFxScenes performanceFxScenes;

static const PatternStepFxInfo kPerfScenePatternStepFx = {
    kPerfFxScenePatternStepFxId,
    'N',
    "Perf FX Scene",
    "Perf Scene",
    "Recall Performance FX scene 1-8.",
    1,
    kPerfScenesCount,
    1,
};

const PatternStepFxInfo* PerformanceFxScenes::patternStepFx(uint8_t index) const
{
    return index == 0 ? &kPerfScenePatternStepFx : nullptr;
}

bool PerformanceFxScenes::onPatternStepFx(uint8_t typeId, uint8_t value)
{
    if (typeId != kPerfFxScenePatternStepFxId)
    {
        return false;
    }

    if (value < 1 || value > kPerfScenesCount)
    {
        return true;
    }

    PerformancePageHost& host = getPerformancePageHost();
    if (recallScene(host, static_cast<uint8_t>(value - 1)) && host.isShowingFxRack())
    {
        host.refreshFxRack();
    }
    return true;
}

void PerformanceFxScenes::init(FileSystem* fileSystem)
{
    this->fileSystem = fileSystem;
    clearBank();
}

void PerformanceFxScenes::onWorkspaceClear()
{
    clearBank();
}

void PerformanceFxScenes::onWorkspaceLoad()
{
    char path[ModuleSidecar::kMaxPathLength];
    ModuleSidecar::workspacePath(path, sizeof(path), id());

    if (!readFile(path))
    {
        clearBank();
        if (fileSystem != nullptr && fileSystem->exists(path))
        {
            fileSystem->remove(path);
        }
    }
}

void PerformanceFxScenes::onProjectOpen(const char* projectPath, const char* projectName)
{
    char srcPath[ModuleSidecar::kMaxPathLength];
    ModuleSidecar::projectPath(srcPath, sizeof(srcPath), projectPath, projectName, id());

    char workspacePathBuf[ModuleSidecar::kMaxPathLength];
    ModuleSidecar::workspacePath(workspacePathBuf, sizeof(workspacePathBuf), id());

    if (!readFile(srcPath))
    {
        clearBank();
        if (fileSystem != nullptr && fileSystem->exists(workspacePathBuf))
        {
            fileSystem->remove(workspacePathBuf);
        }
        return;
    }

    writeFile(workspacePathBuf);
}

void PerformanceFxScenes::onProjectSave(const char* projectPath, const char* projectName)
{
    if (fileSystem == nullptr)
    {
        return;
    }

    char workspacePathBuf[ModuleSidecar::kMaxPathLength];
    ModuleSidecar::workspacePath(workspacePathBuf, sizeof(workspacePathBuf), id());

    if (!fileSystem->exists(workspacePathBuf))
    {
        return;
    }

    if (!readFile(workspacePathBuf))
    {
        return;
    }

    char destPath[ModuleSidecar::kMaxPathLength];
    ModuleSidecar::projectPath(destPath, sizeof(destPath), projectPath, projectName, id());
    writeFile(destPath);
}

void PerformanceFxScenes::clearBank()
{
    memset(&externalRamPerfScenesBank, 0, sizeof(externalRamPerfScenesBank));
    externalRamPerfScenesBank.magic[0] = 'P';
    externalRamPerfScenesBank.magic[1] = 'F';
    externalRamPerfScenesBank.magic[2] = 'X';
    externalRamPerfScenesBank.magic[3] = 'S';
    externalRamPerfScenesBank.version = kPerfScenesFileVersion;
    externalRamPerfScenesBank.count = kPerfScenesCount;
}

bool PerformanceFxScenes::validateHeader() const
{
    return externalRamPerfScenesBank.magic[0] == 'P' && externalRamPerfScenesBank.magic[1] == 'F' &&
           externalRamPerfScenesBank.magic[2] == 'X' && externalRamPerfScenesBank.magic[3] == 'S' &&
           externalRamPerfScenesBank.version == kPerfScenesFileVersion &&
           externalRamPerfScenesBank.count == kPerfScenesCount;
}

void PerformanceFxScenes::clampSongSlotScenes()
{
    for (uint8_t i = 0; i < SongListColumn::kSongSlotCount; i++)
    {
        uint8_t value = externalRamPerfScenesBank.songSlotScene[i];
        if (value > kPerfScenesCount)
        {
            externalRamPerfScenesBank.songSlotScene[i] = kPerfScenesNone;
        }
    }
    externalRamPerfScenesBank.songSlotScene[SongListColumn::kSongSlotCount] = 0;
}

bool PerformanceFxScenes::readFile(const char* path)
{
    if (ModuleSidecar::read(
          fileSystem, path, &externalRamPerfScenesBank, sizeof(externalRamPerfScenesBank)))
    {
        if (!validateHeader())
        {
            clearBank();
            return false;
        }

        clampSongSlotScenes();
        return true;
    }

    strPerfScenesFileV1 legacyBank;
    if (!ModuleSidecar::read(fileSystem, path, &legacyBank, sizeof(legacyBank)))
    {
        return false;
    }

    if (!(legacyBank.magic[0] == 'P' && legacyBank.magic[1] == 'F' && legacyBank.magic[2] == 'X' &&
          legacyBank.magic[3] == 'S' && legacyBank.version == kPerfScenesFileVersionV1 &&
          legacyBank.count == kPerfScenesCount))
    {
        clearBank();
        return false;
    }

    clearBank();
    memcpy(externalRamPerfScenesBank.scenes, legacyBank.scenes, sizeof(legacyBank.scenes));
    return true;
}

void PerformanceFxScenes::writeFile(const char* path)
{
    externalRamPerfScenesBank.magic[0] = 'P';
    externalRamPerfScenesBank.magic[1] = 'F';
    externalRamPerfScenesBank.magic[2] = 'X';
    externalRamPerfScenesBank.magic[3] = 'S';
    externalRamPerfScenesBank.version = kPerfScenesFileVersion;
    externalRamPerfScenesBank.count = kPerfScenesCount;
    clampSongSlotScenes();

    ModuleSidecar::write(
      fileSystem, path, &externalRamPerfScenesBank, sizeof(externalRamPerfScenesBank));
}

void PerformanceFxScenes::saveToWorkspace()
{
    char path[ModuleSidecar::kMaxPathLength];
    ModuleSidecar::workspacePath(path, sizeof(path), id());
    writeFile(path);
}

bool PerformanceFxScenes::isFilled(uint8_t idx) const
{
    if (idx >= kPerfScenesCount)
    {
        return false;
    }
    return externalRamPerfScenesBank.scenes[idx].name[0] != '\0';
}

void PerformanceFxScenes::storeScene(PerformancePageHost& host, uint8_t idx, const char* name)
{
    if (idx >= kPerfScenesCount)
    {
        return;
    }

    strPerfScene& scene = externalRamPerfScenesBank.scenes[idx];
    host.captureFxScene(scene);

    memset(scene.name, 0, sizeof(scene.name));
    if (name != nullptr)
    {
        strncpy(scene.name, name, kPerfFxSceneNameLength - 1);
    }

    saveToWorkspace();
}

bool PerformanceFxScenes::recallScene(PerformancePageHost& host, uint8_t idx)
{
    if (idx >= kPerfScenesCount || !isFilled(idx))
    {
        return false;
    }

    host.recallFxScene(externalRamPerfScenesBank.scenes[idx]);
    return true;
}

void PerformanceFxScenes::assignSlot(uint8_t slot, uint8_t scene)
{
    if (slot >= SongListColumn::kSongSlotCount)
    {
        return;
    }

    if (scene > kPerfScenesCount)
    {
        scene = kPerfScenesNone;
    }

    if (externalRamPerfScenesBank.songSlotScene[slot] == scene)
    {
        return;
    }

    externalRamPerfScenesBank.songSlotScene[slot] = scene;
    saveToWorkspace();
}

void PerformanceFxScenes::formatCell(uint8_t slot, char* out, uint8_t outSize) const
{
    if (out == nullptr || outSize == 0)
    {
        return;
    }

    out[0] = '\0';
    if (slot >= SongListColumn::kSongSlotCount)
    {
        return;
    }

    const uint8_t value = externalRamPerfScenesBank.songSlotScene[slot];
    if (value == kPerfScenesNone || value > kPerfScenesCount)
    {
        snprintf(out, outSize, "-");
        return;
    }

    snprintf(out, outSize, "%u", static_cast<unsigned>(value));
}

void PerformanceFxScenes::onEncoder(SongScreenHost& host, uint8_t slot, int8_t delta)
{
    (void)host;
    if (slot >= SongListColumn::kSongSlotCount || delta == 0)
    {
        return;
    }

    int16_t value = externalRamPerfScenesBank.songSlotScene[slot];
    value += delta;

    const int16_t maxValue = kPerfScenesCount;
    while (value < 0)
    {
        value += maxValue + 1;
    }
    while (value > maxValue)
    {
        value -= maxValue + 1;
    }

    assignSlot(slot, static_cast<uint8_t>(value));
}

void PerformanceFxScenes::onClear(SongScreenHost& host, uint8_t slot)
{
    (void)host;
    assignSlot(slot, kPerfScenesNone);
}

void PerformanceFxScenes::onInserted(uint8_t index, bool duplicate)
{
    if (index >= SongListColumn::kSongSlotCount - 1)
    {
        return;
    }

    for (int i = SongListColumn::kSongSlotCount - 1; i > index; i--)
    {
        externalRamPerfScenesBank.songSlotScene[i] = externalRamPerfScenesBank.songSlotScene[i - 1];
    }

    const uint8_t insertAt = static_cast<uint8_t>(index + 1);
    if (duplicate)
    {
        externalRamPerfScenesBank.songSlotScene[insertAt] =
          externalRamPerfScenesBank.songSlotScene[index];
    }
    else
    {
        externalRamPerfScenesBank.songSlotScene[insertAt] = kPerfScenesNone;
    }

    saveToWorkspace();
}

void PerformanceFxScenes::onRemoved(uint8_t index)
{
    if (index >= SongListColumn::kSongSlotCount)
    {
        return;
    }

    for (uint8_t i = index; i < SongListColumn::kSongSlotCount - 1; i++)
    {
        externalRamPerfScenesBank.songSlotScene[i] = externalRamPerfScenesBank.songSlotScene[i + 1];
    }
    externalRamPerfScenesBank.songSlotScene[SongListColumn::kSongSlotCount - 1] = kPerfScenesNone;

    saveToWorkspace();
}

void PerformanceFxScenes::clearAll()
{
    memset(externalRamPerfScenesBank.songSlotScene,
           0,
           sizeof(externalRamPerfScenesBank.songSlotScene));
    saveToWorkspace();
}

void PerformanceFxScenes::onSlotStart(uint8_t slot)
{
    if (slot >= SongListColumn::kSongSlotCount)
    {
        return;
    }

    const uint8_t scene = externalRamPerfScenesBank.songSlotScene[slot];
    if (scene == kPerfScenesNone || scene > kPerfScenesCount)
    {
        return;
    }

    PerformancePageHost& host = getPerformancePageHost();
    if (!recallScene(host, static_cast<uint8_t>(scene - 1)))
    {
        return;
    }

    if (host.isShowingFxRack())
    {
        host.refreshFxRack();
    }
}

void PerformanceFxScenes::onShow(PerformancePageHost& host)
{
    static const char* const sceneSlotLabels[kPerfScenesCount] = {
        "1", "2", "3", "4", "5", "6", "7", "8"
    };

    host.setPageTitle(title());
    for (uint8_t i = 0; i < kPerfScenesCount; i++)
    {
        host.setScreenLabel(i, sceneSlotLabels[i], isFilled(i));
    }
    host.finishScreenLabels();
}

void PerformanceFxScenes::onHide(PerformancePageHost& host)
{
    host.restoreTrackLabels();
}

void PerformanceFxScenes::onScreenButton(PerformancePageHost& host, uint8_t button, uint8_t state)
{
    if (state != kPerformanceButtonPress || button >= kPerfScenesCount)
    {
        return;
    }

    if (host.isShiftPressed())
    {
        char name[kPerfFxSceneNameLength];
        snprintf(name, sizeof(name), "Scene %d", button + 1);
        storeScene(host, button, name);
        onShow(host);
    }
    else if (recallScene(host, button))
    {
        host.refreshFxRack();
    }
}
