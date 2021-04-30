/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
 *
 *  Author: Rafaël Carré <rafael.carre@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "logger.h"
#include "fileutils.h"
#include "archiver.h"
#include "compiler_intrinsics.h"
#include <opendht/crypto.h>

#ifdef RING_UWP
#include <io.h> // for access and close
#include "ring_signal.h"
#endif

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#if defined(__ANDROID__) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
#include "client/ring_signal.h"
#endif

#ifdef _WIN32
#include <windows.h>
#include "string_utils.h"
#include "ring_signal.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>

#ifndef _MSC_VER
#include <libgen.h>
#endif

#ifdef _MSC_VER
#include "windirent.h"
#else
#include <dirent.h>
#endif

#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#ifndef _WIN32
#include <pwd.h>
#else
#include <shlobj.h>
#define NAME_MAX 255
#endif
#if !defined __ANDROID__ && !defined _WIN32
#include <wordexp.h>
#endif

#include <sstream>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <limits>

#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cstddef>
#include <ciso646>

#include <pj/ctype.h>
#include <pjlib-util/md5.h>

#ifndef _MSC_VER
#define PROTECTED_GETENV(str) \
    ({ \
        char* envvar_ = getenv((str)); \
        envvar_ ? envvar_ : ""; \
    })
#else
#define PROTECTED_GETENV(str) ""
#endif

#define XDG_DATA_HOME   (PROTECTED_GETENV("XDG_DATA_HOME"))
#define XDG_CONFIG_HOME (PROTECTED_GETENV("XDG_CONFIG_HOME"))
#define XDG_CACHE_HOME  (PROTECTED_GETENV("XDG_CACHE_HOME"))

#define PIDFILE     ".ring.pid"
#define ERASE_BLOCK 4096

namespace jami {
namespace fileutils {

// returns true if directory exists
bool
check_dir(const char* path, mode_t UNUSED dirmode, mode_t parentmode)
{
    DIR* dir = opendir(path);

    if (!dir) { // doesn't exist
        if (not recursive_mkdir(path, parentmode)) {
            perror(path);
            return false;
        }
#ifndef _WIN32
        if (chmod(path, dirmode) < 0) {
            JAMI_ERR("fileutils::check_dir(): chmod() failed on '%s', %s", path, strerror(errno));
            return false;
        }
#endif
    } else
        closedir(dir);
    return true;
}

std::string
expand_path(const std::string& path)
{
#if defined __ANDROID__ || defined _MSC_VER || defined WIN32 || defined __APPLE__
    JAMI_ERR("Path expansion not implemented, returning original");
    return path;
#else

    std::string result;

    wordexp_t p;
    int ret = wordexp(path.c_str(), &p, 0);

    switch (ret) {
    case WRDE_BADCHAR:
        JAMI_ERR("Illegal occurrence of newline or one of |, &, ;, <, >, "
                 "(, ), {, }.");
        return result;
    case WRDE_BADVAL:
        JAMI_ERR("An undefined shell variable was referenced");
        return result;
    case WRDE_CMDSUB:
        JAMI_ERR("Command substitution occurred");
        return result;
    case WRDE_SYNTAX:
        JAMI_ERR("Shell syntax error");
        return result;
    case WRDE_NOSPACE:
        JAMI_ERR("Out of memory.");
        // This is the only error where we must call wordfree
        break;
    default:
        if (p.we_wordc > 0)
            result = std::string(p.we_wordv[0]);
        break;
    }

    wordfree(&p);

    return result;
#endif
}

std::mutex&
getFileLock(const std::string& path)
{
    static std::mutex fileLockLock {};
    static std::map<std::string, std::mutex> fileLocks {};

    std::lock_guard<std::mutex> l(fileLockLock);
    return fileLocks[path];
}

bool
isFile(const std::string& path, bool resolveSymlink)
{
    if (path.empty())
        return false;
#ifdef _WIN32
    if (resolveSymlink) {
        struct _stat64i32 s;
        if (_wstat(jami::to_wstring(path).c_str(), &s) == 0)
            return S_ISREG(s.st_mode);
    } else {
        DWORD attr = GetFileAttributes(jami::to_wstring(path).c_str());
        if ((attr != INVALID_FILE_ATTRIBUTES) && !(attr & FILE_ATTRIBUTE_DIRECTORY)
            && !(attr & FILE_ATTRIBUTE_REPARSE_POINT))
            return true;
    }
#else
    if (resolveSymlink) {
        struct stat s;
        if (stat(path.c_str(), &s) == 0)
            return S_ISREG(s.st_mode);
    } else {
        struct stat s;
        if (lstat(path.c_str(), &s) == 0)
            return S_ISREG(s.st_mode);
    }
#endif

    return false;
}

bool
isDirectory(const std::string& path)
{
    struct stat s;
    if (stat(path.c_str(), &s) == 0)
        return s.st_mode & S_IFDIR;
    return false;
}

bool
isDirectoryWritable(const std::string& directory)
{
    return accessFile(directory, W_OK) == 0;
}

bool
isSymLink(const std::string& path)
{
#ifndef _WIN32
    struct stat s;
    if (lstat(path.c_str(), &s) == 0)
        return S_ISLNK(s.st_mode);
#elif !defined(_MSC_VER)
    DWORD attr = GetFileAttributes(jami::to_wstring(path).c_str());
    if (attr & FILE_ATTRIBUTE_REPARSE_POINT)
        return true;
#endif
    return false;
}

std::chrono::system_clock::time_point
writeTime(const std::string& path)
{
#ifndef _WIN32
    struct stat s;
    auto ret = stat(path.c_str(), &s);
    if (ret)
        throw std::runtime_error("Can't check write time for: " + path);
    return std::chrono::system_clock::from_time_t(s.st_mtime);
#else
#if RING_UWP
    _CREATEFILE2_EXTENDED_PARAMETERS ext_params = {0};
    ext_params.dwSize = sizeof(CREATEFILE2_EXTENDED_PARAMETERS);
    ext_params.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
    ext_params.dwFileFlags = FILE_FLAG_NO_BUFFERING;
    ext_params.dwSecurityQosFlags = SECURITY_ANONYMOUS;
    ext_params.lpSecurityAttributes = nullptr;
    ext_params.hTemplateFile = nullptr;
    HANDLE h = CreateFile2(jami::to_wstring(path).c_str(),
                           GENERIC_READ,
                           FILE_SHARE_READ,
                           OPEN_EXISTING,
                           &ext_params);
#elif _WIN32
    HANDLE h = CreateFileW(jami::to_wstring(path).c_str(),
                           GENERIC_READ,
                           FILE_SHARE_READ,
                           nullptr,
                           OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL,
                           nullptr);
#endif
    if (h == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Can't open: " + path);
    FILETIME lastWriteTime;
    if (!GetFileTime(h, nullptr, nullptr, &lastWriteTime))
        throw std::runtime_error("Can't check write time for: " + path);
    CloseHandle(h);
    SYSTEMTIME sTime;
    if (!FileTimeToSystemTime(&lastWriteTime, &sTime))
        throw std::runtime_error("Can't check write time for: " + path);
    struct tm tm
    {};
    tm.tm_year = sTime.wYear - 1900;
    tm.tm_mon = sTime.wMonth - 1;
    tm.tm_mday = sTime.wDay;
    tm.tm_hour = sTime.wHour;
    tm.tm_min = sTime.wMinute;
    tm.tm_sec = sTime.wSecond;
    tm.tm_isdst = -1;
    return std::chrono::system_clock::from_time_t(mktime(&tm));
#endif
}

bool
isPathRelative(const std::string& path)
{
#ifndef _WIN32
    return not path.empty() and not(path[0] == '/');
#else
    return not path.empty() and path.find(":") == std::string::npos;
#endif
}

std::string
getCleanPath(const std::string& base, const std::string& path)
{
    if (base.empty() or path.size() < base.size())
        return path;
    auto base_sep = base + DIR_SEPARATOR_STR;
    if (path.compare(0, base_sep.size(), base_sep) == 0)
        return path.substr(base_sep.size());
    else
        return path;
}

std::string
getFullPath(const std::string& base, const std::string& path)
{
    bool isRelative {not base.empty() and isPathRelative(path)};
    return isRelative ? base + DIR_SEPARATOR_STR + path : path;
}

std::vector<uint8_t>
loadFile(const std::string& path, const std::string& default_dir)
{
    std::vector<uint8_t> buffer;
    std::ifstream file = ifstream(getFullPath(default_dir, path), std::ios::binary);
    if (!file)
        throw std::runtime_error("Can't read file: " + path);
    file.seekg(0, std::ios::end);
    auto size = file.tellg();
    if (size > std::numeric_limits<unsigned>::max())
        throw std::runtime_error("File is too big: " + path);
    buffer.resize(size);
    file.seekg(0, std::ios::beg);
    if (!file.read((char*) buffer.data(), size))
        throw std::runtime_error("Can't load file: " + path);
    return buffer;
}

std::string
loadTextFile(const std::string& path, const std::string& default_dir)
{
    std::string buffer;
    std::ifstream file = ifstream(getFullPath(default_dir, path));
    if (!file)
        throw std::runtime_error("Can't read file: " + path);
    file.seekg(0, std::ios::end);
    auto size = file.tellg();
    if (size > std::numeric_limits<unsigned>::max())
        throw std::runtime_error("File is too big: " + path);
    buffer.resize(size);
    file.seekg(0, std::ios::beg);
    if (!file.read((char*) buffer.data(), size))
        throw std::runtime_error("Can't load file: " + path);
    return buffer;
}

void
saveFile(const std::string& path, const uint8_t* data, size_t data_size, mode_t UNUSED mode)
{
    std::ofstream file = fileutils::ofstream(path, std::ios::trunc | std::ios::binary);
    if (!file.is_open()) {
        JAMI_ERR("Could not write data to %s", path.c_str());
        return;
    }
    file.write((char*) data, data_size);
#ifndef _WIN32
    if (chmod(path.c_str(), mode) < 0)
        JAMI_WARN("fileutils::saveFile(): chmod() failed on '%s', %s",
                  path.c_str(),
                  strerror(errno));
#endif
}

std::vector<uint8_t>
loadCacheFile(const std::string& path, std::chrono::system_clock::duration maxAge)
{
    // writeTime throws exception if file doesn't exist
    auto duration = std::chrono::system_clock::now() - writeTime(path);
    if (duration > maxAge)
        throw std::runtime_error("file too old");

    JAMI_DBG("Loading cache file '%.*s'", (int) path.size(), path.c_str());
    return loadFile(path);
}

std::string
loadCacheTextFile(const std::string& path, std::chrono::system_clock::duration maxAge)
{
    // writeTime throws exception if file doesn't exist
    auto duration = std::chrono::system_clock::now() - writeTime(path);
    if (duration > maxAge)
        throw std::runtime_error("file too old");

    JAMI_DBG("Loading cache file '%.*s'", (int) path.size(), path.c_str());
    return loadTextFile(path);
}

static size_t
dirent_buf_size(UNUSED DIR* dirp)
{
    long name_max;
#if defined(HAVE_FPATHCONF) && defined(HAVE_DIRFD) && defined(_PC_NAME_MAX)
    name_max = fpathconf(dirfd(dirp), _PC_NAME_MAX);
    if (name_max == -1)
#if defined(NAME_MAX)
        name_max = (NAME_MAX > 255) ? NAME_MAX : 255;
#else
        return (size_t)(-1);
#endif
#else
#if defined(NAME_MAX)
    name_max = (NAME_MAX > 255) ? NAME_MAX : 255;
#else
#error "buffer size for readdir_r cannot be determined"
#endif
#endif
    size_t name_end = (size_t) offsetof(struct dirent, d_name) + name_max + 1;
    return name_end > sizeof(struct dirent) ? name_end : sizeof(struct dirent);
}

std::vector<std::string>
readDirectory(const std::string& dir)
{
    DIR* dp = opendir(dir.c_str());
    if (!dp)
        return {};

    size_t size = dirent_buf_size(dp);
    if (size == (size_t)(-1))
        return {};
    std::vector<uint8_t> buf(size);
    dirent* entry;

    std::vector<std::string> files;
#ifndef _WIN32
    while (!readdir_r(dp, reinterpret_cast<dirent*>(buf.data()), &entry) && entry) {
#else
    while ((entry = readdir(dp)) != nullptr) {
#endif
        std::string fname {entry->d_name};
        if (fname == "." || fname == "..")
            continue;
        files.emplace_back(std::move(fname));
    }
    closedir(dp);
    return files;
} // namespace fileutils

std::vector<uint8_t>
readArchive(const std::string& path, const std::string& pwd)
{
    JAMI_DBG("Reading archive from %s", path.c_str());

    std::vector<uint8_t> data;
    if (pwd.empty()) {
        data = archiver::decompressGzip(path);
    } else {
        // Read file
        try {
            data = loadFile(path);
        } catch (const std::exception& e) {
            JAMI_ERR("Error loading archive: %s", e.what());
            throw e;
        }
        // Decrypt
        try {
            data = archiver::decompress(dht::crypto::aesDecrypt(data, pwd));
        } catch (const std::exception& e) {
            JAMI_ERR("Error decrypting archive: %s", e.what());
            throw e;
        }
    }
    return data;
}

void
writeArchive(const std::string& archive_str, const std::string& path, const std::string& password)
{
    JAMI_DBG("Writing archive to %s", path.c_str());

    if (not password.empty()) {
        // Encrypt using provided password
        std::vector<uint8_t> data = dht::crypto::aesEncrypt(archiver::compress(archive_str),
                                                            password);
        // Write
        try {
            saveFile(path, data);
        } catch (const std::runtime_error& ex) {
            JAMI_ERR("Export failed: %s", ex.what());
            return;
        }
    } else {
        JAMI_WARN("Unsecured archiving (no password)");
        archiver::compressGzip(archive_str, path);
    }
}

#if defined(__ANDROID__) || defined(RING_UWP) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
#else
static char* program_dir = NULL;
void
set_program_dir(char* program_path)
{
#ifdef _MSC_VER
    _splitpath(program_path, nullptr, program_dir, nullptr, nullptr);
#else
    program_dir = dirname(program_path);
#endif
}
#endif

std::string
get_cache_dir(const char* pkg)
{
#ifdef RING_UWP
    std::string cache_path;
    std::vector<std::string> paths;
    paths.reserve(1);
    emitSignal<DRing::ConfigurationSignal::GetAppDataPath>("", &paths);
    if (not paths.empty()) {
        cache_path = paths[0] + DIR_SEPARATOR_STR + std::string(".cache");
        if (fileutils::recursive_mkdir(cache_path.data(), 0700) != true) {
            // If directory creation failed
            if (errno != EEXIST)
                JAMI_DBG("Cannot create directory: %s!", cache_path.c_str());
        }
    }
    return cache_path;
#elif defined(__ANDROID__) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
    std::vector<std::string> paths;
    paths.reserve(1);
    emitSignal<DRing::ConfigurationSignal::GetAppDataPath>("cache", &paths);
    if (not paths.empty())
        return paths[0];
    return {};
#elif defined(__APPLE__)
    return get_home_dir() + DIR_SEPARATOR_STR + "Library" + DIR_SEPARATOR_STR + "Caches"
           + DIR_SEPARATOR_STR + pkg;
#else
    const std::string cache_home(XDG_CACHE_HOME);
    if (not cache_home.empty())
        return cache_home;
    else
        return get_home_dir() + DIR_SEPARATOR_STR + ".cache" + DIR_SEPARATOR_STR + pkg;
#endif
}

std::string
get_cache_dir()
{
    return get_cache_dir(PACKAGE);
}

std::string
get_home_dir()
{
#if defined(__ANDROID__) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
    std::vector<std::string> paths;
    paths.reserve(1);
    emitSignal<DRing::ConfigurationSignal::GetAppDataPath>("files", &paths);
    if (not paths.empty())
        return paths[0];
    return {};
#elif defined RING_UWP
    std::vector<std::string> paths;
    paths.reserve(1);
    emitSignal<DRing::ConfigurationSignal::GetAppDataPath>("", &paths);
    if (not paths.empty())
        return paths[0];
    return {};
#elif defined _WIN32
    TCHAR path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPath(nullptr, CSIDL_PROFILE, nullptr, 0, path))) {
        return jami::to_string(path);
    }
    return program_dir;
#else

    // 1) try getting user's home directory from the environment
    std::string home(PROTECTED_GETENV("HOME"));
    if (not home.empty())
        return home;

    // 2) try getting it from getpwuid_r (i.e. /etc/passwd)
    const long max = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (max != -1) {
        char buf[max];
        struct passwd pwbuf, *pw;
        if (getpwuid_r(getuid(), &pwbuf, buf, sizeof(buf), &pw) == 0 and pw != NULL)
            return pw->pw_dir;
    }

    return "";
#endif
}

std::string
get_data_dir(const char* pkg)
{
#if defined(__ANDROID__) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
    std::vector<std::string> paths;
    paths.reserve(1);
    emitSignal<DRing::ConfigurationSignal::GetAppDataPath>("files", &paths);
    if (not paths.empty())
        return paths[0];
    return {};
#elif defined(__APPLE__)
    return get_home_dir() + DIR_SEPARATOR_STR + "Library" + DIR_SEPARATOR_STR
           + "Application Support" + DIR_SEPARATOR_STR + pkg;
#elif defined(RING_UWP)
    std::vector<std::string> paths;
    paths.reserve(1);
    emitSignal<DRing::ConfigurationSignal::GetAppDataPath>("", &paths);
    if (not paths.empty()) {
        auto files_path = paths[0] + DIR_SEPARATOR_STR + std::string(".data");
        if (fileutils::recursive_mkdir(files_path.data(), 0700) != true) {
            // If directory creation failed
            if (errno != EEXIST)
                JAMI_DBG("Cannot create directory: %s!", files_path.c_str());
        }
        return files_path;
    }
    return {};
#else
    std::vector<std::string> paths;
    paths.reserve(1);
    emitSignal<DRing::ConfigurationSignal::GetAppDataPath>("", &paths);
    if (not paths.empty())
        return paths[0];
#ifdef _WIN32
    if (!strcmp(pkg, "ring")) {
        return get_home_dir() + DIR_SEPARATOR_STR + ".local" + DIR_SEPARATOR_STR
               + "share" DIR_SEPARATOR_STR + pkg;
    } else {
        return get_home_dir() + DIR_SEPARATOR_STR + "AppData" + DIR_SEPARATOR_STR + "Local"
               + DIR_SEPARATOR_STR + pkg;
    }
#endif
    const std::string data_home(XDG_DATA_HOME);
    if (not data_home.empty())
        return data_home + DIR_SEPARATOR_STR + pkg;
    // "If $XDG_DATA_HOME is either not set or empty, a default equal to
    // $HOME/.local/share should be used."
    return get_home_dir() + DIR_SEPARATOR_STR ".local" DIR_SEPARATOR_STR "share" DIR_SEPARATOR_STR
           + pkg;
#endif
}

std::string
get_data_dir()
{
    return get_data_dir(PACKAGE);
}

std::string
get_config_dir(const char* pkg)
{
    std::string configdir;
#if defined(__ANDROID__) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
    std::vector<std::string> paths;
    emitSignal<DRing::ConfigurationSignal::GetAppDataPath>("config", &paths);
    if (not paths.empty())
        configdir = std::move(paths[0]);
#elif defined(RING_UWP)
    std::vector<std::string> paths;
    emitSignal<DRing::ConfigurationSignal::GetAppDataPath>("", &paths);
    if (not paths.empty())
        configdir = paths[0] + DIR_SEPARATOR_STR + std::string(".config");
#elif defined(__APPLE__)
    configdir = fileutils::get_home_dir() + DIR_SEPARATOR_STR + "Library" + DIR_SEPARATOR_STR
                + "Application Support" + DIR_SEPARATOR_STR + pkg;
#else
    std::vector<std::string> paths;
    paths.reserve(1);
    emitSignal<DRing::ConfigurationSignal::GetAppDataPath>("", &paths);
    if (not paths.empty()) {
        configdir = paths[0];
    } else {
#ifdef _WIN32
        if (!strcmp(pkg, "ring")) {
            configdir = fileutils::get_home_dir() + DIR_SEPARATOR_STR + ".config"
                        + DIR_SEPARATOR_STR + pkg;
        } else {
            configdir = fileutils::get_home_dir() + DIR_SEPARATOR_STR + "AppData"
                        + DIR_SEPARATOR_STR + "Local" + DIR_SEPARATOR_STR + pkg;
        }
#endif
        const std::string xdg_env(XDG_CONFIG_HOME);
        if (not xdg_env.empty())
            configdir = xdg_env + DIR_SEPARATOR_STR + pkg;
        else
            configdir = fileutils::get_home_dir() + DIR_SEPARATOR_STR + ".config"
                        + DIR_SEPARATOR_STR + pkg;
    }
#endif
    if (fileutils::recursive_mkdir(configdir.data(), 0700) != true) {
        // If directory creation failed
        if (errno != EEXIST)
            JAMI_DBG("Cannot create directory: %s!", configdir.c_str());
    }
    return configdir;
}

std::string
get_config_dir()
{
    return get_config_dir(PACKAGE);
}

bool
recursive_mkdir(const std::string& path, mode_t mode)
{
#ifndef _WIN32
    if (mkdir(path.data(), mode) != 0) {
#else
    if (_wmkdir(jami::to_wstring(path.data()).c_str()) != 0) {
#endif
        if (errno == ENOENT) {
            recursive_mkdir(path.substr(0, path.find_last_of(DIR_SEPARATOR_CH)), mode);
#ifndef _WIN32
            if (mkdir(path.data(), mode) != 0) {
#else
            if (_wmkdir(jami::to_wstring(path.data()).c_str()) != 0) {
#endif
                JAMI_ERR("Could not create directory.");
                return false;
            }
        }
    } // namespace jami
    return true;
}

#ifdef _WIN32
bool
eraseFile_win32(const std::string& path, bool dosync)
{
    HANDLE h
        = CreateFileA(path.c_str(), GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (h == INVALID_HANDLE_VALUE) {
        JAMI_WARN("Can not open file %s for erasing.", path.c_str());
        return false;
    }

    LARGE_INTEGER size;
    if (!GetFileSizeEx(h, &size)) {
        JAMI_WARN("Can not erase file %s: GetFileSizeEx() failed.", path.c_str());
        CloseHandle(h);
        return false;
    }
    if (size.QuadPart == 0) {
        CloseHandle(h);
        return false;
    }

    uint64_t size_blocks = size.QuadPart / ERASE_BLOCK;
    if (size.QuadPart % ERASE_BLOCK)
        size_blocks++;

    char* buffer;
    try {
        buffer = new char[ERASE_BLOCK];
    } catch (std::bad_alloc& ba) {
        JAMI_WARN("Can not allocate buffer for erasing %s.", path.c_str());
        CloseHandle(h);
        return false;
    }
    memset(buffer, 0x00, ERASE_BLOCK);

    OVERLAPPED ovlp;
    if (size.QuadPart < (1024 - 42)) { // a small file can be stored in the MFT record
        ovlp.Offset = 0;
        ovlp.OffsetHigh = 0;
        WriteFile(h, buffer, (DWORD) size.QuadPart, 0, &ovlp);
        FlushFileBuffers(h);
    }
    for (uint64_t i = 0; i < size_blocks; i++) {
        uint64_t offset = i * ERASE_BLOCK;
        ovlp.Offset = offset & 0x00000000FFFFFFFF;
        ovlp.OffsetHigh = offset >> 32;
        WriteFile(h, buffer, ERASE_BLOCK, 0, &ovlp);
    }

    delete[] buffer;

    if (dosync)
        FlushFileBuffers(h);

    CloseHandle(h);
    return true;
}

#else

bool
eraseFile_posix(const std::string& path, bool dosync)
{
    int fd = open(path.c_str(), O_WRONLY);
    if (fd == -1) {
        JAMI_WARN("Can not open file %s for erasing.", path.c_str());
        return false;
    }

    struct stat st;
    if (fstat(fd, &st) == -1) {
        JAMI_WARN("Can not erase file %s: fstat() failed.", path.c_str());
        close(fd);
        return false;
    }

    if (st.st_size == 0) {
        close(fd);
        return false;
    }

    uintmax_t size_blocks = st.st_size / ERASE_BLOCK;
    if (st.st_size % ERASE_BLOCK)
        size_blocks++;

    char* buffer;
    try {
        buffer = new char[ERASE_BLOCK];
    } catch (std::bad_alloc& ba) {
        JAMI_WARN("Can not allocate buffer for erasing %s.", path.c_str());
        close(fd);
        return false;
    }
    memset(buffer, 0x00, ERASE_BLOCK);

    for (uintmax_t i = 0; i < size_blocks; i++) {
        lseek(fd, i * ERASE_BLOCK, SEEK_SET);
        write(fd, buffer, ERASE_BLOCK);
    }

    delete[] buffer;

    if (dosync)
        fsync(fd);

    close(fd);
    return true;
}
#endif

bool
eraseFile(const std::string& path, bool dosync)
{
#ifdef _WIN32
    return eraseFile_win32(path, dosync);
#else
    return eraseFile_posix(path, dosync);
#endif
}

int
remove(const std::string& path, bool erase)
{
    if (erase and isFile(path, false)) {
        eraseFile(path, true);
    }

    return std::remove(path.c_str());
}

int
removeAll(const std::string& path, bool erase)
{
    if (path.empty())
        return -1;
    if (isDirectory(path) and !isSymLink(path)) {
        auto dir = path;
        if (dir.back() != DIR_SEPARATOR_CH)
            dir += DIR_SEPARATOR_CH;
        for (auto& entry : fileutils::readDirectory(dir))
            removeAll(dir + entry, erase);
    }
    return remove(path, erase);
}

void
openStream(std::ifstream& file, const std::string& path, std::ios_base::openmode mode)
{
#ifdef _WIN32
    file.open(jami::to_wstring(path), mode);
#else
    file.open(path, mode);
#endif
}

void
openStream(std::ofstream& file, const std::string& path, std::ios_base::openmode mode)
{
#ifdef _WIN32
    file.open(jami::to_wstring(path), mode);
#else
    file.open(path, mode);
#endif
}

std::ifstream
ifstream(const std::string& path, std::ios_base::openmode mode)
{
#ifdef _WIN32
    return std::ifstream(jami::to_wstring(path), mode);
#else
    return std::ifstream(path, mode);
#endif
}

std::ofstream
ofstream(const std::string& path, std::ios_base::openmode mode)
{
#ifdef _WIN32
    return std::ofstream(jami::to_wstring(path), mode);
#else
    return std::ofstream(path, mode);
#endif
}

std::string
md5File(const std::string& path)
{
    return md5sum(loadFile(path));
}

std::string
md5sum(const std::vector<uint8_t>& buffer)
{
    pj_md5_context pms;

    pj_md5_init(&pms);
    pj_md5_update(&pms, (const uint8_t*) buffer.data(), buffer.size());

    unsigned char digest[16];
    pj_md5_final(&pms, digest);

    char hash[32];

    for (int i = 0; i < 16; ++i)
        pj_val_to_hex_digit(digest[i], &hash[2 * i]);

    return {hash, 32};
}

int
accessFile(const std::string& file, int mode)
{
#ifdef _WIN32
    return _waccess(jami::to_wstring(file).c_str(), mode);
#else
    return access(file.c_str(), mode);
#endif
}

} // namespace fileutils
} // namespace jami
