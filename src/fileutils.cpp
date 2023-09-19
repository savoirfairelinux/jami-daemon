/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
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

#include <nettle/sha3.h>

#include <sstream>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <limits>
#include <array>

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

#define XDG_DATA_HOME   (PROTECTED_GETENV("XDG_DATA_HOME"))
#define XDG_CONFIG_HOME (PROTECTED_GETENV("XDG_CONFIG_HOME"))
#define XDG_CACHE_HOME  (PROTECTED_GETENV("XDG_CACHE_HOME"))
#else
const wchar_t*
winGetEnv(const wchar_t* name)
{
    const DWORD buffSize = 65535;
    static wchar_t buffer[buffSize];
    if (GetEnvironmentVariable(name, buffer, buffSize)) {
        return buffer;
    } else {
        return L"";
    }
}

#define PROTECTED_GETENV(str) winGetEnv(str)

#define JAMI_DATA_HOME   PROTECTED_GETENV(L"JAMI_DATA_HOME")
#define JAMI_CONFIG_HOME PROTECTED_GETENV(L"JAMI_CONFIG_HOME")
#define JAMI_CACHE_HOME  PROTECTED_GETENV(L"JAMI_CACHE_HOME")
#endif

#define PIDFILE     ".ring.pid"
#define ERASE_BLOCK 4096

namespace jami {
namespace fileutils {

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

bool
isDirectoryWritable(const std::string& directory)
{
    return accessFile(directory, W_OK) == 0;
}

bool
createSymlink(const std::filesystem::path& linkFile, const std::filesystem::path& target)
{
    try {
        std::filesystem::create_symlink(target, linkFile);
    } catch (const std::exception& e) {
        JAMI_ERR("Couldn't create soft link: %s", e.what());
        return false;
    }
    return true;
}

bool
createHardlink(const std::filesystem::path& linkFile, const std::filesystem::path& target)
{
    try {
        std::filesystem::create_hard_link(target, linkFile);
    } catch (const std::exception& e) {
        JAMI_ERR("Couldn't create hard link: %s", e.what());
        return false;
    }
    return true;
}

bool
createFileLink(const std::filesystem::path& linkFile, const std::filesystem::path& target, bool hard)
{
    if (not hard or not createHardlink(linkFile, target))
        return createSymlink(linkFile, target);
    return true;
}

std::string_view
getFileExtension(std::string_view filename)
{
    std::string_view result;
    auto sep = filename.find_last_of('.');
    if (sep != std::string_view::npos && sep != filename.size() - 1)
        result = filename.substr(sep + 1);
    if (result.size() >= 8)
        return {};
    return result;
}

bool
isPathRelative(const std::filesystem::path& path)
{
    return not path.empty() and path.is_relative();
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

std::filesystem::path
getFullPath(const std::filesystem::path& base, const std::filesystem::path& path)
{
    bool isRelative {not base.empty() and isPathRelative(path)};
    return isRelative ? base / path : path;
}

std::vector<uint8_t>
loadFile(const std::filesystem::path& path, const std::filesystem::path& default_dir)
{
    return dhtnet::fileutils::loadFile(getFullPath(default_dir, path));
}

std::string
loadTextFile(const std::filesystem::path& path, const std::filesystem::path& default_dir)
{
    std::string buffer;
    std::ifstream file(getFullPath(default_dir, path));
    if (!file)
        throw std::runtime_error("Can't read file: " + path.string());
    file.seekg(0, std::ios::end);
    auto size = file.tellg();
    if (size > std::numeric_limits<unsigned>::max())
        throw std::runtime_error("File is too big: " + path.string());
    buffer.resize(size);
    file.seekg(0, std::ios::beg);
    if (!file.read((char*) buffer.data(), size))
        throw std::runtime_error("Can't load file: " + path.string());
    return buffer;
}

bool
copy(const std::string& src, const std::string& dest)
{
    return std::filesystem::copy_file(src, dest);
}

void
saveFile(const std::filesystem::path& path, const uint8_t* data, size_t data_size, mode_t UNUSED mode)
{
    std::ofstream file(path, std::ios::trunc | std::ios::binary);
    if (!file.is_open()) {
        JAMI_ERROR("Could not write data to {}", path);
        return;
    }
    file.write((char*) data, data_size);
#ifndef _WIN32
    file.close();
    if (chmod(path.c_str(), mode) < 0)
        JAMI_WARNING("fileutils::saveFile(): chmod() failed on {}, {}",
                  path, strerror(errno));
#endif
}

std::vector<uint8_t>
loadCacheFile(const std::filesystem::path& path, std::chrono::system_clock::duration maxAge)
{
    // last_write_time throws exception if file doesn't exist
    auto writeTime = std::filesystem::last_write_time(path);
    if (decltype(writeTime)::clock::now() - writeTime > maxAge)
        throw std::runtime_error("file too old");

    JAMI_LOG("Loading cache file '{}'", path);
    return dhtnet::fileutils::loadFile(path);
}

std::string
loadCacheTextFile(const std::filesystem::path& path, std::chrono::system_clock::duration maxAge)
{
    // last_write_time throws exception if file doesn't exist
    auto writeTime = std::filesystem::last_write_time(path);
    if (decltype(writeTime)::clock::now() - writeTime > maxAge)
        throw std::runtime_error("file too old");

    JAMI_LOG("Loading cache file '{}'", path);
    return loadTextFile(path);
}

std::vector<uint8_t>
readArchive(const std::string& path, const std::string& pwd)
{
    JAMI_DBG("Reading archive from %s", path.c_str());

    auto isUnencryptedGzip = [](const std::vector<uint8_t>& data) {
        // NOTE: some webserver modify gzip files and this can end with a gunzip in a gunzip
        // file. So, to make the readArchive more robust, we can support this case by detecting
        // gzip header via 1f8b 08
        // We don't need to support more than 2 level, else somebody may be able to send
        // gunzip in loops and abuse.
        return data.size() > 3 && data[0] == 0x1f && data[1] == 0x8b && data[2] == 0x08;
    };

    auto decompress = [](std::vector<uint8_t>& data) {
        try {
            data = archiver::decompress(data);
        } catch (const std::exception& e) {
            JAMI_ERR("Error decrypting archive: %s", e.what());
            throw e;
        }
    };

    std::vector<uint8_t> data;
    // Read file
    try {
        data = dhtnet::fileutils::loadFile(path);
    } catch (const std::exception& e) {
        JAMI_ERR("Error loading archive: %s", e.what());
        throw e;
    }

    if (isUnencryptedGzip(data)) {
        if (!pwd.empty())
            JAMI_WARN() << "A gunzip in a gunzip is detected. A webserver may have a bad config";

        decompress(data);
    }

    if (!pwd.empty()) {
        // Decrypt
        try {
            data = dht::crypto::aesDecrypt(data, pwd);
        } catch (const std::exception& e) {
            JAMI_ERR("Error decrypting archive: %s", e.what());
            throw e;
        }
        decompress(data);
    } else if (isUnencryptedGzip(data)) {
        JAMI_WARN() << "A gunzip in a gunzip is detected. A webserver may have a bad config";
        decompress(data);
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

std::filesystem::path
get_cache_dir(const char* pkg)
{
#ifdef RING_UWP
    std::string cache_path;
    std::vector<std::string> paths;
    paths.reserve(1);
    emitSignal<libjami::ConfigurationSignal::GetAppDataPath>("", &paths);
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
    emitSignal<libjami::ConfigurationSignal::GetAppDataPath>("cache", &paths);
    if (not paths.empty())
        return paths[0];
    return {};
#elif defined(__APPLE__)
    return get_home_dir() / "Library" / "Caches" / pkg;
#else
#ifdef _WIN32
    const std::wstring cache_home(JAMI_CACHE_HOME);
    if (not cache_home.empty())
        return jami::to_string(cache_home);
#else
    const std::string cache_home(XDG_CACHE_HOME);
    if (not cache_home.empty())
        return cache_home;
#endif
    return get_home_dir() / ".cache" / pkg;
#endif
}

std::filesystem::path
get_cache_dir()
{
    return get_cache_dir(PACKAGE);
}

std::filesystem::path
get_home_dir()
{
#if defined(__ANDROID__) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
    std::vector<std::string> paths;
    paths.reserve(1);
    emitSignal<libjami::ConfigurationSignal::GetAppDataPath>("files", &paths);
    if (not paths.empty())
        return paths[0];
    return {};
#elif defined RING_UWP
    std::vector<std::string> paths;
    paths.reserve(1);
    emitSignal<libjami::ConfigurationSignal::GetAppDataPath>("", &paths);
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

    return {};
#endif
}

std::filesystem::path
get_data_dir(const char* pkg)
{
#if defined(__ANDROID__) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
    std::vector<std::string> paths;
    paths.reserve(1);
    emitSignal<libjami::ConfigurationSignal::GetAppDataPath>("files", &paths);
    if (not paths.empty())
        return paths[0];
    return {};
#elif defined(__APPLE__)
    return get_home_dir() / "Library" / "Application Support" / pkg;
#elif defined(_WIN32)
    std::wstring data_home(JAMI_DATA_HOME);
    if (not data_home.empty())
        return std::filesystem::path(data_home) / pkg;

    if (!strcmp(pkg, "ring")) {
        return get_home_dir() / ".local" / "share" / pkg;
    } else {
        return get_home_dir() / "AppData" / "Local" / pkg;
    }
#elif defined(RING_UWP)
    std::vector<std::string> paths;
    paths.reserve(1);
    emitSignal<libjami::ConfigurationSignal::GetAppDataPath>("", &paths);
    if (not paths.empty()) {
        auto files_path = std::filesystem::path(paths[0]) / ".data";
        if (fileutils::recursive_mkdir(files_path.data(), 0700) != true) {
            // If directory creation failed
            if (errno != EEXIST)
                JAMI_DBG("Cannot create directory: %s!", files_path.c_str());
        }
        return files_path;
    }
    return {};
#else
    std::string_view data_home(XDG_DATA_HOME);
    if (not data_home.empty())
        return std::filesystem::path(data_home) / pkg;
    // "If $XDG_DATA_HOME is either not set or empty, a default equal to
    // $HOME/.local/share should be used."
    return get_home_dir() / ".local" / "share" / pkg;
#endif
}

std::filesystem::path
get_data_dir()
{
    return get_data_dir(PACKAGE);
}

std::filesystem::path
get_config_dir(const char* pkg)
{
    std::filesystem::path configdir;
#if defined(__ANDROID__) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
    std::vector<std::string> paths;
    emitSignal<libjami::ConfigurationSignal::GetAppDataPath>("config", &paths);
    if (not paths.empty())
        configdir = std::filesystem::path(paths[0]);
#elif defined(RING_UWP)
    std::vector<std::string> paths;
    emitSignal<libjami::ConfigurationSignal::GetAppDataPath>("", &paths);
    if (not paths.empty())
        configdir = std::filesystem::path(paths[0]) / ".config";
#elif defined(__APPLE__)
    configdir = fileutils::get_home_dir() / "Library" / "Application Support" / pkg;
#elif defined(_WIN32)
    std::wstring xdg_env(JAMI_CONFIG_HOME);
    if (not xdg_env.empty()) {
        configdir = std::filesystem::path(xdg_env) / pkg;
    } else if (!strcmp(pkg, "ring")) {
        configdir = fileutils::get_home_dir() / ".config" / pkg;
    } else {
        configdir = fileutils::get_home_dir() / "AppData" / "Local" / pkg;
    }
#else
    std::string xdg_env(XDG_CONFIG_HOME);
    if (not xdg_env.empty())
        configdir = std::filesystem::path(xdg_env) / pkg;
    else
        configdir = fileutils::get_home_dir() / ".config" / pkg;
#endif
    if (dhtnet::fileutils::recursive_mkdir(configdir, 0700) != true) {
        // If directory creation failed
        if (errno != EEXIST)
            JAMI_DBG("Cannot create directory: %s!", configdir.c_str());
    }
    return configdir;
}

std::filesystem::path
get_config_dir()
{
    return get_config_dir(PACKAGE);
}

#ifdef _WIN32
bool
eraseFile_win32(const std::string& path, bool dosync)
{
    // Note: from https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-deletefilea#remarks
    // To delete a read-only file, first you must remove the read-only attribute.
    SetFileAttributesA(path.c_str(), GetFileAttributesA(path.c_str()) & ~FILE_ATTRIBUTE_READONLY);
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
    struct stat st;
    if (stat(path.c_str(), &st) == -1) {
        JAMI_WARN("Can not erase file %s: fstat() failed.", path.c_str());
        return false;
    }
    // Remove read-only flag if possible
    chmod(path.c_str(), st.st_mode | (S_IWGRP+S_IWUSR) );

    int fd = open(path.c_str(), O_WRONLY);
    if (fd == -1) {
        JAMI_WARN("Can not open file %s for erasing.", path.c_str());
        return false;
    }

    if (st.st_size == 0) {
        close(fd);
        return false;
    }

    lseek(fd, 0, SEEK_SET);

    std::array<char, ERASE_BLOCK> buffer;
    buffer.fill(0);
    decltype(st.st_size) written(0);
    while (written < st.st_size) {
        auto ret = write(fd, buffer.data(), buffer.size());
        if (ret < 0) {
            JAMI_WARNING("Error while overriding file with zeros.");
            break;
        } else
            written += ret;
    }

    if (dosync)
        fsync(fd);

    close(fd);
    return written >= st.st_size;
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
remove(const std::filesystem::path& path, bool erase)
{
    if (erase and dhtnet::fileutils::isFile(path, false) and !dhtnet::fileutils::hasHardLink(path))
        eraseFile(path.string(), true);

#ifdef _WIN32
    // use Win32 api since std::remove will not unlink directory in use
    if (std::filesystem::is_directory(path))
        return !RemoveDirectory(path.c_str());
#endif

    return std::remove(path.string().c_str());
}

int64_t
size(const std::string& path)
{
    int64_t size = 0;
    try {
        std::ifstream file(path, std::ios::binary | std::ios::in);
        file.seekg(0, std::ios_base::end);
        size = file.tellg();
        file.close();
    } catch (...) {
    }
    return size;
}

std::string
sha3File(const std::filesystem::path& path)
{
    sha3_512_ctx ctx;
    sha3_512_init(&ctx);

    try {
        if (not std::filesystem::is_regular_file(path))
            return {};
        std::ifstream file(path, std::ios::binary | std::ios::in);
        if (!file)
            return {};
        std::vector<char> buffer(8192, 0);
        while (!file.eof()) {
            file.read(buffer.data(), buffer.size());
            std::streamsize readSize = file.gcount();
            sha3_512_update(&ctx, readSize, (const uint8_t*) buffer.data());
        }
    } catch (...) {
        return {};
    }

    unsigned char digest[SHA3_512_DIGEST_SIZE];
    sha3_512_digest(&ctx, SHA3_512_DIGEST_SIZE, digest);

    char hash[SHA3_512_DIGEST_SIZE * 2];

    for (int i = 0; i < SHA3_512_DIGEST_SIZE; ++i)
        pj_val_to_hex_digit(digest[i], &hash[2 * i]);

    return {hash, SHA3_512_DIGEST_SIZE * 2};
}

std::string
sha3sum(const uint8_t* data, size_t size)
{
    sha3_512_ctx ctx;
    sha3_512_init(&ctx);
    sha3_512_update(&ctx, size, data);
    unsigned char digest[SHA3_512_DIGEST_SIZE];
    sha3_512_digest(&ctx, SHA3_512_DIGEST_SIZE, digest);
    return dht::toHex(digest, SHA3_512_DIGEST_SIZE);
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

uint64_t
lastWriteTimeInSeconds(const std::filesystem::path& filePath)
{
    return std::chrono::duration_cast<std::chrono::seconds>(
            std::filesystem::last_write_time(filePath)
                    .time_since_epoch()).count();
}

} // namespace fileutils
} // namespace jami
