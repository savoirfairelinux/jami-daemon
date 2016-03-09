/*
 *  Copyright (C) 2004-2016 Savoir-faire Linux Inc.
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
#ifndef ICE_SOCKET_H
#define ICE_SOCKET_H

#include <memory>
#include <functional>

namespace ring {

class IceTransport;
using IceRecvCb = std::function<ssize_t(unsigned char* buf, size_t len)>;

class IceSocket
{
    private:
        std::shared_ptr<IceTransport> ice_transport_ {};
        int compId_ = -1;

    public:
        IceSocket(std::shared_ptr<IceTransport> iceTransport, int compId)
            : ice_transport_(iceTransport), compId_(compId) {}

        void close();
        ssize_t recv(unsigned char* buf, size_t len);
        ssize_t send(const unsigned char* buf, size_t len);
        ssize_t getNextPacketSize() const;
        ssize_t waitForData(unsigned int timeout);
        void setOnRecv(IceRecvCb cb);
};

};

#endif /* ICE_SOCKET_H */
