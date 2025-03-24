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
#include "jamidht/conversation_channel_handler.h"

namespace jami {

ConversationChannelHandler::ConversationChannelHandler(const std::shared_ptr<JamiAccount>& acc,
                                                       dhtnet::ConnectionManager& cm)
    : ChannelHandlerInterface()
    , account_(acc)
    , connectionManager_(cm)
{}

ConversationChannelHandler::~ConversationChannelHandler() {}

void
ConversationChannelHandler::connect(const DeviceId& deviceId,
                                    const std::string& channelName,
                                    ConnectCb&& cb,
                                    const std::string& connectionType,
                                    bool forceNewConnection)
{
    connectionManager_.connectDevice(deviceId,
                                     "git://" + deviceId.toString() + "/" + channelName,
                                     std::move(cb));
}

bool
ConversationChannelHandler::onRequest(const std::shared_ptr<dht::crypto::Certificate>& cert,
                                      const std::string& name)
{
    auto acc = account_.lock();
    if (!cert || !cert->issuer || !acc)
        return false;
    // Pre-check before acceptance. Sometimes, another device can start a conversation
    // which is still not synced. So, here we decline channel's request in this case
    // to avoid the other device to want to sync with us if we are not ready.
    auto sep = name.find_last_of('/');
    auto conversationId = name.substr(sep + 1);

    if (auto acc = account_.lock()) {
        if (auto convModule = acc->convModule(true)) {
            auto res = !convModule->isBanned(conversationId, cert->issuer->getId().toString());
            if (!res) {
                JAMI_WARNING("[Account {}] Received ConversationChannel request for '{}' but user {} is banned", acc->getAccountID(), name, cert->issuer->getId().toString());
            } else {
                res &= !convModule->isBanned(conversationId, cert->getLongId().toString());
                if (!res) {
                    JAMI_WARNING("[Account {}] Received ConversationChannel request for '{}' but device {} is banned", acc->getAccountID(), name, cert->getLongId().toString());
                }
            }
            return res;
        } else {
            JAMI_ERROR("Received ConversationChannel request but conversation module is unavailable");
        }
    }
    return false;
}

void
ConversationChannelHandler::onReady(const std::shared_ptr<dht::crypto::Certificate>&,
                                    const std::string&,
                                    std::shared_ptr<dhtnet::ChannelSocket>)
{}

} // namespace jami
