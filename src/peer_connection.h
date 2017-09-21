/*
 *  Copyright (C) 2017 Savoir-faire Linux Inc.
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

#include <string>
#include <map>
#include <vector>
#include <memory>

namespace ring
{

class Account;

class ContentProvider {
public:
    virtual ~ContentProvider() = default;
    virtual std::map<std::string, std::string> meta() = 0;
    virtual std::vector<uint8_t> read() = 0;
};

class ContentConsumer {
public:
    virtual ~ContentConsumer() = default;
    virtual std::vector<uint8_t> read() = 0;
};

class ConnectionEndpoint {
public:
    virtual ~ConnectionEndpoint() { close(); }
    virtual void close() {}
    virtual std::vector<uint8_t> read() = 0;
    virtual void write(const std::vector<uint8_t>&) = 0;
};

class PeerConnection {
public:
    PeerConnection(Account& account, const std::string& peer_uri,
                   std::unique_ptr<ConnectionEndpoint> endpoint);
    ~PeerConnection();

    void close();

    std::string newStream(std::unique_ptr<ContentProvider> provider);

    void acceptStream(const std::string& stream, std::unique_ptr<ContentConsumer> consumer);
    void refuseStream(const std::string& stream);
    void abortStream(const std::string& stream);

private:
    class PeerConnectionImpl;
    std::unique_ptr<PeerConnectionImpl> pimpl_;
};

} // namespace ring
