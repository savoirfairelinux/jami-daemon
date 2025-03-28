/*
 *  Copyright (C) 2004-2025 Savoir-faire Linux Inc.
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
#pragma once
#include "jami/def.h"

#include <dhtnet/fileutils.h>

#include <string>
#include <vector>
#include <chrono>
#include <mutex>
#include <cstdio>
#include <ios>
#include <filesystem>
#include <string_view>

#ifndef _WIN32
#include <sys/stat.h>               // mode_t
#define DIR_SEPARATOR_STR     "/"   // Directory separator string
#define DIR_SEPARATOR_CH      '/'   // Directory separator char
#define DIR_SEPARATOR_STR_ESC "\\/" // Escaped directory separator string
#else
#define mode_t                unsigned
#define DIR_SEPARATOR_STR     "\\"  // Directory separator string
#define DIR_SEPARATOR_CH      '\\'  // Directory separator char
#define DIR_SEPARATOR_STR_ESC "//*" // Escaped directory separator string
#endif

namespace jami {
namespace fileutils {

using namespace std::literals;

std::filesystem::path get_config_dir(const char* pkg);
std::filesystem::path get_data_dir(const char* pkg);
std::filesystem::path get_cache_dir(const char* pkg);

const std::filesystem::path& get_home_dir();
const std::filesystem::path& get_config_dir();
const std::filesystem::path& get_data_dir();
const std::filesystem::path& get_cache_dir();

/**
 * Set the program's resource directory path. This is used for clients that may be installed in
 * different locations and are deployed with ringtones and other resources in an application
 * relative directory.
 * @param resource_dir_path The path to the ringtone directory.
 */
LIBJAMI_PUBLIC void set_resource_dir_path(const std::filesystem::path& resourceDirPath);

/**
 * Get the resource directory path that was set with set_resource_dir_path.
 * @return The resource directory path.
 */
const std::filesystem::path& get_resource_dir_path();

/**
 * Expand the given path.
 * @param path The path to be expanded.
 * @return The expanded path as a string.
 */
std::string expand_path(const std::string& path);

bool isPathRelative(const std::filesystem::path& path);

/**
 * If path is contained in base, return the suffix, otherwise return the full path.
 * @param base must not finish with DIR_SEPARATOR_STR, can be empty
 * @param path the path
 */
std::string getCleanPath(const std::string& base, const std::string& path);
/**
 * If path is relative, it is appended to base.
 */
std::filesystem::path getFullPath(const std::filesystem::path& base,
                                  const std::filesystem::path& path);

bool createFileLink(const std::filesystem::path& src,
                    const std::filesystem::path& dest,
                    bool hard = false);

std::string_view getFileExtension(std::string_view filename);

bool isDirectoryWritable(const std::string& directory);

/**
 * Read the full content of a file at path.
 * If path is relative, it is appended to default_dir.
 */
std::vector<uint8_t> loadFile(const std::filesystem::path& path,
                              const std::filesystem::path& default_dir = {});
std::string loadTextFile(const std::filesystem::path& path,
                         const std::filesystem::path& default_dir = {});

void saveFile(const std::filesystem::path& path,
              const uint8_t* data,
              size_t data_size,
              mode_t mode = 0644);
inline void
saveFile(const std::filesystem::path& path, const std::vector<uint8_t>& data, mode_t mode = 0644)
{
    saveFile(path, data.data(), data.size(), mode);
}

std::vector<uint8_t> loadCacheFile(const std::filesystem::path& path,
                                   std::chrono::system_clock::duration maxAge);
std::string loadCacheTextFile(const std::filesystem::path& path,
                              std::chrono::system_clock::duration maxAge);

static constexpr auto ARCHIVE_AUTH_SCHEME_NONE = ""sv;
static constexpr auto ARCHIVE_AUTH_SCHEME_PASSWORD = "password"sv;
static constexpr auto ARCHIVE_AUTH_SCHEME_KEY = "key"sv;

struct ArchiveStorageData
{
    std::string data;
    std::vector<uint8_t> salt;
};
ArchiveStorageData readArchive(const std::filesystem::path& path,
                               std::string_view scheme,
                               const std::string& pwd);

bool writeArchive(const std::string& data,
                  const std::filesystem::path& path,
                  std::string_view scheme,
                  const std::string& password = {},
                  const std::vector<uint8_t>& password_salt = {});

int64_t size(const std::filesystem::path& path);

std::string sha3File(const std::filesystem::path& path);
std::string sha3sum(const std::vector<uint8_t>& buffer);

/**
 * Windows compatibility wrapper for checking read-only attribute
 */
int accessFile(const std::string& file, int mode);

/**
 * Return the last write time (epoch time) of a given file path (in seconds).
 */
uint64_t lastWriteTimeInSeconds(const std::filesystem::path& filePath);

} // namespace fileutils
} // namespace jami
