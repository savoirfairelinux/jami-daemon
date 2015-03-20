/*
 *  Copyright (C) 2011-2015 Savoir-Faire Linux Inc.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "fileutils.h"
#include "logger.h"
#include "intrin.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <libgen.h>
#include <dirent.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#ifndef __ANDROID__
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
bool check_dir(const char *path)
{
    DIR *dir = opendir(path);

    if (!dir) { // doesn't exist
        if (mkdir(path, 0755) != 0) {   // couldn't create the dir
            perror(path);
            return false;
        }
    } else
        closedir(dir);

    return true;
}

#ifdef __ANDROID__
static char *program_dir = "/data/data/cx.ring";
#else
static char *program_dir = NULL;
#endif

void set_program_dir(char *program_path)
{
    program_dir = dirname(program_path);
}

const char *get_program_dir()
{
    return program_dir;
}

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

std::string
expand_path(const std::string &path)
{
#ifdef __ANDROID__
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

bool isDirectoryWritable(const std::string &directory)
{
    return access(directory.c_str(), W_OK) == 0;
}

std::vector<uint8_t>
loadFile(const std::string& path)
{
    std::vector<uint8_t> buffer;
    std::ifstream file(path, std::ios::binary);
    if (!file)
        throw std::runtime_error("Can't read file: "+path);
    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    if (size > std::numeric_limits<unsigned>::max())
        throw std::runtime_error("File is too big: "+path);
    buffer.resize(size);
    file.seekg(0, std::ios::beg);
    if (!file.read((char*)buffer.data(), size))
        throw std::runtime_error("Can't load file: "+path);
    return buffer;
}

void
saveFile(const std::string& path, const std::vector<uint8_t>& data)
{
    std::ofstream file(path, std::ios::trunc | std::ios::binary);
    if (!file.is_open()) {
        RING_ERR("Could not write data to %s", path.c_str());
        return;
    }
    file.write((char*)data.data(), data.size());
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
    while (!readdir_r(dp, reinterpret_cast<dirent*>(buf.data()), &entry) && entry) {
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

std::string
get_cache_dir()
{
    const std::string cache_home(XDG_CACHE_HOME);

    if (not cache_home.empty()) {
        return cache_home;
    } else {
#ifdef __ANDROID__
        return get_home_dir() + DIR_SEPARATOR_STR + PACKAGE;
#elif __APPLE__
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
#ifdef __ANDROID__
    return get_program_dir();
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
    return get_program_dir();
#elif __APPLE__
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

}} // namespace ring::fileutils
