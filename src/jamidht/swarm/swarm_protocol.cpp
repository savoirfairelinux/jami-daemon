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

#include "swarm_protocol.h"

namespace jami {
namespace swarm_protocol {

static constexpr std::string_view MOBILE_LEASE_SIGNATURE_DOMAIN {"DRT-MOBILE"};

dht::Blob
mobileLeasePayload(const MobileLease& lease)
{
    // The fixed-schema array makes field order and scalar encodings deterministic.
    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> packer(&buffer);
    packer.pack_array(7);
    packer.pack(MOBILE_LEASE_SIGNATURE_DOMAIN);
    packer.pack(lease.format_version);
    packer.pack(lease.conversation_id);
    packer.pack(lease.issuer_id);
    packer.pack(lease.device_id.toString());
    packer.pack(lease.issued_at);
    packer.pack(lease.expires_at);
    return {buffer.data(), buffer.data() + buffer.size()};
}

} // namespace swarm_protocol

} // namespace jami
