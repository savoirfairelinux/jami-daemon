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
#pragma once

#include "dbusnetworkservicemanager.adaptor.h"
#include <networkservice_interface.h>

class DBusNetworkServiceManager : public sdbus::AdaptorInterfaces<cx::ring::Ring::NetworkServiceManager_adaptor>
{
public:
    DBusNetworkServiceManager(sdbus::IConnection& connection)
        : AdaptorInterfaces(connection, sdbus::ObjectPath("/cx/ring/Ring/NetworkServiceManager"))
    {
        registerAdaptor();
        registerSignalHandlers();
    }

    ~DBusNetworkServiceManager() { unregisterAdaptor(); }

    auto getSharedServices(const std::string& accountId) -> decltype(libjami::getSharedServices(accountId))
    {
        return libjami::getSharedServices(accountId);
    }

    auto addSharedService(const std::string& accountId, const std::map<std::string, std::string>& service)
        -> decltype(libjami::addSharedService(accountId, service))
    {
        return libjami::addSharedService(accountId, service);
    }

    auto updateSharedService(const std::string& accountId, const std::map<std::string, std::string>& service)
        -> decltype(libjami::updateSharedService(accountId, service))
    {
        return libjami::updateSharedService(accountId, service);
    }

    auto removeSharedService(const std::string& accountId,
                              const std::string& serviceId) -> decltype(libjami::removeSharedService(accountId,
                                                                                                      serviceId))
    {
        return libjami::removeSharedService(accountId, serviceId);
    }

    auto queryPeerServices(const std::string& accountId,
                           const std::string& peerUri) -> decltype(libjami::queryPeerServices(accountId, peerUri))
    {
        return libjami::queryPeerServices(accountId, peerUri);
    }

    auto openServiceTunnel(const std::string& accountId,
                           const std::string& peerUri,
                           const std::string& peerDevice,
                           const std::string& serviceId,
                           const std::string& serviceName,
                           const uint16_t& localPort)
        -> decltype(libjami::openServiceTunnel(accountId, peerUri, peerDevice, serviceId, serviceName, localPort))
    {
        return libjami::openServiceTunnel(accountId, peerUri, peerDevice, serviceId, serviceName, localPort);
    }

    auto closeServiceTunnel(const std::string& accountId,
                            const std::string& tunnelId) -> decltype(libjami::closeServiceTunnel(accountId, tunnelId))
    {
        return libjami::closeServiceTunnel(accountId, tunnelId);
    }

    auto getActiveTunnels(const std::string& accountId) -> decltype(libjami::getActiveTunnels(accountId))
    {
        return libjami::getActiveTunnels(accountId);
    }

private:
    void registerSignalHandlers()
    {
        using namespace std::placeholders;

        using libjami::exportable_serialized_callback;
        using libjami::ServiceSignal;
        using SharedCallback = std::shared_ptr<libjami::CallbackWrapperBase>;

        const std::map<std::string, SharedCallback> serviceEvHandlers = {
            exportable_serialized_callback<ServiceSignal::PeerServicesReceived>(
                std::bind(&DBusNetworkServiceManager::emitPeerServicesReceived, this, _1, _2, _3, _4, _5)),
            exportable_serialized_callback<ServiceSignal::TunnelOpened>(
                std::bind(&DBusNetworkServiceManager::emitServiceTunnelOpened, this, _1, _2, _3)),
            exportable_serialized_callback<ServiceSignal::TunnelClosed>(
                std::bind(&DBusNetworkServiceManager::emitServiceTunnelClosed, this, _1, _2, _3)),
        };

        libjami::registerSignalHandlers(serviceEvHandlers);
    }
};
