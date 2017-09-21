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
#include <istream>
#include <ostream>

namespace ring
{

class Account;

class ConnectionEndpoint {
public:
    virtual ~ConnectionEndpoint() { close(); }
    virtual void close() {}
    virtual std::vector<char> read() = 0;
    virtual void write(const std::vector<char>&) = 0;
};

class PeerConnection {
public:
    PeerConnection(Account& account, const std::string& peer_uri,
                   std::unique_ptr<ConnectionEndpoint> endpoint);
    ~PeerConnection();

    void close();

    std::string newInputStream(std::unique_ptr<std::istream> stream);
    void newOutputStream(std::unique_ptr<std::ostream> stream, const std::string& id);

    void refuseStream(const std::string& stream);
    void abortStream(const std::string& stream);

private:
    class PeerConnectionImpl;
    std::unique_ptr<PeerConnectionImpl> pimpl_;
};

} // namespace ring
