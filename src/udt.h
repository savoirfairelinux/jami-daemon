/*
 *  Copyright (C) 2015 Savoir-faire Linux Inc.
 *
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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

#pragma once

#include <cstdlib> // size_t
#include <cstdint> // uint*_t
#include <algorithm> // copy_n

namespace ring {
namespace udt {

class UDTPacket {
public:
    inline const uint8_t* payload() const noexcept {
        return rawData_ + 16; // DataPacket Header must be 128bits long
    }

protected:
    UDTPacket(const uint8_t* bytes, size_t lenght) noexcept;

    const size_t lenght_;
    const uint8_t* rawData_;

private:
    UDTPacket() = delete;
};

class DataPacket : public UDTPacket {
public:
    DataPacket(const uint8_t* bytes, size_t lenght) noexcept;

private:
    using Header = struct {
        // This first 128bits must exist in all data packet
        uint32_t seqNum;
        uint32_t msgNum;
        uint32_t timestamp;
        uint32_t destID;
    };
    const Header& header_;
};

class CtrlPacket : public UDTPacket {
public:
    CtrlPacket(const uint8_t* bytes, size_t lenght) noexcept;

private:
    using Header = struct {
        // This first 128bits must exist in all control packet
        uint32_t type;
        uint32_t addInfo;
        uint32_t timestamp;
        uint32_t destID;
    };
    const Header& header_;
};

// Return true if packet has UDT data packet signature
inline bool
isDataPacket(const uint8_t* bytes) {
    return (bytes[0] & 0x80) == 0;
}

template <typename T>
class AllocatedPacket : public T {
    AllocatedPacket(const uint8_t* bytes, size_t lenght) : T(bytes, lenght) {
        bytes_.resize(lenght);
        std::copy_n(bytes, lenght, bytes_.data());
    }

    inline const T& operator*() const noexcept {
        return static_cast<const T&>(*this);
    }

    inline const T& operator->() const noexcept {
        return static_cast<const T&>(*this);
    }

private:
    std::vector<uint8_t> bytes_;
};

template <typename T>
class Server {
public:
    using socket_type = T;

    Server(T& socket);

    T& getSocket() const noexcept {
        return socket_;
    }

    int listen(int max_connection);

private:
    T& socket_; // underlaying udp socket
};

} // namespace ring::udt
} // namespace ring
