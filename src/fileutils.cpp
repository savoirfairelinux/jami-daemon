/*
 *  Copyright (C) 2011-2016 Savoir-faire Linux Inc.
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

#include "fileutils.h"
#include "logger.h"
#include "compiler_intrinsics.h"

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#ifdef __ANDROID__
#include "client/ring_signal.h"
#endif
#ifdef _WIN32
#include "string_utils.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>

#ifndef WIN32_NATIVE
#include <libgen.h>
#endif

#ifdef WIN32_NATIVE
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
#if defined __ANDROID__ || defined WIN32 || TARGET_OS_IPHONE
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

bool isDirectory(const std::string& path)
{
    struct stat s;
    if (stat(path.c_str(), &s) == 0)
        return s.st_mode & S_IFDIR;
    return false;
}

bool isDirectoryWritable(const std::string &directory)
{
    return access(directory.c_str(), W_OK) == 0;
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
    HANDLE h = CreateFile(ring::to_wstring(path).c_str(), GENERIC_READ, FILE_SHARE_READ,  nullptr,  OPEN_EXISTING,  FILE_ATTRIBUTE_NORMAL, nullptr);
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

std::vector<uint8_t>
loadFile(const std::string& path)
{
    std::vector<uint8_t> buffer;
    std::ifstream file(path, std::ios::binary);
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
    if (!dp) {
        RING_ERR("Could not open %s", dir.c_str());
        return {};
    }

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

#ifdef __ANDROID__
static std::string files_path;
static std::string cache_path;
static std::string config_path;
#else
static char *program_dir = NULL;
#ifndef WIN32_NATIVE
void set_program_dir(char *program_path)
{
    program_dir = dirname(program_path);
}
#endif
#endif

std::string
get_cache_dir()
{
    const std::string cache_home(XDG_CACHE_HOME);

    if (not cache_home.empty()) {
        return cache_home;
    } else {
#ifdef __ANDROID__
        std::vector<std::string> paths;
        emitSignal<DRing::ConfigurationSignal::GetAppDataPath>("cache", &paths);
        if (not paths.empty())
            cache_path = paths[0];
        return cache_path;
#elif defined(__APPLE__)
        return get_home_dir() + DIR_SEPARATOR_STR
            + "Library" + DIR_SEPARATOR_STR + "Caches"
            + DIR_SEPARATOR_STR + PACKAGE;
#else
        return get_home_dir() + DIR_SEPARATOR_STR +
            ".cache" + DIR_SEPARATOR_STR + PACKAGE;
#endif
    }
}

std::string
get_home_dir()
{
#if defined __ANDROID__
    std::vector<std::string> paths;
    emitSignal<DRing::ConfigurationSignal::GetAppDataPath>("files", &paths);
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
get_data_dir()
{
#ifdef __ANDROID__
    std::vector<std::string> paths;
    emitSignal<DRing::ConfigurationSignal::GetAppDataPath>("files", &paths);
    if (not paths.empty())
        files_path = paths[0];
    return files_path;
#elif defined(__APPLE__)
    return get_home_dir() + DIR_SEPARATOR_STR
            + "Library" + DIR_SEPARATOR_STR + "Application Support"
            + DIR_SEPARATOR_STR + PACKAGE;
#else
    const std::string data_home(XDG_DATA_HOME);
    if (not data_home.empty())
        return data_home + DIR_SEPARATOR_STR + PACKAGE;
    // "If $XDG_DATA_HOME is either not set or empty, a default equal to
    // $HOME/.local/share should be used."
    return get_home_dir() + DIR_SEPARATOR_STR ".local" DIR_SEPARATOR_STR
        "share" DIR_SEPARATOR_STR + PACKAGE;
#endif
}

std::string
get_config_dir()
{
#ifdef __ANDROID__
    std::vector<std::string> paths;
    emitSignal<DRing::ConfigurationSignal::GetAppDataPath>("config", &paths);
    if (not paths.empty())
        config_path = paths[0];
    return config_path;
#else
#ifdef __APPLE__
    std::string configdir = fileutils::get_home_dir() + DIR_SEPARATOR_STR
        + "Library" + DIR_SEPARATOR_STR + "Application Support"
        + DIR_SEPARATOR_STR + PACKAGE;
#else
    std::string configdir = fileutils::get_home_dir() + DIR_SEPARATOR_STR +
                            ".config" + DIR_SEPARATOR_STR + PACKAGE;
#endif

    const std::string xdg_env(XDG_CONFIG_HOME);
    if (not xdg_env.empty())
        configdir = xdg_env + DIR_SEPARATOR_STR + PACKAGE;

    if (fileutils::recursive_mkdir(configdir.data(), 0700) != true) {
        // If directory creation failed
        if (errno != EEXIST)
            RING_DBG("Cannot create directory: %s!", configdir.c_str());
    }
    return configdir;
#endif
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

}} // namespace ring::fileutils
