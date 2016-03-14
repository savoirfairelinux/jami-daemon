/*
 *  Copyright (C) 2004-2016 Savoir-faire Linux Inc.
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
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "message_engine.h"
#include "account.h"

namespace ring {
namespace InstantMessaging {

static std::uniform_int_distribution<dht::Value::Id> udist;

MessageEngine::MessageEngine(Account& acc, const std::string& path) : account_(acc), savePath_(path)
{
    load();
}

MessageEngine::MessageToken
MessageEngine::sendMessage(const std::string& to, const std::map<std::string, std::string>& payloads)
{
    auto token = udist(account_->rand_);
    messages_.emplace_back(token, Message{});
    save();
}

MessageEngine::MessageStatus
MessageEngine::getStatus(MessageToken t) const
{
    const auto m = messages_.find(t);
    return (m == messages_.end()) ? MessageStatus::UNKNOWN : m->second.status;
}

void
MessageEngine::load()
{

}

void
MessageEngine::save() const
{

}

}}
