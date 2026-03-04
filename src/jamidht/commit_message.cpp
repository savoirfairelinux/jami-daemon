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

#include "string_utils.h"

namespace jami {

Json::Value
CommitMessage::toJson() const
{
    Json::Value value;
    value[CommitKey::TYPE] = type;
    if (type == CommitType::TEXT || type == CommitType::EDITED_MESSAGE) {
        value[CommitKey::BODY] = body;
    }
    if (!replyTo.empty()) {
        value[CommitKey::REPLY_TO] = replyTo;
    }
    if (!reactTo.empty()) {
        value[CommitKey::REACT_TO] = reactTo;
    }
    if (!edit.empty()) {
        value[CommitKey::EDIT] = edit;
    }
    if (!action.empty()) {
        value[CommitKey::ACTION] = action;
    }
    if (!uri.empty()) {
        value[CommitKey::URI] = uri;
    }
    if (!device.empty()) {
        value[CommitKey::DEVICE] = device;
    }
    if (!confId.empty()) {
        value[CommitKey::CONF_ID] = confId;
    }
    if (!to.empty()) {
        value[CommitKey::TO] = to;
    }
    if (!reason.empty()) {
        value[CommitKey::REASON] = reason;
    }
    if (!duration.empty()) {
        value[CommitKey::DURATION] = duration;
    }
    if (type == CommitType::DATA_TRANSFER) {
        value[CommitKey::TID] = tid;
    }
    if (!displayName.empty()) {
        value[CommitKey::DISPLAY_NAME] = displayName;
    }
    if (totalSize >= 0) {
        value[CommitKey::TOTAL_SIZE] = std::to_string(totalSize);
    }
    if (!sha3sum.empty()) {
        value[CommitKey::SHA3SUM] = sha3sum;
    }
    if (mode >= 0) {
        value[CommitKey::MODE] = mode;
    }
    if (!invited.empty()) {
        value[CommitKey::INVITED] = invited;
    }
    return value;
}

std::string
CommitMessage::toString() const
{
    if (type == CommitType::MERGE) {
        return body;
    }
    return json::toString(toJson());
}

std::optional<CommitMessage>
CommitMessage::fromString(const std::string& str)
{
    if (str.starts_with("Merge commit")) {
        CommitMessage msg;
        msg.type = CommitType::MERGE;
        msg.body = str;
        return msg;
    }
    Json::Value value;
    if (!json::parse(str, value) || !value.isObject()) {
        return std::nullopt;
    }

    CommitMessage msg;
    try {
        msg.type = value.get(CommitKey::TYPE, "").asString();
        msg.body = value.get(CommitKey::BODY, "").asString();
        msg.replyTo = value.get(CommitKey::REPLY_TO, "").asString();
        msg.reactTo = value.get(CommitKey::REACT_TO, "").asString();
        msg.edit = value.get(CommitKey::EDIT, "").asString();
        msg.action = value.get(CommitKey::ACTION, "").asString();
        msg.uri = value.get(CommitKey::URI, "").asString();
        msg.device = value.get(CommitKey::DEVICE, "").asString();
        msg.confId = value.get(CommitKey::CONF_ID, "").asString();
        msg.to = value.get(CommitKey::TO, "").asString();
        msg.reason = value.get(CommitKey::REASON, "").asString();
        msg.duration = value.get(CommitKey::DURATION, "").asString();
        msg.tid = value.get(CommitKey::TID, "").asString();
        msg.displayName = value.get(CommitKey::DISPLAY_NAME, "").asString();
        msg.totalSize = to_int<int64_t>(value.get(CommitKey::TOTAL_SIZE, "-1").asString());
        msg.sha3sum = value.get(CommitKey::SHA3SUM, "").asString();
        msg.mode = value.get(CommitKey::MODE, -1).asInt();
        msg.invited = value.get(CommitKey::INVITED, "").asString();
    } catch (const std::exception& e) {
        JAMI_ERROR("Exception while parsing commit message '{}': {}", str, e.what());
        return std::nullopt;
    }
    return msg;
}

} // namespace jami
