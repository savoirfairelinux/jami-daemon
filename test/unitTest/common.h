/*
 *  Copyright (C) 2021-2023 Savoir-faire Linux Inc.
 *
 *  Author: Olivier Dion <olivier.dion>@savoirfairelinux.com>
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

#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <string>

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
load_actors(const std::string& from_yaml);

extern std::map<std::string, std::string>
load_actors_and_wait_for_announcement(const std::string& from_yaml);
