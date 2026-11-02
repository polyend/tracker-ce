#ifndef INTERFACE_PERFORMANCEMODE_PERFSCENES_H_
#define INTERFACE_PERFORMANCEMODE_PERFSCENES_H_

#include <stdint.h>

class FileSystem;

static constexpr uint8_t kPerfScenesCount = 8;
static constexpr uint16_t kPerfScenesFileVersion = 1;
static constexpr uint8_t kPerfSceneNameLength = 16;

struct strPerfScene
{
    char name[kPerfSceneNameLength];
    uint8_t fxPlaces[12];
    uint8_t selectedValues[12];
    int16_t fxValues[12][4];
    int16_t liveValues[12];
};

static_assert(sizeof(strPerfScene) % 4 == 0, "strPerfScene must be 4-byte aligned for FatFs DMA");

struct strPerfScenesFile
{
    char magic[4];
    uint16_t version;
    uint16_t count;
    strPerfScene scenes[kPerfScenesCount];
};

static_assert(sizeof(strPerfScenesFile) % 4 == 0, "strPerfScenesFile must be 4-byte aligned for FatFs DMA");

class PerfScenes
{
  public:
    void init(FileSystem* fileSystem);

    void clearBank();
    void loadFromWorkspace();
    void saveToWorkspace();
    void loadFromProject(const char* projectPath, const char* projectName);
    void copyWorkspaceToProject(const char* projectPath, const char* projectName);

    strPerfScene& getScene(uint8_t idx);
    const strPerfScene& getScene(uint8_t idx) const;
    bool isFilled(uint8_t idx) const;

    void storeScene(uint8_t idx, const char* name);
    bool recallScene(uint8_t idx);

  private:
    bool readFile(const char* path);
    void writeFile(const char* path);
    bool validateHeader() const;

    FileSystem* fileSystem = nullptr;
};

extern PerfScenes perfScenes;

#endif /* INTERFACE_PERFORMANCEMODE_PERFSCENES_H_ */
