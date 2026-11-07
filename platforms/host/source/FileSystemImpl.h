#ifndef SOURCE_FILESYSTEMIMPL_H_
#define SOURCE_FILESYSTEMIMPL_H_

#include "Platform.h"
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

class FileSystemImpl final : public FileSystem
{
  public:
    void init(const std::string& rootPath);
    void update();

    virtual bool isSdCardInserted() override;
    virtual void setMountEventCallback(CardStateChangeCallback funct) override;

    virtual bool exists(const char* path) override;
    virtual bool rename(const char* oldPath, const char* newPath) override;
    virtual bool mkdir(uint8_t hidden, const char* path, bool pFlag = true) override;
    virtual bool remove(const char* path) override;
    virtual bool removeDirWithFiles(const char* path) override;
    virtual bool removeOnlyDirFiles(const char* path) override;

    virtual size_t getFileSize(const char* path) override;
    virtual size_t getFileSize(FileHandle fileHandle) override;
    virtual size_t getFileAvailable(FileHandle fileHandle) override;
    virtual std::tuple<bool, FileHandle> openFile(const char* filepath,
                                                  FileOpenMode openMode = kRead,
                                                  FileHandle* fileHandle = nullptr) override;
    virtual size_t readFile(void* destBuf, size_t bufLen, FileHandle file) override;
    virtual size_t writeFile(const void* data, size_t dataSize, FileHandle fileHandle) override;
    virtual bool seekFileCur(int offset, FileHandle fileHandle) override;
    virtual bool seekFile(uint32_t pos, FileHandle fileHandle) override;
    virtual void closeFile(FileHandle file, bool deleteOnClose = true) override;
    virtual void endFile(FileHandle file) override;
    virtual bool isFileOpen(FileHandle file) override;
    virtual bool enableFastSeek(FileHandle fileHandle,
                                uint32_t* clmtBuffer,
                                uint32_t clmtElementCount,
                                uint32_t* outDatabaseSector = nullptr,
                                uint32_t* outSectorsPerCluster = nullptr) override;
    virtual bool readSectorsAsync(uint8_t* buffer, uint32_t startSector, uint32_t sectorCount) override;
    virtual bool pollAsyncRead(bool* outSuccess = nullptr) override;
    virtual bool waitAsyncRead() override;
    virtual bool isAsyncReadBusy() override;

    virtual int createInstrumentsList(const char* path,
                                      char** list,
                                      uint8_t list_length,
                                      uint32_t* sizeOut) override;
    virtual int countContainedFiles(const char* path) override;
    virtual int createFilesList(const char* path,
                                uint8_t start_line,
                                char** list,
                                uint8_t list_length,
                                uint16_t max_used_memory,
                                enListFileFilter filter = enListFileFilter_all,
                                uint16_t skip_count = 0,
                                uint16_t max_files_in_subdir = 0,
                                enListFileFilter folderSizeFilter = enListFileFilter_all) override;
    virtual void listDirContents(const char* dirpath,
                                 DirContentType contentType,
                                 std::function<bool(const char* name)> nameFilter,
                                 char* namesBuffer,
                                 int namesBufferSize,
                                 int maxNameLen,
                                 int& outNamesCount,
                                 char** fileList) override;
    virtual int readNbFiles(const char* romPath) override;
    virtual bool nesDir_open(const char* path) override;
    virtual bool nesDir_readItem(char* path, uint8_t* isDir, uint8_t* isHid, uint32_t* size) override;
    virtual void nesDir_close() override;

  private:
    struct HostFile
    {
        FILE* fp = nullptr;
        std::string path;
        std::string absPath;
        bool fastSeek = false;
        uint32_t baseSector = 0;
    };

    std::string root;
    CardStateChangeCallback mountCallback = nullptr;
    bool mounted = false;
    std::unordered_map<HostFile*, bool> openFiles;

    // Async sector read state keyed by enableFastSeek file
    HostFile* asyncFile = nullptr;
    uint8_t* asyncBuffer = nullptr;
    uint32_t asyncStartSector = 0;
    uint32_t asyncSectorCount = 0;
    bool asyncBusy = false;
    bool asyncDone = false;
    bool asyncOk = false;

    std::string resolve(const char* path) const;
    HostFile* asFile(FileHandle h) const;
    bool matchFilter(enListFileFilter filter, const std::string& name, bool isDir) const;
};

#endif
