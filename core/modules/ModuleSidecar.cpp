#include "ModuleSidecar.h"

#include "Platform.h"
#include "fileManager.h"

#include <cstdio>
#include <cstring>
#include <tuple>

void ModuleSidecar::workspacePath(char* out, size_t outSize, const char* id)
{
    if (out == nullptr || outSize == 0)
    {
        return;
    }

    if (id == nullptr)
    {
        out[0] = '\0';
        return;
    }

    snprintf(out, outSize, "Workspace/%s.bin", id);
}

void ModuleSidecar::projectPath(char* out,
                                size_t outSize,
                                const char* projectPath,
                                const char* projectName,
                                const char* id)
{
    if (out == nullptr || outSize == 0)
    {
        return;
    }

    if (projectPath == nullptr || projectName == nullptr || id == nullptr)
    {
        out[0] = '\0';
        return;
    }

    snprintf(out, outSize, "%s/%s/%s.bin", projectPath, projectName, id);
}

bool ModuleSidecar::read(FileSystem* fs, const char* path, void* dest, size_t size)
{
    if (fs == nullptr || path == nullptr || dest == nullptr || size == 0)
    {
        return false;
    }

    if (!fs->exists(path))
    {
        return false;
    }

    auto openFileInfo = fs->openFile(path);
    auto isOpen = std::get<0>(openFileInfo);
    auto file = std::get<1>(openFileInfo);

    if (!isOpen)
    {
        return false;
    }

    size_t read = fs->readFile(dest, size, file);
    fs->closeFile(file);
    return read == size;
}

bool ModuleSidecar::write(FileSystem* fs, const char* path, const void* src, size_t size)
{
    if (fs == nullptr || path == nullptr || src == nullptr || size == 0)
    {
        return false;
    }

    newFileManager.createWorkspaceDirs();

    if (fs->exists(path))
    {
        fs->remove(path);
    }

    auto openFileInfo = fs->openFile(path, FileSystem::FileOpenMode::kWriteOpen);
    auto isOpen = std::get<0>(openFileInfo);
    auto file = std::get<1>(openFileInfo);

    if (!isOpen)
    {
        return false;
    }

    size_t written = fs->writeFile(src, size, file);
    fs->closeFile(file);
    return written == size;
}
