/*
 * Copyright (C) 2004-2026 Savoir-faire Linux Inc.
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

#include "commit_message.h"

namespace jami {

Json::Value
CommitMessage::toJson() const
{
    Json::Value value;
    value["type"] = type;
    if (!body.empty()) {
        value["body"] = body;
    }
    if (!replyTo.empty()) {
        value["reply-to"] = replyTo;
    }
    if (!reactTo.empty()) {
        value["react-to"] = reactTo;
    }
    if (!edit.empty()) {
        value["edit"] = edit;
    }
    if (!uri.empty()) {
        value["uri"] = uri;
    }
    if (!device.empty()) {
        value["device"] = device;
    }
    if (!confId.empty()) {
        value["confId"] = confId;
    }
    if (!to.empty()) {
        value["to"] = to;
    }
    if (!reason.empty()) {
        value["reason"] = reason;
    }
    if (duration != 0) {
        value["duration"] = std::to_string(duration);
    }
    if (tid != 0) {
        value["tid"] = tid;
    }
    if (!displayName.empty()) {
        value["displayName"] = displayName;
    }
    if (totalSize != 0) {
        value["totalSize"] = totalSize;
    }
    if (!sha3sum.empty()) {
        value["sha3sum"] = sha3sum;
    }
    return value;
}

std::string
CommitMessage::toString() const
{
    return json::toString(toJson());
}

} // namespace jami
