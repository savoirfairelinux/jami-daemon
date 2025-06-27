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

#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <filesystem>
#include <git2.h>
#include "jamidht/jamiaccount.h"

constexpr size_t WAIT_FOR_ANNOUNCEMENT_TIMEOUT = 30;
constexpr size_t WAIT_FOR_REMOVAL_TIMEOUT = 30;

extern void
wait_for_announcement_of(const std::vector<std::string> accountIDs,
                         std::chrono::seconds timeout = std::chrono::seconds(WAIT_FOR_ANNOUNCEMENT_TIMEOUT));

extern void
wait_for_announcement_of(const std::string& accountId,
                         std::chrono::seconds timeout = std::chrono::seconds(WAIT_FOR_ANNOUNCEMENT_TIMEOUT));

extern void
wait_for_removal_of(const std::vector<std::string> accounts,
                    std::chrono::seconds timeout = std::chrono::seconds(WAIT_FOR_REMOVAL_TIMEOUT));

extern void
wait_for_removal_of(const std::string& account,
                    std::chrono::seconds timeout = std::chrono::seconds(WAIT_FOR_REMOVAL_TIMEOUT));

extern std::map<std::string, std::string>
load_actors(const std::filesystem::path& from_yaml);

extern std::map<std::string, std::string>
load_actors_and_wait_for_announcement(const std::string& from_yaml);

extern std::string
addCommit(git_repository* repo,
            const std::shared_ptr<jami::JamiAccount> account,
            const std::string& branch,
            const std::string& commit_msg);
