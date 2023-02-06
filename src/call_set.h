/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */
#pragma once

#include "call.h"
#include "conference.h"

#include <map>
#include <memory>
#include <string>
#include <mutex>

namespace jami {

class CallSet
{
public:
    std::shared_ptr<Call> getCall(const std::string& callId) const
    {
        std::lock_guard<std::mutex> l(mutex_);
        auto i = calls_.find(callId);
        return i == calls_.end() ? std::shared_ptr<Call> {} : i->second.lock();
    }
    std::shared_ptr<Conference> getConference(const std::string& conferenceId) const
    {
        std::lock_guard<std::mutex> l(mutex_);
        auto i = conferences_.find(conferenceId);
        return i == conferences_.end() ? std::shared_ptr<Conference> {} : i->second;
    }

    void add(const std::shared_ptr<Call>& call)
    {
        std::lock_guard<std::mutex> l(mutex_);
        calls_.emplace(call->getCallId(), call);
    }
    void add(const std::shared_ptr<Conference>& conference)
    {
        std::lock_guard<std::mutex> l(mutex_);
        conferences_.emplace(conference->getConfId(), conference);
    }
    bool remove(const std::shared_ptr<Call>& call)
    {
        std::lock_guard<std::mutex> l(mutex_);
        return calls_.erase(call->getCallId()) > 0;
    }
    bool removeConference(const std::string& confId)
    {
        std::lock_guard<std::mutex> l(mutex_);
        return conferences_.erase(confId) > 0;
    }

    std::vector<std::string> getCallIds() const
    {
        std::lock_guard<std::mutex> l(mutex_);
        std::vector<std::string> ids;
        ids.reserve(calls_.size());
        for (const auto& callIt : calls_)
            ids.emplace_back(callIt.first);
        return ids;
    }
    std::vector<std::shared_ptr<Call>> getCalls() const
    {
        std::lock_guard<std::mutex> l(mutex_);
        std::vector<std::shared_ptr<Call>> calls;
        calls.reserve(calls_.size());
        for (const auto& callIt : calls_)
            if (auto call = callIt.second.lock())
                calls.emplace_back(std::move(call));
        return calls;
    }

    std::vector<std::string> getConferenceIds() const
    {
        std::lock_guard<std::mutex> l(mutex_);
        std::vector<std::string> ids;
        ids.reserve(conferences_.size());
        for (const auto& confIt : conferences_)
            ids.emplace_back(confIt.first);
        return ids;
    }
    std::vector<std::shared_ptr<Conference>> getConferences() const
    {
        std::lock_guard<std::mutex> l(mutex_);
        std::vector<std::shared_ptr<Conference>> confs;
        confs.reserve(conferences_.size());
        for (const auto& confIt : conferences_)
            if (const auto& conf = confIt.second)
                confs.emplace_back(conf);
        return confs;
    }

private:
    mutable std::mutex mutex_;
    std::map<std::string, std::weak_ptr<Call>> calls_;
    std::map<std::string, std::shared_ptr<Conference>> conferences_;
};

} // namespace jami
