#include "PerformanceFxScenes.h"

#include "ModuleSidecar.h"
#include "Platform.h"

#include <cr_section_macros.h>
#include <cstdio>
#include <cstring>

__NOINIT(EXTERNAL_RAM) static strPerfScenesFile externalRamPerfScenesBank;

PerformanceFxScenes performanceFxScenes;

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
    activeSlot = 0xFF;
}

void PerformanceFxScenes::setActiveSlot(uint8_t idx)
{
    activeSlot = (idx < kPerfScenesCount) ? idx : 0xFF;
}

void PerformanceFxScenes::refreshPage(PerformancePageHost& host)
{
    static const char* const sceneSlotLabels[kPerfScenesCount] = {
        "1", "2", "3", "4", "5", "6", "7", "8"
    };

    if (activeSlot < kPerfScenesCount)
    {
        // TitleBar stores a pointer — keep this buffer stable.
        static char titleBuf[24];
        snprintf(titleBuf, sizeof(titleBuf), "Scenes - %u", static_cast<unsigned>(activeSlot + 1));
        host.setPageTitle(titleBuf);
    }
    else
    {
        host.setPageTitle(title());
    }

    for (uint8_t i = 0; i < kPerfScenesCount; i++)
    {
        PerformanceLabelState state = PerformanceLabelState::Empty;
        if (i == activeSlot)
        {
            state = PerformanceLabelState::Active;
        }
        else if (isFilled(i))
        {
            state = PerformanceLabelState::Filled;
        }
        host.setScreenLabel(i, sceneSlotLabels[i], state);
    }
    host.finishScreenLabels();
}

bool PerformanceFxScenes::validateHeader() const
{
    return externalRamPerfScenesBank.magic[0] == 'P' && externalRamPerfScenesBank.magic[1] == 'F' &&
           externalRamPerfScenesBank.magic[2] == 'X' && externalRamPerfScenesBank.magic[3] == 'S' &&
           externalRamPerfScenesBank.version == kPerfScenesFileVersion &&
           externalRamPerfScenesBank.count == kPerfScenesCount;
}

bool PerformanceFxScenes::readFile(const char* path)
{
    // v1 and v3 share the same blob size (scenes only).
    strPerfScenesFile scenesOnly;
    if (ModuleSidecar::read(fileSystem, path, &scenesOnly, sizeof(scenesOnly)))
    {
        if (scenesOnly.magic[0] == 'P' && scenesOnly.magic[1] == 'F' && scenesOnly.magic[2] == 'X' &&
            scenesOnly.magic[3] == 'S' && scenesOnly.count == kPerfScenesCount &&
            (scenesOnly.version == kPerfScenesFileVersion ||
             scenesOnly.version == kPerfScenesFileVersionV1))
        {
            clearBank();
            memcpy(externalRamPerfScenesBank.scenes, scenesOnly.scenes, sizeof(scenesOnly.scenes));
            return true;
        }
    }

    strPerfScenesFileV2 legacyV2;
    if (ModuleSidecar::read(fileSystem, path, &legacyV2, sizeof(legacyV2)))
    {
        if (legacyV2.magic[0] == 'P' && legacyV2.magic[1] == 'F' && legacyV2.magic[2] == 'X' &&
            legacyV2.magic[3] == 'S' && legacyV2.version == kPerfScenesFileVersionV2 &&
            legacyV2.count == kPerfScenesCount)
        {
            clearBank();
            memcpy(externalRamPerfScenesBank.scenes, legacyV2.scenes, sizeof(legacyV2.scenes));
            return true;
        }
    }

    clearBank();
    return false;
}

void PerformanceFxScenes::writeFile(const char* path)
{
    externalRamPerfScenesBank.magic[0] = 'P';
    externalRamPerfScenesBank.magic[1] = 'F';
    externalRamPerfScenesBank.magic[2] = 'X';
    externalRamPerfScenesBank.magic[3] = 'S';
    externalRamPerfScenesBank.version = kPerfScenesFileVersion;
    externalRamPerfScenesBank.count = kPerfScenesCount;

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
    setActiveSlot(idx);
    return true;
}

void PerformanceFxScenes::onShow(PerformancePageHost& host)
{
    refreshPage(host);
}

void PerformanceFxScenes::onHide(PerformancePageHost& host)
{
    (void)host;
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
        refreshPage(host);
    }
    else if (recallScene(host, button))
    {
        host.refreshFxRack();
        refreshPage(host);
    }
}
