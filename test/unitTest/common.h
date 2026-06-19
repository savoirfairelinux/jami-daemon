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

#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <filesystem>

constexpr size_t WAIT_FOR_ANNOUNCEMENT_TIMEOUT = 30;
constexpr size_t WAIT_FOR_REMOVAL_TIMEOUT = 30;

extern void wait_for_announcement_of(const std::vector<std::string> accountIDs,
                                     std::chrono::seconds timeout = std::chrono::seconds(WAIT_FOR_ANNOUNCEMENT_TIMEOUT));

extern void wait_for_announcement_of(const std::string& accountId,
                                     std::chrono::seconds timeout = std::chrono::seconds(WAIT_FOR_ANNOUNCEMENT_TIMEOUT));

extern void wait_for_removal_of(const std::vector<std::string> accounts,
                                std::chrono::seconds timeout = std::chrono::seconds(WAIT_FOR_REMOVAL_TIMEOUT));

extern void wait_for_removal_of(const std::string& account,
                                std::chrono::seconds timeout = std::chrono::seconds(WAIT_FOR_REMOVAL_TIMEOUT));

extern std::map<std::string, std::string> load_actors(const std::filesystem::path& from_yaml);

extern std::map<std::string, std::string> load_actors_and_wait_for_announcement(const std::string& from_yaml);

// Make a git object unreachable. Removes the loose object if present (precise:
// only the target object disappears) and returns true. If the object is packed,
// falls back to removing the pack directory and returns false; callers that
// assert on a specific missing object should check the return value so they
// fail loudly rather than silently corrupting unrelated objects. The freshly
// created repositories used by the precision tests store objects loose, so the
// precise path is taken there.
inline bool
makeGitObjectUnreachable(const std::filesystem::path& gitObjectsDir, const std::string& oid)
{
    std::error_code ec;
    auto loose = gitObjectsDir / oid.substr(0, 2) / oid.substr(2);
    if (std::filesystem::exists(loose, ec)) {
        ec.clear();
        return std::filesystem::remove(loose, ec) && !ec;
    }
    auto packDir = gitObjectsDir / "pack";
    if (std::filesystem::is_directory(packDir, ec))
        std::filesystem::remove_all(packDir, ec);
    return false;
}
