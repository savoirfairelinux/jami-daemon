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

/* Dring */
#include "logger.h"
#include "jami/jami.h"

/* std */
#include <condition_variable>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#define AGENT_ERR(FMT, ARGS...)  JAMI_ERR("AGENT: " FMT, ##ARGS)
#define AGENT_INFO(FMT, ARGS...) JAMI_INFO("AGENT: " FMT, ##ARGS)
#define AGENT_DBG(FMT, ARGS...)  JAMI_DBG("AGENT: " FMT, ##ARGS)
#define AGENT_ASSERT(COND, MSG, ARGS...) \
    if (not(COND)) { \
        AGENT_ERR(MSG, ##ARGS); \
        exit(1); \
    }

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

            Agent::instance().notify();

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

    Handler<const std::string&, const std::string&, std::map<std::string, std::string>>
        onConversationRequestReceived_;

    Handler<const std::string&, const std::string&> onConversationReady_;

    Handler<const std::string&, const std::string&, signed> onCallStateChanged_;

    Handler<const std::string&,
            const std::string&,
            const std::string&,
            const std::vector<DRing::MediaMap>>
        onIncomingCall_;

    Handler<const std::string&, const std::string&, bool> onContactAdded_;

    Handler<const std::string&, const std::string&, int, const std::string&>
        onRegistrationStateChanged_;

    Handler<const std::string&, const std::map<std::string, std::string>&>
    onVolatileDetailsChanged_;

    /*  Initialize agent */
    void installSignalHandlers();
    void registerStaticCallbacks();

    /* Bookkeeping */
    std::string peerID_;
    const std::string accountID_ {"afafafafafafafaf"};
    std::vector<std::string> conversations_;
    std::condition_variable eventsCV_;

public:

    /* Events */
    void notify() {
        eventsCV_.notify_one();
    }

    /* Behavior */
    bool ping(const std::string& conversation);
    bool placeCall(const std::string& contact);
    std::string someContact() const;
    std::string someConversation() const;
    void setDetails(const std::map<std::string, std::string>& details);
    std::map<std::string, std::string> getDetails() const;
    void stopRecording(const std::string& context);
    void startRecording(const std::string& context, const std::string& to);
    void searchForPeers(std::vector<std::string>& peers);
    void wait(std::chrono::seconds period);
    void exportToArchive(const std::string& path);
    void importFromArchive(const std::string& path);
    void ensureAccount();
    void waitForAnnouncement(std::chrono::seconds timeout=std::chrono::seconds(30));
    void activate(bool state);
    void waitForEvent();

    void init();
    void fini();

    static Agent& instance();
};
