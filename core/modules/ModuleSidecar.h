#ifndef MODULES_MODULESIDECAR_H_
#define MODULES_MODULESIDECAR_H_

#include <stddef.h>
#include <stdint.h>

class FileSystem;

// Builds Workspace/<id>.bin and <projectPath>/<projectName>/<id>.bin paths,
// and reads/writes one 4-byte-aligned blob. Magic/version validation is owned
// by the calling module.
class ModuleSidecar
{
  public:
    static constexpr size_t kMaxPathLength = 255;

    static void workspacePath(char* out, size_t outSize, const char* id);
    static void projectPath(char* out,
                            size_t outSize,
                            const char* projectPath,
                            const char* projectName,
                            const char* id);

    static bool read(FileSystem* fs, const char* path, void* dest, size_t size);
    static bool write(FileSystem* fs, const char* path, const void* src, size_t size);
};

#endif /* MODULES_MODULESIDECAR_H_ */
