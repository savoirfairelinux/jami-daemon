/*
 *  Copyright (C) 2019-2023 Savoir-faire Linux Inc.
 *  Author: Sébastien Blin <sebastien.blin@savoirfairelinux.com>
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
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <functional>
#include <memory>
#include <string>

#include "def.h"
#include "jamidht/conversationrepository.h"

namespace jami {

class ChannelSocket;

using onFetchedCb = std::function<void(const std::string&)>;

/**
 * This class offers to a ChannelSocket the possibility to interact with a Git repository
 */
class LIBJAMI_TESTABLE GitServer
{
public:
    /**
     * Serve a conversation to a remote client
     * This client will be able to fetch commits/clone the repository
     * @param accountId         Account related to the conversation
     * @param conversationId    Conversation's id
     * @param client            The client to serve
     */
    GitServer(const std::string& accountId,
              const std::string& conversationId,
              const std::shared_ptr<ChannelSocket>& client);
    ~GitServer();

    /**
     * Add a callback which will be triggered when the peer gets the data
     * @param cb
     */
    void setOnFetched(const onFetchedCb& cb);

    /**
     * Stopping a GitServer will shut the channel down
     */
    void stop();

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace jami