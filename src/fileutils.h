/*
 *  Copyright (C) 2011-2016 Savoir-faire Linux Inc.
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

#define PROTECTED_GETENV(str) ({char *envvar_ = getenv((str)); \
                                                   envvar_ ? envvar_ : "";})

#define XDG_DATA_HOME           (PROTECTED_GETENV("XDG_DATA_HOME"))
#define XDG_CONFIG_HOME         (PROTECTED_GETENV("XDG_CONFIG_HOME"))
#define XDG_CACHE_HOME          (PROTECTED_GETENV("XDG_CACHE_HOME"))

#define PIDFILE ".ring.pid"


#ifndef _WIN32
#include <sys/stat.h>           // mode_t
#define DIR_SEPARATOR_STR "/"   // Directory separator char
#define DIR_SEPARATOR_CH  '/'   // Directory separator string
#else
#define mode_t unsigned
#define DIR_SEPARATOR_STR "\\"  // Directory separator char
#define DIR_SEPARATOR_CH  '\\'  // Directory separator string
#endif

namespace ring { namespace fileutils {

    std::string get_home_dir();
    std::string get_config_dir();
    std::string get_data_dir();
    std::string get_cache_dir();

    /**
     * Check directory existance and create it with given mode if it doesn't.
     * @path path to check, relative or absolute
     * @dir last directory creation mode
     * @param parents default mode for all created directories except the last
     */
    bool check_dir(const char *path, mode_t dir=0755, mode_t parents=0755);
    void set_program_dir(char *program_path);
    std::string expand_path(const std::string &path);
    bool isDirectoryWritable(const std::string &directory);

    bool recursive_mkdir(const std::string& path, mode_t mode=0755);

    bool isDirectory(const std::string& path);

    std::chrono::system_clock::time_point writeTime(const std::string& path);

    /**
     * Read content of the directory.
     * The result is a list of full paths of files in the directory,
     * without "." and "..".
     */
    std::vector<std::string> readDirectory(const std::string &dir);

    std::vector<uint8_t> loadFile(const std::string& path);
    void saveFile(const std::string& path, const std::vector<uint8_t>& data, mode_t mode=0644);

    struct FileHandle {
        int fd;
        const std::string name;
        FileHandle(const std::string &name);
        ~FileHandle();
    };
    FileHandle create_pidfile();

}} // namespace ring::fileutils

#endif // FILEUTILS_H_
