/*
 *  Copyright (C) 2014-2019 Savoir-faire Linux Inc.
 *
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace jami {

class JamiAccount;
class ConversationRepository;

class Conversation {
public:
    Conversation(const std::weak_ptr<JamiAccount>& account, const std::string& conversationId="");
    Conversation(const std::weak_ptr<JamiAccount>& account, const std::string& remoteDevice, const std::string& conversationId);
    ~Conversation();

    std::string id() const;

    // Member management
    bool addMember(const std::string& contactUri);
    bool removeMember(const std::string& contactUri);
    /**
     * @return a vector of member details:
     * {
     *  "uri":"xxx",
     *  "role":"member/admin",
     *  "lastRead":"id"
     *  ...
     * }
     */
    std::vector<std::map<std::string, std::string>> getMembers();
    /**
     * Test if an URI is a member
     * @param uri       URI to test
     * @return true if uri is a member
     */
    bool isMember(const std::string& uri); // TODO test

    // Message send/load
    std::string sendMessage(const std::string& message, const std::string& type="text/plain", const std::string& parent="");
    std::vector<std::map<std::string, std::string>> loadMessages(const std::string& fromMessage = "", size_t n = 0);

    /**
     * Get new messages from peer
     * @param uri       the peer
     * @return if the operation was successful
     */
    bool fetchFrom(const std::string& uri); // TODO test

    /**
     * Analyze if merge is possible and merge history
     * @param uri       the peer
     * @return if the operation was successful
     */
    bool mergeHistory(const std::string& uri); // TODO test

private:
    std::weak_ptr<JamiAccount> account_;
    std::unique_ptr<ConversationRepository> repository_;
};

}
