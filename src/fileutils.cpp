/*
 *  Copyright (C) 2011-2019 Savoir-faire Linux Inc.
 *  Copyright (C) 2010 Michael Kerrisk
 *  Copyright (C) 2007-2009 Rémi Denis-Courmont
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
#include "string_utils.h"
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
#   include <wordexp.h>
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

namespace ring { namespace fileutils {

// returns true if directory exists
bool check_dir(const char *path,
            mode_t UNUSED dirmode,
            mode_t parentmode)
{
    DIR *dir = opendir(path);

    if (!dir) { // doesn't exist
        if (not recursive_mkdir(path, parentmode)) {
            perror(path);
            return false;
        }
#ifndef _WIN32
        if (chmod(path, dirmode) < 0) {
            RING_ERR("fileutils::check_dir(): chmod() failed on '%s', %s", path, strerror(errno));
            return false;
        }
#endif
    } else
        closedir(dir);
    return true;
}

#ifndef _WIN32
/* Lock a file region */
static int
lockReg(int fd, int cmd, int type, int whence, int start, off_t len)
{
    struct flock fl;

    fl.l_type = type;
    fl.l_whence = whence;
    fl.l_start = start;
    fl.l_len = len;

    return fcntl(fd, cmd, &fl);
}

static int /* Lock a file region using nonblocking F_SETLK */
lockRegion(int fd, int type, int whence, int start, int len)
{
    return lockReg(fd, F_SETLK, type, whence, start, len);
}

FileHandle
create_pidfile()
{
    const std::string name(get_home_dir() + DIR_SEPARATOR_STR PIDFILE);
    FileHandle f(name);
    char buf[100];
    f.fd = open(f.name.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (f.fd == -1) {
        RING_ERR("Could not open PID file %s", f.name.c_str());
        return f;
    }

    if (lockRegion(f.fd, F_WRLCK, SEEK_SET, 0, 0) == -1) {
        if (errno  == EAGAIN or errno == EACCES)
            RING_ERR("PID file '%s' is locked; probably "
                    "'%s' is already running", f.name.c_str(), PACKAGE_NAME);
        else
            RING_ERR("Unable to lock PID file '%s'", f.name.c_str());
        close(f.fd);
        f.fd = -1;
        return f;
    }

    if (ftruncate(f.fd, 0) == -1) {
        RING_ERR("Could not truncate PID file '%s'", f.name.c_str());
        close(f.fd);
        f.fd = -1;
        return f;
    }

    snprintf(buf, sizeof(buf), "%ld\n", (long) getpid());

    const int buf_strlen = strlen(buf);
    if (write(f.fd, buf, buf_strlen) != buf_strlen) {
        RING_ERR("Problem writing to PID file '%s'", f.name.c_str());
        close(f.fd);
        f.fd = -1;
        return f;
    }

    return f;
}
#endif // !_WIN32

std::string
expand_path(const std::string &path)
{
#if defined __ANDROID__ || defined _MSC_VER || defined WIN32 || defined __APPLE__
    RING_ERR("Path expansion not implemented, returning original");
    return path;
#else

    std::string result;

    wordexp_t p;
    int ret = wordexp(path.c_str(), &p, 0);

    switch (ret) {
        case WRDE_BADCHAR:
            RING_ERR("Illegal occurrence of newline or one of |, &, ;, <, >, "
                  "(, ), {, }.");
            return result;
        case WRDE_BADVAL:
            RING_ERR("An undefined shell variable was referenced");
            return result;
        case WRDE_CMDSUB:
            RING_ERR("Command substitution occurred");
            return result;
        case WRDE_SYNTAX:
            RING_ERR("Shell syntax error");
            return result;
        case WRDE_NOSPACE:
            RING_ERR("Out of memory.");
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

std::map<std::string, std::mutex> fileLocks {};
std::mutex fileLockLock {};

std::mutex&
getFileLock(const std::string& path)
{
    std::lock_guard<std::mutex> l(fileLockLock);
    return fileLocks[path];
}

bool isFile (const std::string& path) {
  struct stat s;
  return (stat (path.c_str(), &s) == 0) and not (s.st_mode & S_IFDIR);
}

bool isDirectory(const std::string& path)
{
    struct stat s;
    if (stat(path.c_str(), &s) == 0)
        return s.st_mode & S_IFDIR;
    return false;
}

bool isDirectoryWritable(const std::string &directory)
{
#ifdef _WIN32
    return access(decodeMultibyteString(directory).c_str(), W_OK) == 0;
#endif
    return access(directory.c_str(), W_OK) == 0;
}

bool isSymLink(const std::string& path)
{
#ifndef _WIN32
    struct stat s;
    if (lstat(path.c_str(), &s) == 0)
        return S_ISLNK(s.st_mode);
#elif !defined(_MSC_VER)
    DWORD attr = GetFileAttributes(ring::to_wstring(path).c_str());
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
    _CREATEFILE2_EXTENDED_PARAMETERS ext_params = { 0 };
    ext_params.dwSize = sizeof(CREATEFILE2_EXTENDED_PARAMETERS);
    ext_params.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
    ext_params.dwFileFlags = FILE_FLAG_NO_BUFFERING;
    ext_params.dwSecurityQosFlags = SECURITY_ANONYMOUS;
    ext_params.lpSecurityAttributes = nullptr;
    ext_params.hTemplateFile = nullptr;
    HANDLE h = CreateFile2(ring::to_wstring(path).c_str(), GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, &ext_params);
#elif _MSC_VER
    HANDLE h = CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
#else
    HANDLE h = CreateFile(ring::to_wstring(path).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
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
    struct tm tm {};
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

bool isPathRelative(const std::string& path)
{
#ifndef _WIN32
    return not path.empty() and not (path[0] == '/');
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
    std::ifstream file(getFullPath(default_dir, path), std::ios::binary);
    if (!file)
        throw std::runtime_error("Can't read file: "+path);
    file.seekg(0, std::ios::end);
    auto size = file.tellg();
    if (size > std::numeric_limits<unsigned>::max())
        throw std::runtime_error("File is too big: "+path);
    buffer.resize(size);
    file.seekg(0, std::ios::beg);
    if (!file.read((char*)buffer.data(), size))
        throw std::runtime_error("Can't load file: "+path);
    return buffer;
}

void
saveFile(const std::string& path,
        const std::vector<uint8_t>& data,
        mode_t UNUSED mode)
{
    std::ofstream file(path, std::ios::trunc | std::ios::binary);
    if (!file.is_open()) {
        RING_ERR("Could not write data to %s", path.c_str());
        return;
    }
    file.write((char*)data.data(), data.size());
#ifndef _WIN32
    if (chmod(path.c_str(), mode) < 0)
        RING_WARN("fileutils::saveFile(): chmod() failed on '%s', %s", path.c_str(), strerror(errno));
#endif
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
    DIR *dp = opendir(dir.c_str());
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
        const std::string fname {entry->d_name};
        if (fname == "." || fname == "..")
            continue;
        files.push_back(std::move(fname));
    }
    closedir(dp);
    return files;
}

std::vector<uint8_t>
readArchive(const std::string& path, const std::string& pwd)
{
    RING_DBG("Reading archive from %s", path.c_str());

    std::vector<uint8_t> data;
    if (pwd.empty()) {
        data = archiver::decompressGzip(path);
    } else {
        // Read file
        try {
            data = loadFile(path);
        } catch (const std::exception& e) {
            RING_ERR("Error loading archive: %s", e.what());
            throw;
        }
        // Decrypt
        try {
            data = archiver::decompress(dht::crypto::aesDecrypt(data, pwd));
        } catch (const std::exception& e) {
            RING_ERR("Error decrypting archive: %s", e.what());
            throw;
        }
    }
    return data;
}

void
writeArchive(const std::string& archive_str, const std::string& path, const std::string& password)
{
    RING_DBG("Writing archive to %s", path.c_str());

    if (not password.empty()) {
        // Encrypt using provided password
        std::vector<uint8_t> data = dht::crypto::aesEncrypt(archiver::compress(archive_str), password);
        // Write
        try {
            saveFile(path, data);
        } catch (const std::runtime_error& ex) {
            RING_ERR("Export failed: %s", ex.what());
            return;
        }
    } else {
        RING_WARN("Unsecured archiving (no password)");
        archiver::compressGzip(archive_str, path);
    }
}


FileHandle::FileHandle(const std::string &n) : fd(-1), name(n)
{}

FileHandle::~FileHandle()
{
    // we will only delete the file if it was created by this process
    if (fd != -1) {
        close(fd);
        if (unlink(name.c_str()) == -1)
            RING_ERR("%s", strerror(errno));
    }
}

#if defined(__ANDROID__) || defined(RING_UWP) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
static std::string files_path;
static std::string cache_path;
static std::string config_path;
#else
static char *program_dir = NULL;
void set_program_dir(char *program_path)
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
    std::vector<std::string> paths;
    emitSignal<DRing::ConfigurationSignal::GetAppDataPath>("", &paths);
    if (not paths.empty())
        cache_path = paths[0] + DIR_SEPARATOR_STR + std::string(".cache");

    if (fileutils::recursive_mkdir(cache_path.data(), 0700) != true) {
        // If directory creation failed
        if (errno != EEXIST)
            RING_DBG("Cannot create directory: %s!", cache_path.c_str());
    }
    return cache_path;
#else
    const std::string cache_home(XDG_CACHE_HOME);

    if (not cache_home.empty()) {
        return cache_home;
    } else {
#endif
#if defined(__ANDROID__) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
        std::vector<std::string> paths;
        emitSignal<DRing::ConfigurationSignal::GetAppDataPath>("cache", &paths);
        if (not paths.empty())
            cache_path = paths[0];
        return cache_path;
#elif defined(__APPLE__)
        return get_home_dir() + DIR_SEPARATOR_STR
            + "Library" + DIR_SEPARATOR_STR + "Caches"
            + DIR_SEPARATOR_STR + pkg;
#else
        return get_home_dir() + DIR_SEPARATOR_STR +
            ".cache" + DIR_SEPARATOR_STR + pkg;
#endif
#ifndef RING_UWP
    }
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
    emitSignal<DRing::ConfigurationSignal::GetAppDataPath>("files", &paths);
    if (not paths.empty())
        files_path = paths[0];
    return files_path;
#elif defined RING_UWP
    std::vector<std::string> paths;
    emitSignal<DRing::ConfigurationSignal::GetAppDataPath>("", &paths);
    if (not paths.empty())
        files_path = paths[0];
    return files_path;
#elif defined _WIN32
    WCHAR path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROFILE, nullptr, 0, path))) {
        char tmp[MAX_PATH];
        char DefChar = ' ';
        WideCharToMultiByte(CP_ACP, 0, path, -1, tmp, MAX_PATH, &DefChar, nullptr);
        return std::string(tmp);
    }
    return program_dir;
#else

    // 1) try getting user's home directory from the environment
    const std::string home(PROTECTED_GETENV("HOME"));
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
    emitSignal<DRing::ConfigurationSignal::GetAppDataPath>("files", &paths);
    if (not paths.empty())
        files_path = paths[0];
    return files_path;
#elif defined(__APPLE__)
    return get_home_dir() + DIR_SEPARATOR_STR
            + "Library" + DIR_SEPARATOR_STR + "Application Support"
            + DIR_SEPARATOR_STR + pkg;
#elif defined (RING_UWP)
    std::vector<std::string> paths;
    emitSignal<DRing::ConfigurationSignal::GetAppDataPath>("", &paths);
    if (not paths.empty())
        files_path = paths[0] + DIR_SEPARATOR_STR + std::string(".data");

    if (fileutils::recursive_mkdir(files_path.data(), 0700) != true) {
        // If directory creation failed
        if (errno != EEXIST)
            RING_DBG("Cannot create directory: %s!", files_path.c_str());
    }
    return files_path;
#else
    const std::string data_home(XDG_DATA_HOME);
    if (not data_home.empty())
        return data_home + DIR_SEPARATOR_STR + pkg;
    // "If $XDG_DATA_HOME is either not set or empty, a default equal to
    // $HOME/.local/share should be used."
    return get_home_dir() + DIR_SEPARATOR_STR ".local" DIR_SEPARATOR_STR
        "share" DIR_SEPARATOR_STR + pkg;
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
#if defined(__ANDROID__) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
    std::vector<std::string> paths;
    emitSignal<DRing::ConfigurationSignal::GetAppDataPath>("config", &paths);
    if (not paths.empty())
        config_path = paths[0];
    return config_path;

#elif defined(RING_UWP)
    std::vector<std::string> paths;
    emitSignal<DRing::ConfigurationSignal::GetAppDataPath>("", &paths);
    if (not paths.empty())
        config_path = paths[0] + DIR_SEPARATOR_STR + std::string(".config");

    if (fileutils::recursive_mkdir(config_path.data(), 0700) != true) {
        // If directory creation failed
        if (errno != EEXIST)
            RING_DBG("Cannot create directory: %s!", config_path.c_str());
    }
    return config_path;
#else
#if defined(__APPLE__)
    std::string configdir = fileutils::get_home_dir() + DIR_SEPARATOR_STR
        + "Library" + DIR_SEPARATOR_STR + "Application Support"
        + DIR_SEPARATOR_STR + pkg;
#else
    std::string configdir = fileutils::get_home_dir() + DIR_SEPARATOR_STR +
                            ".config" + DIR_SEPARATOR_STR + pkg;
#endif
    const std::string xdg_env(XDG_CONFIG_HOME);
    if (not xdg_env.empty())
        configdir = xdg_env + DIR_SEPARATOR_STR + pkg;

    if (fileutils::recursive_mkdir(configdir.data(), 0700) != true) {
        // If directory creation failed
        if (errno != EEXIST)
            RING_DBG("Cannot create directory: %s!", configdir.c_str());
    }
    return configdir;
#endif
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
    if (mkdir(path.data()) != 0) {
#endif
        if (errno == ENOENT) {
            recursive_mkdir(path.substr(0, path.find_last_of(DIR_SEPARATOR_STR)), mode);
#ifndef _WIN32
            if (mkdir(path.data(), mode) != 0) {
#else
            if (mkdir(path.data()) != 0) {
#endif
                RING_ERR("Could not create directory.");
                return false;
            }
        }
    }
    return true;
}

int
removeAll(const std::string& path)
{
    if (path.empty())
        return -1;
    if (isDirectory(path) and !isSymLink(path)) {
        auto dir = path;
        if (dir.back() != DIR_SEPARATOR_CH)
            dir += DIR_SEPARATOR_CH;
        for (auto& entry : fileutils::readDirectory(dir))
            removeAll(dir + entry);
    }
    return remove(path);
}

}} // namespace ring::fileutils
