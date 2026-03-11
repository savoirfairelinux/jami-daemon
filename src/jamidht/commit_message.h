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
constexpr const char* const MEMBER {"member"};
constexpr const char* const CALL_HISTORY {"application/call-history+json"};
constexpr const char* const DATA_TRANSFER {"application/data-transfer+json"};
constexpr const char* const INITIAL {"initial"};
constexpr const char* const VOTE {"vote"};
constexpr const char* const UPDATE_PROFILE {"application/update-profile"};
constexpr const char* const MERGE {"merge"};
// Jami no longer creates messages of type "application/edited-message", but we
// still need to be able to parse them for backward compatibility.
constexpr const char* const EDITED_MESSAGE {"application/edited-message"};
} // namespace CommitType

namespace CommitAction {
constexpr const char* const ADD {"add"};
constexpr const char* const JOIN {"join"};
constexpr const char* const REMOVE {"remove"};
constexpr const char* const BAN {"ban"};
constexpr const char* const UNBAN {"unban"};
} // namespace CommitAction

/*
 * Jami conversations are stored as git repositories. Most of the information is contained
 * in the commit messages. With the exception of merge commits, the commit messages are JSON
 * objects with a fixed set of possible fields defined in the CommitKey namespace above. The
 * "type" field is mandatory and determines which other fields can be present as well as their
 * meaning.
 */
struct CommitMessage
{
    std::string type {};
    std::string body {};
    std::string replyTo {};
    std::string reactTo {};
    std::string editedId {};
    std::string action {};
    std::string uri {};
    std::string confId {};
    std::string device {};
    std::string duration {};
    std::string reason {};
    std::string to {};
    std::string tid {};
    std::string displayName {};
    int64_t totalSize {-1};
    std::string sha3sum {};
    int mode {-1};
    std::string invited {};

    // User messages are stored as commits of type "text/plain". The message text is in the "body"
    // field. For example:
    //
    //     {"body":"Hello!","type":"text/plain"}
    //
    // When a user edits a message, the new message text is stored in the "body" field of a commit
    // of type "text/plain" (or "application/edited-message" in old versions of Jami), with an
    // additional "edit" field containing the ID of the commit being edited. For example:
    //
    //     {"body":"Hello, how are you?","edit":"7de8a42695da4de31f774df7040893d34de0829d","type":"text/plain"}
    //     {"body":"Hi!","edit":"81528f849e844b6b0ded23b92ed0fc8d06bc21a2","type":"application/edited-message"}
    //
    // If the same message is edited multiple times, the ID in the "edit" field always refers to
    // the original message, not the previous edit.
    //
    // A deleted message is represented by an edit with an empty "body", e.g.:
    //
    //     {"body":"","edit":"7de8a42695da4de31f774df7040893d34de0829d","type":"text/plain"}
    //
    // Text messages can optionally include a "reply-to" field with the ID of the message being
    // replied to, e.g.:
    //
    //     {"body":"You're right!","reply-to":"200779c99a3f6ed7efc2a83bdfddb6a9e45a4e55","type":"text/plain"}
    //
    // The "edit" and "reply-to" fields are mutually exclusive. When replying to an edited message,
    // the "reply-to" field contains the ID of the original message, not the edit.
    //
    // Reactions are also encoded as messages of type "text/plain" with a "react-to" field containing
    // the ID of the message being reacted to and the reaction itself in the "body" field, e.g.:
    //
    //     {"body":"\ud83d\udc4d","react-to":"d433e037b32314bc3de2fad2ca4a12d914ecad57","type":"text/plain"}
    //
    // Removing a reaction is done the same way as deleting a message, i.e. as an edit with an
    // empty "body":
    //
    //     {"body":"","edit":"9f5a429073f313d3edb149c61fbd682c6e0fc704","type":"text/plain"}
    //
    // The "react-to" field is mutually exclusive with the "edit" and "reply-to" fields.
    static CommitMessage text(const std::string& body, const std::string& replyToId = "")
    {
        CommitMessage msg;
        msg.type = CommitType::TEXT;
        msg.body = body;
        msg.replyTo = replyToId;
        return msg;
    }
    static CommitMessage reaction(const std::string& reaction, const std::string& reactToId)
    {
        CommitMessage msg;
        msg.type = CommitType::TEXT;
        msg.body = reaction;
        msg.reactTo = reactToId;
        return msg;
    }
    static CommitMessage edit(const std::string& newBody, const std::string& editedId)
    {
        CommitMessage msg;
        msg.type = CommitType::TEXT;
        msg.body = newBody;
        msg.editedId = editedId;
        return msg;
    }

    // Commits of type "member" always have an "action" field and a "uri" field. The "uri" field
    // contains the Jami ID of the user impacted by the action, which can be one of the following:
    // - "add": the user was invited to join the conversation
    // - "join": the user joined the conversation
    // - "remove": the user left the conversation
    // - "ban": the user was banned from the conversation
    // - "unban": the user was unbanned from the conversation
    // For example:
    //
    //     {"action":"join","type":"member","uri":"f32701058c69f8ad6a095c6d14650294a4ba39a3"}
    static CommitMessage member(const std::string& action, const std::string& memberId)
    {
        CommitMessage msg;
        msg.type = CommitType::MEMBER;
        msg.action = action;
        msg.uri = memberId;
        return msg;
    }

    // Commits of type "application/call-history+json" represent either the beginning or the end
    // of a call. Their format differs depending on whether the call was started in a one-to-one
    // conversation or in a group conversation.
    //
    // In a one-to-one conversation, the user who initiated a call creates a commit once it ends.
    // The "duration" field contains the duration of the call in milliseconds, and the "to" field
    // contains the Jami ID of the called peer. For example:
    //
    //      {"duration":"80805","to":"ff114e1934db7b79e4f7ac676cb943d97ffb6a32","type":"application/call-history+json"}
    //
    // If the call failed to start, the "reason" field may provide more information about the cause
    // of the failure. For example, the call may have been declined by the peer:
    //
    //     {"duration":"0","reason":"declined","to":"ff114e1934db7b79e4f7ac676cb943d97ffb6a32","type":"application/call-history+json"}
    //
    // In a group conversation, the host creates a commit when the call starts, and another one
    // when it ends. Both commits include the following fields:
    // - "confId": a 64-bit unsigned integer identifying the call
    // - "device": the host's device ID
    // - "uri": the host's Jami ID
    // The end call commit additionally includes a "duration" field with the call duration in
    // milliseconds. A pair of start/end commits for a group call may look like this:
    //
    //     {"confId":"6342183642926168","device":"c87dc5b688c0e6a7d1cd30fe5c2b4a24aa68d6387ebba9aa7cbb487419578ea1","type":"application/call-history+json","uri":"079ddd3b04f35f6381f2516315e6aa5b98d43ef4"}
    //     {"confId":"6342183642926168","device":"c87dc5b688c0e6a7d1cd30fe5c2b4a24aa68d6387ebba9aa7cbb487419578ea1","duration":"9142","type":"application/call-history+json","uri":"079ddd3b04f35f6381f2516315e6aa5b98d43ef4"}
    static CommitMessage outgoingCallEnd(const std::string& peer, uint64_t duration_ms, const std::string& reason = "")
    {
        CommitMessage msg;
        msg.type = CommitType::CALL_HISTORY;
        msg.to = peer;
        msg.duration = std::to_string(duration_ms);
        msg.reason = reason;
        return msg;
    }
    static CommitMessage conferenceHostingStart(const std::string& confId,
                                                const std::string& device,
                                                const std::string& hostId)
    {
        CommitMessage msg;
        msg.type = CommitType::CALL_HISTORY;
        msg.confId = confId;
        msg.device = device;
        msg.uri = hostId;
        return msg;
    }
    static CommitMessage conferenceHostingEnd(const std::string& confId,
                                              const std::string& device,
                                              const std::string& hostId,
                                              uint64_t duration_ms)
    {
        CommitMessage msg;
        msg.type = CommitType::CALL_HISTORY;
        msg.confId = confId;
        msg.device = device;
        msg.uri = hostId;
        msg.duration = std::to_string(duration_ms);
        return msg;
    }

    // When a user sends a file in a conversation, a commit of type "application/data-transfer+json"
    // is created with the following fields:
    // - "displayName": the file name
    // - "sha3sum": the SHA3-512 hash of the file content, encoded as a hexadecimal string
    // - "tid": an ID for the file transfer (currently consists of a nonzero 64-bit unsigned integer
    //   generated randomly by the sender, encoded as a decimal string)
    // - "totalSize": the file size in bytes
    // For example:
    //
    //     {"displayName":"some_image.png","sha3sum":"5ce2fb16eb16c9dc42f824218ec0b7be4927d9f9fef9860161159faee1c4236a758aeb4ed98b27bf439364ea3199fce23181be4720c79756cf714271b702efcd","tid":"6147910008623250","totalSize":"581","type":"application/data-transfer+json"}
    //
    // A deleted file is represented by a commit of type "application/data-transfer+json" with an edit
    // field containing the ID of the original commit and an empty "tid", e.g.:
    //
    //     {"edit":"7fc3b0cba7e0742b0753051a576a5d17a77a57d0","tid":"","type":"application/data-transfer+json"}
    static CommitMessage fileSent(const std::string& displayName,
                                  const std::string& sha3sum,
                                  const std::string& tid,
                                  int64_t totalSize)
    {
        CommitMessage msg;
        msg.type = CommitType::DATA_TRANSFER;
        msg.displayName = displayName;
        msg.sha3sum = sha3sum;
        msg.tid = tid;
        msg.totalSize = totalSize;
        return msg;
    }
    static CommitMessage fileDeleted(const std::string& fileCommitId)
    {
        CommitMessage msg;
        msg.type = CommitType::DATA_TRANSFER;
        msg.editedId = fileCommitId;
        return msg;
    }

    // Every Jami conversation starts with a commit of type "initial" containing a "mode" field indicating the
    // kind of conversation. There are currently two supported values for the mode: 0 (ConversationMode::ONE_TO_ONE)
    // and 2 (ConversationMode::INVITES_ONLY). In the case of one-to-one conversations (mode 0), there is an
    // additional "invited" field containing the Jami ID of the other participant. For example:
    //
    //     {"invited":"f32701048c59f9ad6a095c6d14650294b4cf30a4","mode":0,"type":"initial"}
    //     {"mode":2,"type":"initial"}
    //
    // Jami allows users to create one-to-one conversations with themselves. In that case, the "invited" field is
    // still present and contains the user's own Jami ID.
    static CommitMessage initial(int mode, const std::string& invitedId = "")
    {
        CommitMessage msg;
        msg.type = CommitType::INITIAL;
        msg.mode = mode;
        msg.invited = invitedId;
        return msg;
    }

    // Commits of type "vote" are created by admins when voting to ban or unban a user from a
    // conversation. They have a "uri" field with the Jami ID of the user being voted on, e.g.:
    //
    //     {"type":"vote","uri":"f32701048c59f9ad6a095c6d14650294b4cf30a4"}
    static CommitMessage vote(const std::string& userId)
    {
        CommitMessage msg;
        msg.type = CommitType::VOTE;
        msg.uri = userId;
        return msg;
    }

    // Commits that modify a conversation's profile (which is stored in the 'profile.vcf' file at
    // the root of the conversation repository) are of type "application/update-profile". They
    // do not contain any additional fields:
    //
    //      {"type":"application/update-profile"}
    static CommitMessage updateProfile()
    {
        CommitMessage msg;
        msg.type = CommitType::UPDATE_PROFILE;
        return msg;
    }

    Json::Value toJson() const;
    std::string toString() const;
    static std::optional<CommitMessage> fromString(const std::string& str);
};

} // namespace jami
