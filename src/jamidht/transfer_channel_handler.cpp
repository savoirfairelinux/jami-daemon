/*
 *  Copyright (C) 2021-2022 Savoir-faire Linux Inc.
 *
 *  Author: Sébastien Blin <sebastien.blin@savoirfairelinux.com>
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

#include "jamidht/transfer_channel_handler.h"

#include <charconv>

#include "fileutils.h"

namespace jami {

TransferChannelHandler::TransferChannelHandler(const std::shared_ptr<JamiAccount>& account,
                                               ConnectionManager& cm)
    : ChannelHandlerInterface()
    , account_(account)
    , connectionManager_(cm)
{
    auto acc = account_.lock();
    idPath_ = fileutils::get_data_dir() + DIR_SEPARATOR_STR + acc->getAccountID();
}

TransferChannelHandler::~TransferChannelHandler() {}

void
TransferChannelHandler::connect(const DeviceId& deviceId,
                                const std::string& channelName,
                                ConnectCb&& cb)
{}

bool
TransferChannelHandler::onRequest(const std::shared_ptr<dht::crypto::Certificate>& cert,
                                  const std::string& name)
{
    auto acc = account_.lock();
    if (!acc || !cert || !cert->issuer)
        return false;
    auto uri = cert->issuer->getId().toString();
    // Else, check if it's a profile or file in a conversation.
    auto idstr = name.substr(16);
    if (idstr == "profile.vcf") {
        // If it's our profile from another device
        return uri == acc->getUsername();
    }
    auto sep = idstr.find('/');
    auto lastSep = idstr.find_last_of('/');
    auto conversationId = idstr.substr(0, sep);
    auto fileHost = idstr.substr(sep + 1, lastSep - sep - 1);
    auto fileId = idstr.substr(lastSep + 1);
    if (fileHost == acc->currentDeviceId())
        return false;

    sep = fileId.find_last_of('?');
    if (sep != std::string::npos) {
        fileId = fileId.substr(0, sep);
    }

    // Check if peer is member of the conversation
    if (fileId == fmt::format("{}.vcf", acc->getUsername()) || fileId == "profile.vcf") {
        // Or a member from the conversation
        auto members = acc->convModule()->getConversationMembers(conversationId);
        return std::find_if(members.begin(), members.end(), [&](auto m) { return m["uri"] == uri; })
               != members.end();
    } else if (fileHost == "profile") {
        // If a profile is sent, check if it's from another device
        return uri == acc->getUsername();
    }

    return acc->convModule()->onFileChannelRequest(conversationId,
                                                   uri,
                                                   fileId,
                                                   acc->sha3SumVerify());
}

void
TransferChannelHandler::onReady(const std::shared_ptr<dht::crypto::Certificate>&,
                                const std::string& name,
                                std::shared_ptr<ChannelSocket> channel)
{
    auto acc = account_.lock();
    if (!acc)
        return;

    auto idstr = name.substr(16);
    if (idstr == "profile.vcf") {
        if (!channel->isInitiator()) {
            acc->dataTransfer()->onIncomingProfile(channel);
        } else {
            // If it's a profile from sync
            std::string path = fmt::format("{}/profile.vcf", idPath_);
            acc->dataTransfer()->transferFile(channel, idstr, "", path);
        }
        return;
    }

    auto splitted_id = split_string(idstr, '/');
    if (splitted_id.size() < 3) {
        JAMI_ERR() << "Unsupported ID detected " << name;
        channel->shutdown();
        return;
    }

    // convId/fileHost/fileId or convId/profile/fileId
    auto conversationId = std::string(splitted_id[0]);
    auto fileHost = std::string(splitted_id[1]);
    auto isContactProfile = splitted_id[1] == "profile";
    auto fileId = std::string(splitted_id[splitted_id.size() - 1]);
    if (channel->isInitiator())
        return;

    auto sep = fileId.find_last_of('?');
    std::string arguments;
    if (sep != std::string::npos) {
        arguments = fileId.substr(sep + 1);
        fileId = fileId.substr(0, sep);
    }

    // Profile for a member in the conversation
    if (fileId == fmt::format("{}.vcf", acc->getUsername())) {
        std::string path = fmt::format("{}/profile.vcf", idPath_);
        acc->dataTransfer()->transferFile(channel, fileId, "", path);
        return;
    } else if (isContactProfile && fileId.find(".vcf") != std::string::npos) {
        auto path = acc->dataTransfer()->profilePath(fileId.substr(0, fileId.size() - 4));
        acc->dataTransfer()->transferFile(channel, fileId, "", path);
        return;
    } else if (fileId == "profile.vcf") {
        acc->dataTransfer()->onIncomingProfile(channel);
        return;
    }
    // Check if it's a file in a conversation
    auto dt = acc->dataTransfer(conversationId);
    sep = fileId.find('_');
    if (!dt or sep == std::string::npos) {
        channel->shutdown();
        return;
    }
    auto interactionId = fileId.substr(0, sep);
    std::string path = dt->path(fileId);
    auto start = 0u, end = 0u;
    for (const auto arg : split_string(arguments, '&')) {
        auto keyVal = split_string(arg, '=');
        if (keyVal.size() == 2) {
            if (keyVal[0] == "start") {
                start = to_int<unsigned>(keyVal[1]);
            } else if (keyVal[0] == "end") {
                end = to_int<unsigned>(keyVal[1]);
            }
        }
    }

    dt->transferFile(channel, fileId, interactionId, path, start, end);
}

} // namespace jami