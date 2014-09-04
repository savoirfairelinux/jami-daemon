/*
 *  Copyright (C) 2011-2013 Savoir-Faire Linux Inc.
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#ifndef FILEUTILS_H_
#define FILEUTILS_H_

#include <string>

#define PROTECTED_GETENV(str) ({char *envvar_ = getenv((str)); \
                                                   envvar_ ? envvar_ : "";})

#define XDG_DATA_HOME           (PROTECTED_GETENV("XDG_DATA_HOME"))
#define XDG_CONFIG_HOME         (PROTECTED_GETENV("XDG_CONFIG_HOME"))
#define XDG_CACHE_HOME          (PROTECTED_GETENV("XDG_CACHE_HOME"))

#define PIDFILE ".sfl.pid"


#define DIR_SEPARATOR_STR "/"   // Directory separator char
#define DIR_SEPARATOR_CH  '/'  // Directory separator string

namespace fileutils {
    std::string get_data_dir();
    std::string get_home_dir();
    std::string get_cache_dir();
    bool check_dir(const char *path);
    void set_program_dir(char *program_path);
    const char *get_program_dir();
    std::string get_ringtone_dir();
    std::string expand_path(const std::string &path);
    bool isDirectoryWritable(const std::string &directory);
    struct FileHandle {
        int fd;
        const std::string name;
        FileHandle(const std::string &name);
        ~FileHandle();
    };
    FileHandle create_pidfile();
}

#endif	// FILEUTILS_H_
