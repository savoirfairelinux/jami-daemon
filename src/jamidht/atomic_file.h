/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
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

#include <opendht/infohash.h>

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>

// Header-only so that standalone repository tests do not need the daemon file utilities.
namespace jami::fileutils {

// Reads a whole file, or nothing when it is missing, unreadable or larger than limit.
inline std::optional<std::string>
readBounded(const std::filesystem::path& path, size_t limit)
{
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error || size > limit)
        return std::nullopt;
    std::ifstream file(path, std::ios::binary);
    std::string data(size, '\0');
    if (!file || !file.read(data.data(), static_cast<std::streamsize>(size)))
        return std::nullopt;
    return data;
}

// Replaces a file through a rename, so readers see either the old or the new content.
inline bool
writeAtomic(const std::filesystem::path& path, std::string_view data)
{
    auto temporary = path;
    temporary += "." + dht::InfoHash::getRandom().toString().substr(0, 16);
    std::error_code error;
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        file.write(data.data(), static_cast<std::streamsize>(data.size()));
        if (!file) {
            file.close();
            std::filesystem::remove(temporary, error);
            return false;
        }
    }
    std::filesystem::rename(temporary, path, error);
    if (error)
        std::filesystem::remove(temporary, error);
    return !error;
}

} // namespace jami::fileutils
