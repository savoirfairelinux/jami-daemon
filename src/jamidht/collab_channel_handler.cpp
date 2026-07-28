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
#include "jamidht/collab_channel_handler.h"

#include "jamidht/collab_repository.h"
#include "jamidht/conversation_module.h"

namespace jami {

static constexpr std::string_view COLLAB_SCHEME {"collab://"};

CollabChannelHandler::CollabChannelHandler(const std::shared_ptr<JamiAccount>& acc, dhtnet::ConnectionManager& cm)
    : ChannelHandlerInterface()
    , account_(acc)
    , connectionManager_(cm)
{}

CollabChannelHandler::~CollabChannelHandler() {}

bool
CollabChannelHandler::parse(std::string_view name,
                            std::string& deviceId,
                            std::string& conversationId,
                            std::string& documentId)
{
    if (!name.starts_with(COLLAB_SCHEME))
        return false;
    auto rest = name.substr(COLLAB_SCHEME.size());
    auto firstSep = rest.find('/');
    if (firstSep == std::string_view::npos)
        return false;
    auto secondSep = rest.find('/', firstSep + 1);
    if (secondSep == std::string_view::npos)
        return false;
    deviceId = rest.substr(0, firstSep);
    conversationId = rest.substr(firstSep + 1, secondSep - firstSep - 1);
    documentId = rest.substr(secondSep + 1);
    // Both ids end up in a filesystem path, so hold them to the hexadecimal
    // alphabet they are generated with. Without this a peer could ask for
    // "<conv>/../../<other account>/conversations/<private conv>" and have us
    // serve a repository it has no business reading.
    return !deviceId.empty() && CollabRepository::isValidId(conversationId) && CollabRepository::isValidId(documentId);
}

void
CollabChannelHandler::connect(const DeviceId& deviceId,
                              const std::string& channelName,
                              ConnectCb&& cb,
                              const std::string& /*connectionType*/,
                              bool /*forceNewConnection*/)
{
    connectionManager_.connectDevice(deviceId,
                                     fmt::format("{}{}/{}", COLLAB_SCHEME, deviceId, channelName),
                                     std::move(cb));
}

bool
CollabChannelHandler::onRequest(const std::shared_ptr<dht::crypto::Certificate>& cert, const std::string& name)
{
    auto acc = account_.lock();
    if (!cert || !cert->issuer || !acc)
        return false;

    std::string targetDevice, conversationId, documentId;
    if (!parse(name, targetDevice, conversationId, documentId))
        return false;

    auto* convModule = acc->convModule(true);
    if (!convModule) {
        JAMI_ERROR("Received CollabChannel request but conversation module is unavailable");
        return false;
    }
    // A document is readable by exactly the devices allowed to read the
    // conversation that announced it.
    return convModule->isPeerAuthorized(conversationId,
                                        cert->issuer->getId().toString(),
                                        cert->getLongId().toString(),
                                        true);
}

void
CollabChannelHandler::onReady(const std::shared_ptr<dht::crypto::Certificate>&,
                              const std::string&,
                              std::shared_ptr<dhtnet::ChannelSocket>)
{
    // Handled by JamiAccount::onConnectionReady, which owns the git servers.
}

} // namespace jami
