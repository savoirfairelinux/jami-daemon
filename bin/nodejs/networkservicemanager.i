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

%header %{
#include "jami/jami.h"
#include "jami/networkservice_interface.h"

class NetworkServiceCallback {
public:
    virtual ~NetworkServiceCallback(){}
    virtual void peerServicesReceived(uint32_t /*requestId*/, const std::string& /*accountId*/, const std::string& /*peerId*/, int /*status*/, const std::string& /*servicesJson*/){}
    virtual void serviceTunnelOpened(const std::string& /*accountId*/, const std::string& /*tunnelId*/, uint16_t /*localPort*/){}
    virtual void serviceTunnelClosed(const std::string& /*accountId*/, const std::string& /*tunnelId*/, const std::string& /*reason*/){}
};
%}

%feature("director") NetworkServiceCallback;

namespace libjami {

/* Local-service exposure ("Network Services") */

std::string addExposedService(const std::string& accountId, const std::map<std::string, std::string>& details);
bool updateExposedService(const std::string& accountId, const std::map<std::string, std::string>& details);
bool removeExposedService(const std::string& accountId, const std::string& serviceId);
std::vector<std::map<std::string, std::string>> getExposedServices(const std::string& accountId);
uint32_t queryPeerServices(const std::string& accountId, const std::string& peerUri);
std::string openServiceTunnel(const std::string& accountId,
                              const std::string& peerUri,
                              const std::string& deviceId,
                              const std::string& serviceId,
                              const std::string& serviceName,
                              uint16_t localPort);
bool closeServiceTunnel(const std::string& accountId, const std::string& tunnelId);
std::vector<std::map<std::string, std::string>> getActiveTunnels(const std::string& accountId);

}

class NetworkServiceCallback {
public:
    virtual ~NetworkServiceCallback(){}
    virtual void peerServicesReceived(uint32_t /*requestId*/, const std::string& /*accountId*/, const std::string& /*peerId*/, int /*status*/, const std::string& /*servicesJson*/){}
    virtual void serviceTunnelOpened(const std::string& /*accountId*/, const std::string& /*tunnelId*/, uint16_t /*localPort*/){}
    virtual void serviceTunnelClosed(const std::string& /*accountId*/, const std::string& /*tunnelId*/, const std::string& /*reason*/){}
};
