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

#include "dring/datatransfer_interface.h"
#include "ip_utils.h"

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <functional>

namespace ring {

class Account;
class TurnTransport;

//==============================================================================

class Stream
{
public:
    virtual ~Stream() { close(); }
    virtual void close() noexcept { }
    virtual DRing::DataTransferId getId() const = 0;
    virtual bool read(std::vector<char>& buffer) const {
        (void)buffer;
        return false;
    }
    virtual bool write(const std::vector<char>& buffer) {
        (void)buffer;
        return false;
    };
};

//==============================================================================

class ConnectionEndpoint
{
public:
    virtual ~ConnectionEndpoint() { close(); }
    virtual void close() {}
    virtual void read(std::vector<char>& buffer) const = 0;
    virtual void write(const std::vector<char>& buffer) = 0;
};

//==============================================================================

class TcpTurnEndpoint : public ConnectionEndpoint
{
public:
    TcpTurnEndpoint(TurnTransport& turn, const IpAddr& peer);
    ~TcpTurnEndpoint();

    void read(std::vector<char>& buffer) const override;

    void write(const std::vector<char>& buffer) override;

private:
    const IpAddr peer_;
    TurnTransport& turn_;
};

//==============================================================================

class TcpSocketEndpoint : public ConnectionEndpoint
{
public:
    TcpSocketEndpoint(const IpAddr& addr);

    ~TcpSocketEndpoint();

    void close() noexcept override;

    void read(std::vector<char>& buffer) const override;
    std::vector<char> readline() const;
    char readchar() const;

    void write(const std::vector<char>& buffer) override;
    void writeline(const char* const buffer, std::size_t length);
    void writeline(const std::vector<char>& buffer);

    void connect();

private:
    const IpAddr addr_;
    int sock_ {-1};
};

//==============================================================================

class PeerConnection
{
public:
    PeerConnection(Account& account, const std::string& peer_uri,
                   std::unique_ptr<ConnectionEndpoint> endpoint);

    ~PeerConnection();

    void close();

    void attachOutputStream(const std::shared_ptr<Stream>& stream);

    void attachInputStream(const std::shared_ptr<Stream>& stream);

    void refuseStream(const DRing::DataTransferId& id);

    void abortStream(const DRing::DataTransferId& id);

private:
    class PeerConnectionImpl;
    std::unique_ptr<PeerConnectionImpl> pimpl_;
};

} // namespace ring
