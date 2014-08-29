/*
 *  Copyright (C) 2004-2014 Savoir-Faire Linux Inc.
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */
#ifndef ICE_SOCKET_H
#define ICE_SOCKET_H

#include "logger.h"

#include <pjnath.h>
#include <ccrtp/rtp.h>

namespace sfl {

class ICESocket
{
    private:
        struct in_addr iaddr_{0};

    public:
        ICESocket(struct in_addr /*iaddr*/, int /*port*/) {
            // STUB
            ERROR("STUB!");
        }

        ICESocket() {
            // STUB
            ERROR("STUB!");
        }

        size_t waitForData(unsigned int /*timeout*/) {
            // STUB
            ERROR("STUB!");
            return -1;
        }

        struct in_addr getAddress() const {
            return iaddr_;
        }

        int connect(struct in_addr /*iaddr*/, int /*port*/) {
            // STUB
            ERROR("STUB!");
            return -1;
        }

        void close() {
            // STUB
            ERROR("STUB!");
        }

        size_t recv(unsigned char* /*buf*/, size_t /*len*/) {
            // STUB
            ERROR("STUB!");
            return -1;
        }

        size_t send(const unsigned char* /*buf*/, size_t /*len*/) {
            // STUB
            ERROR("STUB!");
            return -1;
        }

        size_t getNextPacketSize() {
            // STUB
            ERROR("STUB!");
            return -1;
        }
};

/* RTPOverICEChannel
 * A CCRTP Channel on top of our ICESocket
 */
class RTPOverICEChannel
{
    private:
        std::unique_ptr<ICESocket> sk_ = nullptr;

    public:
        RTPOverICEChannel(const ost::InetAddress& ia, ost::tpport_t port) {
            sk_.reset(new ICESocket(ia.getAddress(), port));
        }

        inline ~RTPOverICEChannel() {}

        inline bool
        isPendingRecv(ost::microtimeout_t timeout) {
            return sk_->waitForData(timeout) > 0;
        }

        inline ost::InetHostAddress
        getSender(ost::tpport_t& /*port*/) const {
            return ost::InetHostAddress(sk_->getAddress());
        }

        inline size_t
        recv(unsigned char* buffer, size_t len) {
            return sk_->recv(buffer, len);
        }

        inline size_t
        getNextPacketSize() const {
            return sk_->getNextPacketSize();
        }

        ost::Socket::Error
        setMulticast(bool /*enable*/) {
            ERROR("setMulticast() not implemented");
            return ost::Socket::errMulticastDisabled;
        }

        inline ost::Socket::Error
        join(const ost::InetMcastAddress& /*ia*/, uint32 /*iface*/) {
            ERROR("join() not implemented");
            return ost::Socket::errBindingFailed;
        }

        inline ost::Socket::Error
        drop(const ost::InetMcastAddress& /*ia*/) {
            ERROR("drop() not implemented");
            return ost::Socket::errBindingFailed;
        }

        inline ost::Socket::Error
        setTimeToLive(unsigned char /*ttl*/) {
            ERROR("setTimeToLive() not implemented");
            return ost::Socket::errServiceUnavailable;
        }

        RTPOverICEChannel() {
            sk_.reset(new ICESocket);
        }

        inline void
        setPeer(const ost::InetAddress &ia, ost::tpport_t port) {
            sk_->connect(ia.getAddress(), port);
        }

        inline size_t
        send(const unsigned char* const buffer, size_t len) {
            return sk_->send(buffer, len);
        }

        inline ost::SOCKET getRecvSocket() const {
            ERROR("getRecvSocket() not implemented");
            return -1;
        }

        // common
        inline void
        endSocket() {
            sk_->close();
        }
};

};

#endif /* ICE_SOCKET_H */
