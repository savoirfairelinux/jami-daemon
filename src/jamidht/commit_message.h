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

#include <optional>
#include <string>

namespace jami {

namespace CommitKey {
constexpr const char* const TYPE {"type"};
constexpr const char* const BODY {"body"};
constexpr const char* const REPLY_TO {"reply-to"};
constexpr const char* const REACT_TO {"react-to"};
constexpr const char* const EDIT {"edit"};
constexpr const char* const ACTION {"action"};
constexpr const char* const URI {"uri"};
constexpr const char* const DEVICE {"device"};
constexpr const char* const CONF_ID {"confId"};
constexpr const char* const TO {"to"};
constexpr const char* const REASON {"reason"};
constexpr const char* const DURATION {"duration"};
constexpr const char* const TID {"tid"};
constexpr const char* const DISPLAY_NAME {"displayName"};
constexpr const char* const TOTAL_SIZE {"totalSize"};
constexpr const char* const SHA3SUM {"sha3sum"};
constexpr const char* const MODE {"mode"};
constexpr const char* const INVITED {"invited"};
} // namespace CommitKey

namespace CommitType {
constexpr const char* const TEXT {"text/plain"};
constexpr const char* const CALL_HISTORY {"application/call-history+json"};
constexpr const char* const DATA_TRANSFER {"application/data-transfer+json"};
constexpr const char* const MEMBER {"member"};
constexpr const char* const INITIAL {"initial"};
constexpr const char* const VOTE {"vote"};
constexpr const char* const UPDATE_PROFILE {"application/update-profile"};
constexpr const char* const MERGE {"merge"};
} // namespace CommitType

struct CommitMessage
{
    std::string type {};
    std::string body {};
    std::string replyTo {};
    std::string reactTo {};
    std::string edit {};

    std::string action {};

    std::string uri {};
    std::string device {};
    std::string confId {};
    std::string to {};
    std::string reason {};
    uint64_t duration {0};

    uint64_t tid {0};
    std::string displayName {};
    int64_t totalSize {-1};
    std::string sha3sum {};

    int mode {-1};
    std::string invited {};

    Json::Value toJson() const;
    std::string toString() const;
    static CommitMessage fromJson(const Json::Value& value);
    static std::optional<CommitMessage> fromString(const std::string& str);
};

} // namespace jami
