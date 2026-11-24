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
#include "swarm_certificate.h"

namespace jami {

std::optional<DeviceIdentity>
readDeviceIdentity(git_repository* repo,
                   const git_tree* tree,
                   std::string_view deviceId,
                   std::string_view memberId,
                   std::initializer_list<std::string_view> memberDirs,
                   size_t directoryLimit,
                   size_t certificateLimit)
{
    try {
        DeviceIdentity identity;
        identity.deviceId = deviceId;
        auto deviceData
            = git::readBlob(repo, tree, "devices", identity.deviceId + ".crt", directoryLimit, certificateLimit);
        if (!deviceData)
            return std::nullopt;
        identity.deviceData = std::move(*deviceData);
        identity.device = std::make_shared<dht::crypto::Certificate>(identity.deviceData);
        if (!isCertificateOfDevice(*identity.device, deviceId))
            return std::nullopt;
        identity.memberId = memberId;
        if (identity.memberId.empty())
            identity.memberId = identity.device->getIssuerUID();
        if (identity.memberId.empty() && identity.device->issuer)
            identity.memberId = identity.device->issuer->getId().toString();
        if (identity.memberId.empty())
            return std::nullopt;
        for (const auto directory : memberDirs) {
            auto memberData = git::readBlob(repo,
                                            tree,
                                            std::string(directory),
                                            identity.memberId + ".crt",
                                            directoryLimit,
                                            certificateLimit);
            if (!memberData)
                continue;
            identity.memberData = std::move(*memberData);
            identity.member = std::make_shared<dht::crypto::Certificate>(identity.memberData);
            if (!isDeviceOfMember(*identity.device, *identity.member, identity.memberId))
                return std::nullopt;
            identity.device->issuer = identity.member;
            return identity;
        }
    } catch (const std::exception&) {
    }
    return std::nullopt;
}

} // namespace jami
