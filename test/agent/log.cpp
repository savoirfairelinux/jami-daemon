/*
 *  Copyright (C) 2021 Savoir-faire Linux Inc.
 *
 *  Author: Olivier Dion <olivier.dion@savoirfairelinux.com>
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

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "agent/agent.h"
#include "agent/log.h"

FileHandler::FileHandler(const std::string& context, const std::string& to)
    : LogHandler(context)
    , out_(to, std::fstream::out | std::fstream::app)
{
    /* NO OP */
}

FileHandler::~FileHandler()
{
    flush();
}

void
FileHandler::pushMessage(const std::string& message)
{
    messages_.emplace_back(message);
}

void
FileHandler::flush()
{
    if (messages_.empty()) {
        return;
    }

    auto header = "CONTEXT: " + context_ + "\n";

    out_ << header;

    for (const auto& msg : messages_) {
        out_ << msg << '\n';
    }

    messages_.clear();
    out_.flush();
}

NetHandler::NetHandler(const std::string& context, const std::string& to, uint16_t port)
    : LogHandler(context)
{
    int domain, type, protocol;

    domain = AF_INET;
    type = SOCK_STREAM;
    protocol = 0;

    sockfd_ = socket(domain, type, protocol);

    AGENT_ASSERT(0 <= sockfd_, "Can't open socket - %m");

    struct sockaddr_in addr;

    memset(&addr, '\0', sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(to.c_str());
    addr.sin_port = htons(port);

    AGENT_ASSERT(0 == connect(sockfd_, (const sockaddr*) &addr, sizeof(addr)),
                 "Can't connect socket to %s:%u - %m",
                 to.c_str(),
                 port);
}

NetHandler::~NetHandler()
{
    flush();
    close(sockfd_);
}

void
NetHandler::pushMessage(const std::string& message)
{
    messages_.emplace_back(message + "\n");
}

void
NetHandler::flush()
{
    if (messages_.empty()) {
        return;
    }

    auto header = "CONTEXT: " + context_ + "\n";

    write(sockfd_, header.c_str(), header.size());

    for (const auto& msg : messages_) {
        write(sockfd_, msg.c_str(), msg.size());
    }

    messages_.clear();
}
