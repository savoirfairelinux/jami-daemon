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

#pragma once

#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

/* Jami */
#include "account.h"
#include "jamidht/jamiaccount.h"

class Agent
{
    template<typename... Args>
    class Handler
    {
        std::mutex mutex_;
        std::vector<std::function<bool(Args...)>> callbacks_;
    public:
            void emplace_back(std::function<bool(Args...)>&& cb) {
                    std::unique_lock<std::mutex> lck(mutex_);
                    callbacks_.emplace_back(std::move(cb));
            }

            void execute(Args... args) {


                std::vector<std::function<bool(Args...)>> todo, to_keep;

                {
                    std::unique_lock<std::mutex> lck(mutex_);
                    todo.swap(callbacks_);
                }

                for (auto& cb : todo) {
                    if (cb(args...)) {
                        to_keep.emplace_back(std::move(cb));
                    }
                }

                {
                    std::unique_lock<std::mutex> lck(mutex_);
                    for (const auto& cb : to_keep) {
                        callbacks_.emplace_back(std::move(cb));
                    }
                }
            }
    };

    std::string accountId_;
    std::shared_ptr<jami::JamiAccount> account_;
    std::set<std::string> contacts_;
    std::vector<std::string> peers_;

    /* Signal handlers */
    Handler<const std::string&,
            const std::string&,
            const std::string&,
            const std::map<std::string, std::string>&>
        incomingAccountMessage_;

    Handler<const std::string&, const std::string&, const std::string&, const std::vector<uint8_t>&, time_t>
    incomingTrustRequest_;

    Handler<const std::string&, const std::string&, signed>
    callStateChanged_;

    Handler<const std::string&,
            const std::string&,
            const std::string&,
            const std::vector<DRing::MediaMap>>
    incomingCall_;

    void onIncomingAccountMessage(std::function<bool(const std::string&,
                                                     const std::string&,
                                                     const std::string&,
                                                     const std::map<std::string, std::string>)>&& cb)
    {
        incomingAccountMessage_.emplace_back(std::move(cb));
    }

    void onIncomingTrustRequest(std::function<bool(const std::string&,
                                                   const std::string&,
                                                   const std::string&,
                                                   const std::vector<uint8_t>&,
                                                   time_t)>&& cb)
    {
        incomingTrustRequest_.emplace_back(std::move(cb));
    }

    void onCallStateChanged(std::function<bool(const std::string&, const std::string&, signed)>&& cb)
    {
        callStateChanged_.emplace_back(std::move(cb));
    }

    void onIncomingCall(std::function<bool(const std::string&,
                                           const std::string&,
                                           const std::string&,
                                           const std::vector<DRing::MediaMap>)>&& cb)
    {
        incomingCall_.emplace_back(std::move(cb));
    }

    /* Behavior */
    bool searchPeer(void);
    bool wait(void);
    bool pingPong(void);
    bool makeCall(void);
    bool end(void) { return false; }

public:
    static void run(const std::string& yaml_config);
};
