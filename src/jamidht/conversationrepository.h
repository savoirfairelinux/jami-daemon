/*
/*
 *  Copyright (C) 2017-2019 Savoir-faire Linux Inc.
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

class DRING_TESTABLE ConversationRepository {
public:
    static DRING_TESTABLE std::unique_ptr<ConversationRepository> createConversation(const std::weak_ptr<JamiAccount>& account);

    ConversationRepository(const std::string& id);
    ~ConversationRepository();

    std::string id() const;

private:
    ConversationRepository() = delete;
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

}