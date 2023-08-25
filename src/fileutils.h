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
#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <mutex>
#include <cstdio>
#include <ios>

#include "jami/def.h"
#include <dhtnet/fileutils.h>

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
LIBJAMI_PUBLIC void set_program_dir(char* program_path); // public because bin/main.cpp uses it
std::string expand_path(const std::string& path);

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
std::filesystem::path getFullPath(const std::filesystem::path& base, const std::string& path);

bool createFileLink(const std::string& src, const std::string& dest, bool hard = false);

std::string_view getFileExtension(std::string_view filename);

bool isDirectoryWritable(const std::string& directory);

/**
 * Read the full content of a file at path.
 * If path is relative, it is appended to default_dir.
 */
std::vector<uint8_t> loadFile(const std::string& path, const std::string& default_dir = {});
std::string loadTextFile(const std::string& path, const std::string& default_dir = {});

bool copy(const std::string& src, const std::string& dest);

void saveFile(const std::string& path, const uint8_t* data, size_t data_size, mode_t mode = 0644);
inline void
saveFile(const std::string& path, const std::vector<uint8_t>& data, mode_t mode = 0644)
{
    saveFile(path, data.data(), data.size(), mode);
}

std::vector<uint8_t> loadCacheFile(const std::string& path,
                                   std::chrono::system_clock::duration maxAge);
std::string loadCacheTextFile(const std::string& path, std::chrono::system_clock::duration maxAge);

std::vector<uint8_t> readArchive(const std::string& path, const std::string& password = {});
void writeArchive(const std::string& data,
                  const std::string& path,
                  const std::string& password = {});

/**
 * Wrappers for fstream opening that will convert paths to wstring
 * on windows
 */
void openStream(std::ifstream& file,
                const std::string& path,
                std::ios_base::openmode mode = std::ios_base::in);
void openStream(std::ofstream& file,
                const std::string& path,
                std::ios_base::openmode mode = std::ios_base::out);
std::ifstream ifstream(const std::string& path, std::ios_base::openmode mode = std::ios_base::in);
std::ofstream ofstream(const std::string& path, std::ios_base::openmode mode = std::ios_base::out);

int64_t size(const std::string& path);

std::string sha3File(const std::string& path);
std::string sha3sum(const std::vector<uint8_t>& buffer);

/**
 * Windows compatibility wrapper for checking read-only attribute
 */
int accessFile(const std::string& file, int mode);

/**
 * Return the last write time (epoch time) of a given file path (in seconds).
 */
uint64_t lastWriteTimeInSeconds(const std::string& filePath);

} // namespace fileutils
} // namespace jami
