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

/* std */
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

/* DRing */
#include "account_const.h"
#include "dring/callmanager_interface.h"
#include "dring/configurationmanager_interface.h"
#include "dring/conversation_interface.h"

/* agent */
#include "agent/bt.h"

class Agent
{
    template<typename... Args>
    class Handler
    {
        std::mutex mutex_;
        std::vector<std::function<bool(Args...)>> callbacks_;

    public:
        void add(std::function<bool(Args...)>&& cb)
        {
            std::unique_lock<std::mutex> lck(mutex_);
            callbacks_.emplace_back(std::move(cb));
        }

        void execute(Args... args)
        {
            std::vector<std::function<bool(Args...)>> to_keep;
            std::unique_lock<std::mutex> lck(mutex_);

            for (auto& cb : callbacks_) {
                if (cb(args...)) {
                    to_keep.emplace_back(std::move(cb));
                }
            }

            callbacks_.swap(to_keep);
        }
    };

    /* Signal handlers */
    Handler<const std::string&, const std::string&, std::map<std::string, std::string>>
        onMessageReceived_;

    Handler<const std::string&,
            const std::string&,
            const std::string&,
            const std::map<std::string, std::string>&>
        onIncomingAccountMessage_;

    Handler<const std::string&, const std::string&, const std::string&, const std::vector<uint8_t>&, time_t>
        onIncomingTrustRequest_;

    Handler<const std::string&, const std::string&, signed> onCallStateChanged_;

    Handler<const std::string&,
            const std::string&,
            const std::string&,
            const std::vector<DRing::MediaMap>>
        onIncomingCall_;

    Handler<const std::string&, const std::string&, bool> onContactAdded_;

    Handler<const std::string&, const std::string&, bool> onContactRemoved_;

    /*  Initialize agent */
    void configure(const std::string& yaml_config);
    void createContacts(void);
    void ensureAccount(void);
    void initBehavior(void);
    void installSignalHandlers(void);
    void registerStaticCallbacks(void);

    /* Bookkeeping */
    std::string account_;
    std::set<std::string> contacts_;
    std::vector<std::string> peers_;
    std::unique_ptr<BT::Node> root_;
    std::vector<std::function<void(void)>> params_;

    /* Helper */
    void sendMessage(const std::string& to, const std::string& msg);

    /* Behavior */
    bool searchPeer(void);
    bool wait(void);
    bool echo(void);
    bool makeCall(void);
    bool False(void) { return false; }
    bool True(void) { return true; }

public:
    static void run(const std::string& yaml_config);
};
