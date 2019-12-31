/*
 *  Copyright (C) 2019 Savoir-faire Linux Inc.
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

#include <memory>
#include <string>

#include "def.h"

namespace jami {

class JamiAccount;
class ChannelSocket;

/**
 * This class gives access to the git repository that represents the conversation
 */
class DRING_TESTABLE ConversationRepository {
public:
    /**
     * Creates a new repository, with initial files, where the first commit hash is the conversation id
     * @param account       The related account
     * @return  the conversation repository object
     */
    static DRING_TESTABLE std::unique_ptr<ConversationRepository> createConversation(
        const std::weak_ptr<JamiAccount>& account
    );

    /**
     * Clones a conversation on a remote device
     * @note This will use the socket registered for the conversation with JamiAccount::addGitSocket()
     * @param account           The account getting the conversation
     * @param deviceId          Remote device
     * @param conversationId    Conversation to clone
     * @param socket            Socket used to clone
     */
    static DRING_TESTABLE std::unique_ptr<ConversationRepository> cloneConversation(
        const std::weak_ptr<JamiAccount>& account,
        const std::string& deviceId,
        const std::string& conversationId
    );

    /**
     * Open a conversation repository for an account and an id
     * @param account       The related account
     * @param id            The conversation id
     */
    ConversationRepository(const std::weak_ptr<JamiAccount>& account, const std::string& id);
    ~ConversationRepository();

    /**
     * Fetch a remote repository via the given socket
     * @note This will use the socket registered for the conversation with JamiAccount::addGitSocket()
     * @note will create a remote identified by the deviceId
     * @param remoteDeviceId    Remote device id to fetch
     * @return if the operation was successful
     */
    bool fetch(const std::string& remoteDeviceId);

    /**
     * Retrieve remote head. Can be useful after a fetch operation
     * @param remoteDeviceId        The remote name
     * @param branch                Remote branch to check (default: master)
     * @return the commit id pointed
     */
    std::string remoteHead(const std::string& remoteDeviceId, const std::string& branch = "master");

    /**
     * Return the conversation id
     */
    std::string id() const;

    /**
     * Add a new commit to the conversation
     * @param msg     The msg to send
     * @return <empty> on failure, else the message id
     */
    std::string sendMessage(const std::string& msg);

private:
    ConversationRepository() = delete;
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

}