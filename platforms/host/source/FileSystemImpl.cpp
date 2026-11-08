#include "FileSystemImpl.h"
#include <algorithm>
#include <cstring>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace
{
bool endsWithIgnoreCase(const std::string& s, const char* suffix)
{
    const size_t n = std::strlen(suffix);
    if (s.size() < n)
        return false;
    for (size_t i = 0; i < n; ++i)
    {
        char a = s[s.size() - n + i];
        char b = suffix[i];
        if (a >= 'A' && a <= 'Z')
            a = static_cast<char>(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z')
            b = static_cast<char>(b - 'A' + 'a');
        if (a != b)
            return false;
    }
    return true;
}
} // namespace

void FileSystemImpl::init(const std::string& rootPath)
{
    root = rootPath;
    while (!root.empty() && root.back() == '/')
        root.pop_back();
    ::mkdir(root.c_str(), 0755);
    ::mkdir((root + "/Projects").c_str(), 0755);
    ::mkdir((root + "/Workspace").c_str(), 0755);
    ::mkdir((root + "/Firmware").c_str(), 0755);
    ::mkdir((root + "/Effects").c_str(), 0755);
    mounted = true;
    if (mountCallback)
        mountCallback(true);
}

void FileSystemImpl::update() {}

std::string FileSystemImpl::resolve(const char* path) const
{
    if (path == nullptr || path[0] == '\0')
        return root;
    if (path[0] == '/')
        return root + path;
    return root + "/" + path;
}

FileSystemImpl::HostFile* FileSystemImpl::asFile(FileHandle h) const
{
    return static_cast<HostFile*>(h);
}

bool FileSystemImpl::isSdCardInserted()
{
    return mounted;
}

void FileSystemImpl::setMountEventCallback(CardStateChangeCallback funct)
{
    mountCallback = funct;
}

bool FileSystemImpl::exists(const char* path)
{
    struct stat st{};
    return ::stat(resolve(path).c_str(), &st) == 0;
}

bool FileSystemImpl::rename(const char* oldPath, const char* newPath)
{
    return ::rename(resolve(oldPath).c_str(), resolve(newPath).c_str()) == 0;
}

bool FileSystemImpl::mkdir(uint8_t /*hidden*/, const char* path, bool pFlag)
{
    std::string abs = resolve(path);
    if (!pFlag)
        return ::mkdir(abs.c_str(), 0755) == 0 || errno == EEXIST;

    // Create each path segment without destroying separators in `cur`.
    std::string cur;
    for (size_t i = 0; i < abs.size(); ++i)
    {
        cur.push_back(abs[i]);
        const bool atSep = abs[i] == '/';
        const bool atEnd = i + 1 == abs.size();
        if (!atSep && !atEnd)
            continue;

        std::string segment = cur;
        if (segment.size() > 1 && segment.back() == '/')
            segment.pop_back();
        if (!segment.empty() && segment != "/")
            (void)::mkdir(segment.c_str(), 0755);
    }
    return exists(path);
}

bool FileSystemImpl::remove(const char* path)
{
    return ::unlink(resolve(path).c_str()) == 0;
}

bool FileSystemImpl::removeDirWithFiles(const char* path)
{
    removeOnlyDirFiles(path);
    return ::rmdir(resolve(path).c_str()) == 0 || !exists(path);
}

bool FileSystemImpl::removeOnlyDirFiles(const char* path)
{
    DIR* dir = opendir(resolve(path).c_str());
    if (!dir)
        return false;
    while (dirent* ent = readdir(dir))
    {
        if (std::strcmp(ent->d_name, ".") == 0 || std::strcmp(ent->d_name, "..") == 0)
            continue;
        std::string child = std::string(path) + "/" + ent->d_name;
        struct stat st{};
        if (::stat(resolve(child.c_str()).c_str(), &st) == 0 && S_ISDIR(st.st_mode))
            removeDirWithFiles(child.c_str());
        else
            remove(child.c_str());
    }
    closedir(dir);
    return true;
}

size_t FileSystemImpl::getFileSize(const char* path)
{
    struct stat st{};
    if (::stat(resolve(path).c_str(), &st) != 0)
        return 0;
    return static_cast<size_t>(st.st_size);
}

size_t FileSystemImpl::getFileSize(FileHandle fileHandle)
{
    HostFile* f = asFile(fileHandle);
    if (!f || !f->fp)
        return 0;
    long pos = std::ftell(f->fp);
    std::fseek(f->fp, 0, SEEK_END);
    long end = std::ftell(f->fp);
    std::fseek(f->fp, pos, SEEK_SET);
    return end < 0 ? 0 : static_cast<size_t>(end);
}

size_t FileSystemImpl::getFileAvailable(FileHandle fileHandle)
{
    HostFile* f = asFile(fileHandle);
    if (!f || !f->fp)
        return 0;
    long pos = std::ftell(f->fp);
    if (pos < 0)
        return 0;
    size_t size = getFileSize(fileHandle);
    if (static_cast<size_t>(pos) >= size)
        return 0;
    return size - static_cast<size_t>(pos);
}

std::tuple<bool, FileSystem::FileHandle> FileSystemImpl::openFile(const char* filepath,
                                                                  FileOpenMode openMode,
                                                                  FileHandle* fileHandle)
{
    HostFile* f = nullptr;
    bool created = false;
    if (fileHandle && *fileHandle)
    {
        f = asFile(*fileHandle);
    }
    else
    {
        f = new HostFile();
        created = true;
    }

    const char* mode = "rb";
    switch (openMode)
    {
        case kRead:
            mode = "rb";
            break;
        case kWriteOpen:
            mode = "r+b";
            break;
        case kWriteAppend:
            mode = "ab";
            break;
        case kWriteCreateNew:
            mode = "wb";
            break;
    }

    f->path = filepath ? filepath : "";
    f->absPath = resolve(filepath);
    f->fp = std::fopen(f->absPath.c_str(), mode);
    if (!f->fp && openMode == kWriteOpen)
        f->fp = std::fopen(f->absPath.c_str(), "w+b");
    if (!f->fp)
    {
        if (created)
            delete f;
        return { false, nullptr };
    }

    // Match device FS: stash the handle so callers that pass &transferFile see it.
    if (fileHandle && *fileHandle == nullptr)
        *fileHandle = f;

    openFiles[f] = true;
    return { true, f };
}

size_t FileSystemImpl::readFile(void* destBuf, size_t bufLen, FileHandle file)
{
    HostFile* f = asFile(file);
    if (!f || !f->fp)
        return 0;
    return std::fread(destBuf, 1, bufLen, f->fp);
}

size_t FileSystemImpl::writeFile(const void* data, size_t dataSize, FileHandle fileHandle)
{
    HostFile* f = asFile(fileHandle);
    if (!f || !f->fp)
        return 0;
    return std::fwrite(data, 1, dataSize, f->fp);
}

bool FileSystemImpl::seekFileCur(int offset, FileHandle fileHandle)
{
    HostFile* f = asFile(fileHandle);
    if (!f || !f->fp)
        return false;
    return std::fseek(f->fp, offset, SEEK_CUR) == 0;
}

bool FileSystemImpl::seekFile(uint32_t pos, FileHandle fileHandle)
{
    HostFile* f = asFile(fileHandle);
    if (!f || !f->fp)
        return false;
    return std::fseek(f->fp, static_cast<long>(pos), SEEK_SET) == 0;
}

void FileSystemImpl::closeFile(FileHandle file, bool deleteOnClose)
{
    HostFile* f = asFile(file);
    if (!f)
        return;
    if (f->fp)
    {
        std::fclose(f->fp);
        f->fp = nullptr;
    }
    openFiles.erase(f);
    if (asyncFile == f)
        asyncFile = nullptr;
    if (deleteOnClose)
        delete f;
}

void FileSystemImpl::endFile(FileHandle file)
{
    HostFile* f = asFile(file);
    if (f && f->fp)
        std::fflush(f->fp);
}

bool FileSystemImpl::isFileOpen(FileHandle file)
{
    HostFile* f = asFile(file);
    return f && f->fp;
}

bool FileSystemImpl::enableFastSeek(FileHandle fileHandle,
                                    uint32_t* /*clmtBuffer*/,
                                    uint32_t /*clmtElementCount*/,
                                    uint32_t* outDatabaseSector,
                                    uint32_t* outSectorsPerCluster)
{
    HostFile* f = asFile(fileHandle);
    if (!f || !f->fp)
        return false;
    f->fastSeek = true;
    f->baseSector = 0;
    if (outDatabaseSector)
        *outDatabaseSector = 0;
    if (outSectorsPerCluster)
        *outSectorsPerCluster = 1;
    asyncFile = f;
    return true;
}

bool FileSystemImpl::readSectorsAsync(uint8_t* buffer, uint32_t startSector, uint32_t sectorCount)
{
    if (!asyncFile || !asyncFile->fp || !buffer || sectorCount == 0)
        return false;
    asyncBuffer = buffer;
    asyncStartSector = startSector;
    asyncSectorCount = sectorCount;
    asyncBusy = true;
    asyncDone = false;
    asyncOk = false;
    return true;
}

bool FileSystemImpl::pollAsyncRead(bool* outSuccess)
{
    if (!asyncBusy)
        return true;
    if (!asyncDone)
    {
        // Complete immediately: map sector LBA onto file byte offset.
        const long offset = static_cast<long>(asyncStartSector) * 512L;
        asyncOk = false;
        if (asyncFile && asyncFile->fp && asyncBuffer)
        {
            if (std::fseek(asyncFile->fp, offset, SEEK_SET) == 0)
            {
                const size_t want = static_cast<size_t>(asyncSectorCount) * 512u;
                const size_t got = std::fread(asyncBuffer, 1, want, asyncFile->fp);
                asyncOk = (got == want) || (got > 0 && std::feof(asyncFile->fp));
                if (got < want)
                    std::memset(asyncBuffer + got, 0, want - got);
            }
        }
        asyncDone = true;
        asyncBusy = false;
    }
    if (outSuccess)
        *outSuccess = asyncOk;
    return true;
}

bool FileSystemImpl::waitAsyncRead()
{
    bool ok = false;
    pollAsyncRead(&ok);
    return ok;
}

bool FileSystemImpl::isAsyncReadBusy()
{
    return asyncBusy && !asyncDone;
}

bool FileSystemImpl::matchFilter(enListFileFilter filter, const std::string& name, bool isDir) const
{
    switch (filter)
    {
        case enListFileFilter_folder:
            return isDir;
        case enListFileFilter_wave:
            return !isDir && (endsWithIgnoreCase(name, ".wav") || endsWithIgnoreCase(name, ".wave"));
        case enListFileFilter_wave_inst:
            return !isDir &&
                   (endsWithIgnoreCase(name, ".wav") || endsWithIgnoreCase(name, ".pti") ||
                    endsWithIgnoreCase(name, ".wave"));
        case enListFileFilter_firmware:
            return !isDir && endsWithIgnoreCase(name, ".ptf");
        case enListFileFilter_mod:
            return !isDir &&
                   (endsWithIgnoreCase(name, ".mod") || endsWithIgnoreCase(name, ".it") ||
                    endsWithIgnoreCase(name, ".xm") || endsWithIgnoreCase(name, ".s3m"));
        case enListFileFilter_game:
            return !isDir && endsWithIgnoreCase(name, ".nes");
        case enListFileFilter_all:
        default:
            return true;
    }
}

int FileSystemImpl::createInstrumentsList(const char* path,
                                          char** list,
                                          uint8_t list_length,
                                          uint32_t* sizeOut)
{
    // Match device SdDir::createInstrumentsList: files are named "N name.pti"
    // (1-based slot). Store just the display name at list[slot-1].
    if (sizeOut)
        *sizeOut = 0;
    if (!list || list_length == 0)
        return 0;

    for (uint8_t i = 0; i < list_length; ++i)
    {
        if (list[i] != nullptr)
        {
            delete[] list[i];
            list[i] = nullptr;
        }
    }

    DIR* dir = opendir(resolve(path).c_str());
    if (!dir)
        return kListError;

    int found = 0;
    const std::string base = (path && path[0]) ? path : "/";

    while (dirent* ent = readdir(dir))
    {
        if (ent->d_name[0] == '.')
            continue;

        std::string child = base;
        if (child.back() != '/')
            child.push_back('/');
        child += ent->d_name;

        struct stat st{};
        if (::stat(resolve(child.c_str()).c_str(), &st) != 0 || S_ISDIR(st.st_mode))
            continue;

        const char* fname = ent->d_name;
        const int nameLen = static_cast<int>(std::strlen(fname));
        if (nameLen < 5)
            continue;
        if (!endsWithIgnoreCase(fname, ".pti"))
            continue;

        const char* p = fname;
        int idx1 = -1;
        int idx2 = -1;
        if (*p >= '0' && *p <= '9')
            idx1 = *p - '0';
        else
            continue;
        ++p;
        if (*p >= '0' && *p <= '9')
        {
            idx2 = *p - '0';
            ++p;
        }
        if (*p != ' ')
            continue;
        ++p;

        if (idx2 >= 0)
            idx1 = idx1 * 10 + idx2;
        if (idx1 < 1 || idx1 > static_cast<int>(list_length))
            continue;

        const int slot = idx1 - 1;
        if (list[slot] != nullptr)
            continue;

        // Strip trailing ".pti"
        int len = nameLen - static_cast<int>(p - fname) - 4;
        if (len < 0)
            len = 0;
        list[slot] = new char[static_cast<size_t>(len) + 1];
        if (len > 0)
            std::strncpy(list[slot], p, static_cast<size_t>(len));
        list[slot][len] = '\0';

        if (sizeOut)
            *sizeOut += static_cast<uint32_t>(st.st_size);
        ++found;
        if (found >= list_length)
            break;
    }

    closedir(dir);
    return found;
}

int FileSystemImpl::countContainedFiles(const char* path)
{
    DIR* dir = opendir(resolve(path).c_str());
    if (!dir)
        return 0;
    int count = 0;
    while (dirent* ent = readdir(dir))
    {
        if (ent->d_name[0] == '.')
            continue;
        ++count;
    }
    closedir(dir);
    return count;
}

int FileSystemImpl::countFilteredInDir(const char* path, enListFileFilter filter) const
{
    DIR* dir = opendir(resolve(path).c_str());
    if (!dir)
        return 0;
    int count = 0;
    while (dirent* ent = readdir(dir))
    {
        if (ent->d_name[0] == '.')
            continue;
        std::string child = path ? path : "/";
        if (child.empty() || child.back() != '/')
            child.push_back('/');
        child += ent->d_name;
        struct stat st{};
        const bool isDir =
          (::stat(resolve(child.c_str()).c_str(), &st) == 0 && S_ISDIR(st.st_mode));
        if (matchFilter(filter, ent->d_name, isDir))
            ++count;
    }
    closedir(dir);
    return count;
}

int FileSystemImpl::createFilesList(const char* path,
                                    uint8_t start_line,
                                    char** list,
                                    uint8_t list_length,
                                    uint16_t /*max_used_memory*/,
                                    enListFileFilter filter,
                                    uint16_t skip_count,
                                    uint16_t max_files_in_subdir,
                                    enListFileFilter folderSizeFilter)
{
    // Match device SdDir::createFilesList: allocate list[i] with new char[],
    // prefix folders with '/', start filling at start_line, return end index.
    if (!list || list_length == 0)
        return 0;

    for (uint8_t i = 0; i < list_length; ++i)
    {
        if (list[i] != nullptr)
        {
            delete[] list[i];
            list[i] = nullptr;
        }
    }

    DIR* dir = opendir(resolve(path).c_str());
    if (!dir)
        return kListError;

    uint8_t n = start_line;
    const std::string base = (path && path[0]) ? path : "/";

    while (n < list_length)
    {
        dirent* ent = readdir(dir);
        if (!ent)
            break;
        if (ent->d_name[0] == '.')
            continue;

        std::string child = base;
        if (child.back() != '/')
            child.push_back('/');
        child += ent->d_name;

        struct stat st{};
        const bool isDir =
          (::stat(resolve(child.c_str()).c_str(), &st) == 0 && S_ISDIR(st.st_mode));
        if (!matchFilter(filter, ent->d_name, isDir))
            continue;

        if (skip_count > 0)
        {
            --skip_count;
            continue;
        }

        if (isDir)
        {
            const size_t nameLen = std::strlen(ent->d_name);
            int filesCount = 0;
            if (max_files_in_subdir != 0)
                filesCount = countFilteredInDir(child.c_str(), folderSizeFilter);

            if (max_files_in_subdir != 0 && filesCount > static_cast<int>(max_files_in_subdir))
            {
                uint16_t offset = 0;
                while (filesCount > 0 && n < list_length)
                {
                    // "/name (start-end)" — generous buffer like device
                    const size_t len = nameLen + 4 + 2 * 5 + 8;
                    list[n] = new char[len];
                    std::snprintf(list[n],
                                  len,
                                  "/%s (%u-%u)",
                                  ent->d_name,
                                  static_cast<unsigned>(offset + 1),
                                  static_cast<unsigned>(offset + max_files_in_subdir));
                    filesCount -= static_cast<int>(max_files_in_subdir);
                    offset = static_cast<uint16_t>(offset + max_files_in_subdir);
                    ++n;
                }
            }
            else
            {
                list[n] = new char[nameLen + 2];
                list[n][0] = '/';
                std::strcpy(list[n] + 1, ent->d_name);
                ++n;
            }
        }
        else
        {
            const size_t nameLen = std::strlen(ent->d_name);
            list[n] = new char[nameLen + 1];
            std::strcpy(list[n], ent->d_name);
            ++n;
        }
    }

    closedir(dir);
    return n;
}

void FileSystemImpl::listDirContents(const char* dirpath,
                                     DirContentType contentType,
                                     std::function<bool(const char* name)> nameFilter,
                                     char* namesBuffer,
                                     int namesBufferSize,
                                     int maxNameLen,
                                     int& outNamesCount,
                                     char** fileList)
{
    outNamesCount = 0;
    if (!namesBuffer || namesBufferSize <= 0 || maxNameLen <= 0)
        return;

    DIR* dir = opendir(resolve(dirpath).c_str());
    if (!dir)
        return;

    int offset = 0;
    while (dirent* ent = readdir(dir))
    {
        if (ent->d_name[0] == '.')
            continue;
        std::string child = std::string(dirpath) + "/" + ent->d_name;
        struct stat st{};
        const bool isDir =
          (::stat(resolve(child.c_str()).c_str(), &st) == 0 && S_ISDIR(st.st_mode));
        if (contentType == kFolders && !isDir)
            continue;
        if (contentType == kFiles && isDir)
            continue;
        if (nameFilter && !nameFilter(ent->d_name))
            continue;
        if (offset + maxNameLen > namesBufferSize)
            break;
        char* slot = namesBuffer + offset;
        std::strncpy(slot, ent->d_name, static_cast<size_t>(maxNameLen) - 1);
        slot[maxNameLen - 1] = '\0';
        if (fileList)
            fileList[outNamesCount] = slot;
        offset += maxNameLen;
        ++outNamesCount;
    }
    closedir(dir);
}

int FileSystemImpl::readNbFiles(const char* romPath)
{
    return countContainedFiles(romPath);
}

bool FileSystemImpl::nesDir_open(const char* /*path*/)
{
    return false;
}

bool FileSystemImpl::nesDir_readItem(char* /*path*/,
                                     uint8_t* /*isDir*/,
                                     uint8_t* /*isHid*/,
                                     uint32_t* /*size*/)
{
    return false;
}

void FileSystemImpl::nesDir_close() {}
