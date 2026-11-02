#include "perfScenes.h"

#include "Platform.h"
#include "fileManager.h"
#include "fileManagerDefs.h"
#include "performanceEngine.h"

#include <cr_section_macros.h>
#include <cstdio>
#include <cstring>
#include <tuple>

__NOINIT(EXTERNAL_RAM) static strPerfScenesFile externalRamPerfScenesBank;

PerfScenes perfScenes;

void PerfScenes::init(FileSystem* fileSystem)
{
    this->fileSystem = fileSystem;
    clearBank();
}

void PerfScenes::clearBank()
{
    memset(&externalRamPerfScenesBank, 0, sizeof(externalRamPerfScenesBank));
    externalRamPerfScenesBank.magic[0] = 'P';
    externalRamPerfScenesBank.magic[1] = 'F';
    externalRamPerfScenesBank.magic[2] = 'X';
    externalRamPerfScenesBank.magic[3] = 'S';
    externalRamPerfScenesBank.version = kPerfScenesFileVersion;
    externalRamPerfScenesBank.count = kPerfScenesCount;
}

bool PerfScenes::validateHeader() const
{
    return externalRamPerfScenesBank.magic[0] == 'P' && externalRamPerfScenesBank.magic[1] == 'F' &&
           externalRamPerfScenesBank.magic[2] == 'X' && externalRamPerfScenesBank.magic[3] == 'S' &&
           externalRamPerfScenesBank.version == kPerfScenesFileVersion &&
           externalRamPerfScenesBank.count == kPerfScenesCount;
}

bool PerfScenes::readFile(const char* path)
{
    if (fileSystem == nullptr || path == nullptr)
    {
        return false;
    }

    if (!fileSystem->exists(path))
    {
        return false;
    }

    auto openFileInfo = fileSystem->openFile(path);
    auto isOpen = std::get<0>(openFileInfo);
    auto file = std::get<1>(openFileInfo);

    if (!isOpen)
    {
        return false;
    }

    size_t read = fileSystem->readFile(&externalRamPerfScenesBank, sizeof(externalRamPerfScenesBank), file);
    fileSystem->closeFile(file);

    if (read != sizeof(externalRamPerfScenesBank) || !validateHeader())
    {
        clearBank();
        return false;
    }

    return true;
}

void PerfScenes::writeFile(const char* path)
{
    if (fileSystem == nullptr || path == nullptr)
    {
        return;
    }

    externalRamPerfScenesBank.magic[0] = 'P';
    externalRamPerfScenesBank.magic[1] = 'F';
    externalRamPerfScenesBank.magic[2] = 'X';
    externalRamPerfScenesBank.magic[3] = 'S';
    externalRamPerfScenesBank.version = kPerfScenesFileVersion;
    externalRamPerfScenesBank.count = kPerfScenesCount;

    newFileManager.createWorkspaceDirs();

    if (fileSystem->exists(path))
    {
        fileSystem->remove(path);
    }

    auto openFileInfo = fileSystem->openFile(path, FileSystem::FileOpenMode::kWriteOpen);
    auto isOpen = std::get<0>(openFileInfo);
    auto file = std::get<1>(openFileInfo);

    if (isOpen)
    {
        fileSystem->writeFile(&externalRamPerfScenesBank, sizeof(externalRamPerfScenesBank), file);
        fileSystem->closeFile(file);
    }
}

void PerfScenes::loadFromWorkspace()
{
    if (!readFile(cWorkspacePerfScenesFilePath))
    {
        clearBank();
        if (fileSystem != nullptr && fileSystem->exists(cWorkspacePerfScenesFilePath))
        {
            fileSystem->remove(cWorkspacePerfScenesFilePath);
        }
    }
}

void PerfScenes::saveToWorkspace()
{
    writeFile(cWorkspacePerfScenesFilePath);
}

void PerfScenes::loadFromProject(const char* projectPath, const char* projectName)
{
    char srcPath[PATCH_SIZE];
    snprintf(srcPath,
             sizeof(srcPath),
             cProjectsPerfScenesFileFormat,
             projectPath,
             projectName);

    if (!readFile(srcPath))
    {
        clearBank();
        if (fileSystem != nullptr && fileSystem->exists(cWorkspacePerfScenesFilePath))
        {
            fileSystem->remove(cWorkspacePerfScenesFilePath);
        }
        return;
    }

    writeFile(cWorkspacePerfScenesFilePath);
}

void PerfScenes::copyWorkspaceToProject(const char* projectPath, const char* projectName)
{
    if (fileSystem == nullptr)
    {
        return;
    }

    if (!fileSystem->exists(cWorkspacePerfScenesFilePath))
    {
        return;
    }

    if (!readFile(cWorkspacePerfScenesFilePath))
    {
        return;
    }

    char destPath[PATCH_SIZE];
    snprintf(destPath,
             sizeof(destPath),
             cProjectsPerfScenesFileFormat,
             projectPath,
             projectName);

    writeFile(destPath);
}

strPerfScene& PerfScenes::getScene(uint8_t idx)
{
    if (idx >= kPerfScenesCount)
    {
        idx = 0;
    }
    return externalRamPerfScenesBank.scenes[idx];
}

const strPerfScene& PerfScenes::getScene(uint8_t idx) const
{
    if (idx >= kPerfScenesCount)
    {
        idx = 0;
    }
    return externalRamPerfScenesBank.scenes[idx];
}

bool PerfScenes::isFilled(uint8_t idx) const
{
    if (idx >= kPerfScenesCount)
    {
        return false;
    }
    return externalRamPerfScenesBank.scenes[idx].name[0] != '\0';
}

void PerfScenes::storeScene(uint8_t idx, const char* name)
{
    if (idx >= kPerfScenesCount)
    {
        return;
    }

    strPerfScene& scene = externalRamPerfScenesBank.scenes[idx];
    performance.captureScene(scene);

    memset(scene.name, 0, sizeof(scene.name));
    if (name != nullptr)
    {
        strncpy(scene.name, name, kPerfSceneNameLength - 1);
    }

    saveToWorkspace();
}

bool PerfScenes::recallScene(uint8_t idx)
{
    if (idx >= kPerfScenesCount || !isFilled(idx))
    {
        return false;
    }

    performance.recallScene(externalRamPerfScenesBank.scenes[idx]);
    return true;
}
