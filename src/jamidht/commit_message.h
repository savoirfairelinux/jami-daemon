/*
 * Copyright (C) 2026 Savoir-faire Linux Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "json_utils.h"

#include <string>

namespace jami {

struct CommitMessage
{
    std::string type {};
    std::string body {};
    std::string replyTo {};
    std::string reactTo {};
    std::string edit {};

    std::string uri {};
    std::string device {};
    std::string confId {};
    std::string to {};
    std::string reason {};
    uint64_t duration {0};

    uint64_t tid {0};
    std::string displayName {};
    int64_t totalSize {0};
    std::string sha3sum {};

    Json::Value toJson() const;
    std::string toString() const;
};

} // namespace jami
