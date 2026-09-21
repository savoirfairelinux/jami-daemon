/*
 * Copyright (C) 2026 Savoir-faire Linux Inc.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <json/json.h>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace jami {

class JamiAccount;

class FeedModule : public std::enable_shared_from_this<FeedModule>
{
public:
    using Info = std::map<std::string, std::string>;
    explicit FeedModule(const std::shared_ptr<JamiAccount>& account);
    std::string create(const std::string& title, const std::string& avatar, bool replies);
    bool update(const std::string& id, const Info& changes);
    bool setAccess(const std::string& id, const std::string& uri, bool allowed);
    bool subscribe(const std::string& id, bool subscribed, bool propagate = true);
    std::vector<Info> list() const;
    void refresh();
    void resumeSubscriptions();
    void onMessage(const std::string& from, const Json::Value& message);
    bool wantsSubscription(const std::string& id, const std::string& owner) const;
    void onConversationReady(const std::string& id);
    void onProfileChanged(const std::string& id);
    std::map<std::string, Info> syncData() const;
    void onSyncData(const std::map<std::string, Info>& state);
    bool subscriptionCancelled(const std::string& id) const;

private:
    void send(const std::string& to, const Json::Value& message);
    void query(const std::string& owner);
    void syncToDevices();
    void offerCatalogue(const std::string& to, const std::string& query);
    void receiveCatalogue(const std::string& from, const Json::Value& message);
    void announceChange(const std::string& id, const std::set<std::string>& previous);
    bool saveLocked() const;
    void changed() const;
    void error(const std::string& id, const std::string& message) const;

    std::weak_ptr<JamiAccount> account_;
    std::string accountId_;
    mutable std::mutex mutex_;
    std::map<std::string, Info> offers_;
    std::set<std::string> requested_;
    std::map<std::string, std::string> queries_;
};

} // namespace jami
