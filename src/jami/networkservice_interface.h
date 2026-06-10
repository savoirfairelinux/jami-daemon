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

#include "def.h"

#include <vector>
#include <map>
#include <string>
#include <cstdint>

#include "jami.h"

namespace libjami {

LIBJAMI_PUBLIC std::string addExposedService(const std::string& accountId,
                                             const std::map<std::string, std::string>& details);
LIBJAMI_PUBLIC bool updateExposedService(const std::string& accountId,
                                         const std::map<std::string, std::string>& details);
LIBJAMI_PUBLIC bool removeExposedService(const std::string& accountId, const std::string& serviceId);
LIBJAMI_PUBLIC std::vector<std::map<std::string, std::string>> getExposedServices(const std::string& accountId);
LIBJAMI_PUBLIC uint32_t queryPeerServices(const std::string& accountId, const std::string& peerUri);
LIBJAMI_PUBLIC std::string openServiceTunnel(const std::string& accountId,
                                             const std::string& peerUri,
                                             const std::string& deviceId,
                                             const std::string& serviceId,
                                             const std::string& serviceName,
                                             uint16_t localPort);
LIBJAMI_PUBLIC bool closeServiceTunnel(const std::string& accountId, const std::string& tunnelId);
LIBJAMI_PUBLIC std::vector<std::map<std::string, std::string>> getActiveTunnels(const std::string& accountId);

// Service-exposure signal type definitions
struct LIBJAMI_PUBLIC ServiceSignal
{
    /**
     * Status code attached to PeerServicesReceived. Every queryPeerServices()
     * call is guaranteed to result in exactly one PeerServicesReceived signal,
     * with `status` indicating the outcome.
     */
    enum class PeerServicesStatus : int {
        OK = 0,            ///< Peer responded; servicesJson is the (possibly empty) list.
        NoDevices = 1,     ///< No devices announced for the peer (offline / unknown).
        Unreachable = 2,   ///< Devices were found but no channel could be opened.
        Timeout = 3,       ///< Channel opened but no response arrived within the deadline.
        InternalError = 4, ///< Local error (account stopping, handler missing, ...).
    };

    /**
     * Asynchronous response to queryPeerServices(). Emitted once per request id,
     * and additionally with requestId == 0 as an unsolicited push whenever the
     * peer's service list or device availability changes (cache update or
     * presence change); such a push carries the peer's full current list.
     * @param requestId   Token returned by queryPeerServices() — first arg.
     *                    0 for an unsolicited availability/cache update push.
     * @param accountId   Local account that issued the query.
     * @param peerId      URI of the peer that produced the response.
     * @param status      One of PeerServicesStatus, indicating success or
     *                    the failure mode.
     * @param servicesJson JSON array describing visible services. Each entry is
     *                    {"id":..,"name":..,"description":..,"proto":..,
     *                     "scheme":..,"device":..,"available":bool}. The
     *                    "available" flag is true when the advertising device is
     *                    currently online (presence system). Empty `[]` for any
     *                    non-OK status.
     */
    struct LIBJAMI_PUBLIC PeerServicesReceived
    {
        constexpr static const char* name = "PeerServicesReceived";
        using cb_type = void(uint32_t /*requestId*/,
                             const std::string& /*accountId*/,
                             const std::string& /*peerId*/,
                             int /*status*/,
                             const std::string& /*servicesJson*/);
    };
    struct LIBJAMI_PUBLIC TunnelOpened
    {
        constexpr static const char* name = "ServiceTunnelOpened";
        using cb_type = void(const std::string& /*accountId*/, const std::string& /*tunnelId*/, uint16_t /*localPort*/);
    };
    /**
     * Emitted when a tunnel created with openServiceTunnel() is torn down.
     * @param reason  Why the tunnel closed. Known tokens:
     *                - "closed"         : closed on request (closeServiceTunnel).
     *                - "connect-failed" : a local connection could not reach the
     *                                     remote service (peer device offline or
     *                                     unreachable); the tunnel was closed
     *                                     automatically.
     */
    struct LIBJAMI_PUBLIC TunnelClosed
    {
        constexpr static const char* name = "ServiceTunnelClosed";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& /*tunnelId*/,
                             const std::string& /*reason*/);
    };
};

} // namespace libjami
