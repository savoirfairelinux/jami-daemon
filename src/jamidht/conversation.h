/*
 *  Copyright (C) 2014-2019 Savoir-faire Linux Inc.
 *
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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

class Conversation {
public:
    // Member management
    void addMember(const std::string& contactUri);
    bool removeMember(const std::string& contactUri);
    std::vector<std::map<std::string, std::string>> getMembers();

    // Message send/load
    void sendMessage(const std::string& message, const std::string& parent);
    void loadMessages(const std::string& fromMessage, size_t n);

private:
    std::weak_ptr<JamiAccount> account_;
};

}
