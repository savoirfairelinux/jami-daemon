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
#include "sip/sipaccountbase.h"

#include <fstream>

namespace ring {
namespace InstantMessaging {

static std::uniform_int_distribution<uint64_t> udist;

MessageEngine::MessageEngine(SIPAccountBase& acc, const std::string& path) : account_(acc), savePath_(path)
{
    load();
}

MessageEngine::MessageToken
MessageEngine::sendMessage(const std::string& to, const std::map<std::string, std::string>& payloads)
{
    if (payloads.empty() or to.empty())
        return 0;
    auto token = udist(account_.rand_);
    auto m = messages_.emplace(token, Message{});
    m.first->second.to = to;
    m.first->second.payloads = payloads;
    trySend(m.first);
    //save();
    return token;
}

MessageEngine::MessageStatus
MessageEngine::getStatus(MessageToken t) const
{
    const auto m = messages_.find(t);
    return (m == messages_.end()) ? MessageStatus::UNKNOWN : m->second.status;
}

void
MessageEngine::trySend(decltype(MessageEngine::messages_)::iterator m)
{
    auto token = m->first;
    m->second.status = MessageStatus::SENDING;
    m->second.retried++;
    m->second.last_op = std::chrono::system_clock::now();
    account_.sendTextMessage(m->second.to, m->second.payloads, [=](bool ok) {
        RING_WARN("sendTextMessage callback %d", ok);
        auto f = messages_.find(token);
        if (f != messages_.end()) {
            if (f->second.status == MessageStatus::SENDING) {
                if (ok) {
                    f->second.status = MessageStatus::SENT;
                } else if (f->second.retried == MAX_RETRIES) {
                    f->second.status = MessageStatus::ERROR;
                } else {
                }
            }
        }
    });
}

void
MessageEngine::load()
{
    std::ifstream file(savePath_);
    if (!file.is_open()) {
        RING_WARN("Could not load %s", savePath_.c_str());
        return;
    }
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        MessageToken token;
        Message msg;
        time_t t;
        int status;
        std::string txt;
        if (!(iss >> std::hex >> token >> status >> msg.retried >> msg.to >> t >> txt)) { break; }
        msg.status = (MessageStatus)status;
        msg.last_op = std::chrono::system_clock::from_time_t(t);
        msg.payloads["text/plain"] = std::move(txt);
        messages_.emplace(token, std::move(msg));
    }
}

void
MessageEngine::save() const
{
    std::ofstream file(savePath_, std::ios::trunc);
    if (!file.is_open()) {
        RING_ERR("Could not save to %s", savePath_.c_str());
        return;
    }
    for (auto& c : messages_) {
        auto& v = c.second;
        file << std::hex << c.first
             << ' ' << (int)v.status
             << ' ' << v.retried
             << ' ' << v.to
             << ' ' << std::chrono::system_clock::to_time_t(v.last_op)
             << ' ' << v.payloads.cbegin()->second << '\n';
    }
}

}}
