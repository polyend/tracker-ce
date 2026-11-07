#include "MuteBanks.h"

#include "ModuleSidecar.h"
#include "Platform.h"

#include <cr_section_macros.h>
#include <cstdio>
#include <cstring>

__NOINIT(EXTERNAL_RAM) static strMuteBanksFile externalRamMuteBanksBank;

MuteBanks muteBanks;

void MuteBanks::init(FileSystem* fileSystem)
{
    this->fileSystem = fileSystem;
    clearBank();
}

void MuteBanks::onWorkspaceClear()
{
    clearBank();
}

void MuteBanks::onWorkspaceLoad()
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

void MuteBanks::onProjectOpen(const char* projectPath, const char* projectName)
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

void MuteBanks::onProjectSave(const char* projectPath, const char* projectName)
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

void MuteBanks::clearBank()
{
    memset(&externalRamMuteBanksBank, 0, sizeof(externalRamMuteBanksBank));
    externalRamMuteBanksBank.magic[0] = 'M';
    externalRamMuteBanksBank.magic[1] = 'U';
    externalRamMuteBanksBank.magic[2] = 'T';
    externalRamMuteBanksBank.magic[3] = 'E';
    externalRamMuteBanksBank.version = kMuteBanksFileVersion;
    externalRamMuteBanksBank.count = kMuteBanksCount;
}

bool MuteBanks::validateHeader() const
{
    return externalRamMuteBanksBank.magic[0] == 'M' && externalRamMuteBanksBank.magic[1] == 'U' &&
           externalRamMuteBanksBank.magic[2] == 'T' && externalRamMuteBanksBank.magic[3] == 'E' &&
           externalRamMuteBanksBank.version == kMuteBanksFileVersion &&
           externalRamMuteBanksBank.count == kMuteBanksCount;
}

bool MuteBanks::readFile(const char* path)
{
    if (!ModuleSidecar::read(
          fileSystem, path, &externalRamMuteBanksBank, sizeof(externalRamMuteBanksBank)))
    {
        return false;
    }

    if (!validateHeader())
    {
        clearBank();
        return false;
    }

    return true;
}

void MuteBanks::writeFile(const char* path)
{
    externalRamMuteBanksBank.magic[0] = 'M';
    externalRamMuteBanksBank.magic[1] = 'U';
    externalRamMuteBanksBank.magic[2] = 'T';
    externalRamMuteBanksBank.magic[3] = 'E';
    externalRamMuteBanksBank.version = kMuteBanksFileVersion;
    externalRamMuteBanksBank.count = kMuteBanksCount;

    ModuleSidecar::write(
      fileSystem, path, &externalRamMuteBanksBank, sizeof(externalRamMuteBanksBank));
}

void MuteBanks::saveToWorkspace()
{
    char path[ModuleSidecar::kMaxPathLength];
    ModuleSidecar::workspacePath(path, sizeof(path), id());
    writeFile(path);
}

bool MuteBanks::isFilled(uint8_t idx) const
{
    if (idx >= kMuteBanksCount)
    {
        return false;
    }
    return externalRamMuteBanksBank.scenes[idx].name[0] != '\0';
}

void MuteBanks::storeScene(PerformancePageHost& host, uint8_t idx, const char* name)
{
    if (idx >= kMuteBanksCount)
    {
        return;
    }

    strMuteScene& scene = externalRamMuteBanksBank.scenes[idx];
    host.captureMuteScene(scene);

    memset(scene.name, 0, sizeof(scene.name));
    if (name != nullptr)
    {
        strncpy(scene.name, name, kPerfFxSceneNameLength - 1);
    }

    saveToWorkspace();
}

bool MuteBanks::recallScene(PerformancePageHost& host, uint8_t idx)
{
    if (idx >= kMuteBanksCount || !isFilled(idx))
    {
        return false;
    }

    host.recallMuteScene(externalRamMuteBanksBank.scenes[idx]);
    return true;
}

void MuteBanks::onShow(PerformancePageHost& host)
{
    static const char* const slotLabels[kMuteBanksCount] = {
        "1", "2", "3", "4", "5", "6", "7", "8"
    };

    host.setPageTitle(title());
    for (uint8_t i = 0; i < kMuteBanksCount; i++)
    {
        host.setScreenLabel(i, slotLabels[i], isFilled(i));
    }
    host.finishScreenLabels();
}

void MuteBanks::onHide(PerformancePageHost& host)
{
    host.restoreTrackLabels();
}

void MuteBanks::onScreenButton(PerformancePageHost& host, uint8_t button, uint8_t state)
{
    if (state != kPerformanceButtonPress || button >= kMuteBanksCount)
    {
        return;
    }

    if (host.isShiftPressed())
    {
        char name[kPerfFxSceneNameLength];
        snprintf(name, sizeof(name), "Mute %d", button + 1);
        storeScene(host, button, name);
        onShow(host);
    }
    else
    {
        recallScene(host, button);
    }
}
