/*
 *  Copyright (C) 2004-2020 Savoir-faire Linux Inc.
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

#ifndef FILEUTILS_H_
#define FILEUTILS_H_

#include <string>
#include <vector>
#include <chrono>
#include <mutex>
#include <cstdio>
#include <ios>

#include "dring/def.h"

#ifndef _MSC_VER
#define PROTECTED_GETENV(str) ({char *envvar_ = getenv((str)); \
                                                   envvar_ ? envvar_ : "";})
#else
#define PROTECTED_GETENV(str) ""
#endif

#define XDG_DATA_HOME           (PROTECTED_GETENV("XDG_DATA_HOME"))
#define XDG_CONFIG_HOME         (PROTECTED_GETENV("XDG_CONFIG_HOME"))
#define XDG_CACHE_HOME          (PROTECTED_GETENV("XDG_CACHE_HOME"))

#define PIDFILE ".ring.pid"
#define ERASE_BLOCK 4096

#ifndef _WIN32
#include <sys/stat.h>           // mode_t
#define DIR_SEPARATOR_STR "/"   // Directory separator char
#define DIR_SEPARATOR_CH  '/'   // Directory separator string
#else
#define mode_t unsigned
#define DIR_SEPARATOR_STR "\\"  // Directory separator char
#define DIR_SEPARATOR_CH  '\\'  // Directory separator string
#endif

namespace jami { namespace fileutils {

std::string get_home_dir();
std::string get_config_dir(const char* pkg);
std::string get_data_dir(const char* pkg);
std::string get_cache_dir(const char* pkg);

std::string get_config_dir();
std::string get_data_dir();
std::string get_cache_dir();

/**
 * Check directory existence and create it with given mode if it doesn't.
 * @param path to check, relative or absolute
 * @param dir last directory creation mode
 * @param parents default mode for all created directories except the last
 */
bool check_dir(const char *path, mode_t dir=0755, mode_t parents=0755);
DRING_PUBLIC void set_program_dir(char *program_path); // public because bin/main.cpp uses it
std::string expand_path(const std::string &path);
bool isDirectoryWritable(const std::string &directory);

bool recursive_mkdir(const std::string& path, mode_t mode=0755);

bool isPathRelative(const std::string& path);
/**
 * If path is contained in base, return the suffix, otherwise return the full path.
 * @param base must not finish with DIR_SEPARATOR_STR, can be empty
 * @param path the path
 */
std::string getCleanPath(const std::string& base, const std::string& path);
/**
 * If path is relative, it is appended to base.
 */
std::string getFullPath(const std::string& base, const std::string& path);

bool isFile(const std::string& path, bool resolveSymlink = true);
bool isDirectory(const std::string& path);
bool isSymLink(const std::string& path);

std::chrono::system_clock::time_point writeTime(const std::string& path);

/**
 * Read content of the directory.
 * The result is a list of relative (to @param dir) paths of all entries
 * in the directory, without "." and "..".
 */
std::vector<std::string> readDirectory(const std::string &dir);

/**
 * Read the full content of a file at path.
 * If path is relative, it is appended to default_dir.
 */
std::vector<uint8_t> loadFile(const std::string& path, const std::string& default_dir = {});
std::string loadTextFile(const std::string& path, const std::string& default_dir = {});

void saveFile(const std::string& path, const uint8_t* data, size_t data_size, mode_t mode=0644);
inline void saveFile(const std::string& path, const std::vector<uint8_t>& data, mode_t mode=0644) {
    saveFile(path, data.data(), data.size(), mode);
}

std::vector<uint8_t> loadCacheFile(const std::string& path, std::chrono::system_clock::duration maxAge);
std::string loadCacheTextFile(const std::string& path, std::chrono::system_clock::duration maxAge);

std::vector<uint8_t> readArchive(const std::string& path, const std::string& password = {});
void writeArchive(const std::string& data, const std::string& path, const std::string& password = {});

std::mutex& getFileLock(const std::string& path);

struct FileHandle {
    int fd;
    const std::string name;
    FileHandle(const std::string &name);
    ~FileHandle();
};
FileHandle create_pidfile();

/**
 * Remove a file with optional erasing of content.
 * Return the same value as std::remove().
 */
int remove(const std::string& path, bool erase = false);

/**
 * Prune given directory's content and remove it, symlinks are not followed.
 * Return 0 if succeed, -1 if directory is not removed (content can be removed partially).
 */
int removeAll(const std::string& path, bool erase = false);

/**
 * Wrappers for fstream opening that will convert paths to wstring
 * on windows
 */
void openStream(std::ifstream& file, const std::string& path, std::ios_base::openmode mode = std::ios_base::in);
void openStream(std::ofstream& file, const std::string& path, std::ios_base::openmode mode = std::ios_base::out);
std::ifstream ifstream(const std::string& path, std::ios_base::openmode mode = std::ios_base::in);
std::ofstream ofstream(const std::string& path, std::ios_base::openmode mode = std::ios_base::out);

/**
 * Windows compatibility wrapper for checking read-only attribute
 */
int accessFile(const std::string& file, int mode);

}} // namespace jami::fileutils

#endif // FILEUTILS_H_
