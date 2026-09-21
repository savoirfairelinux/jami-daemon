/*
 * Copyright (C) 2026 Savoir-faire Linux Inc.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "feed_module.h"
#include "feed_policy.h"
#include "sync_module.h"
#include "jamiaccount.h"
#include "conversation_module.h"
#include "conversationrepository.h"
#include "account_manager.h"
#include "client/jami_signal.h"
#include "fileutils.h"
#include "logger.h"
#include "string_utils.h"

#include <opendht/thread_pool.h>
#include <filesystem>
#include <random>
#include <charconv>

namespace jami {

namespace {
constexpr size_t MAX_CATALOGUE_SIZE = 1024 * 1024;

uint64_t
versionOf(const FeedModule::Info& info, const char* key)
{
    const auto it = info.find(key);
    if (it == info.end())
        return 0;
    uint64_t value {};
    const auto result = std::from_chars(it->second.data(), it->second.data() + it->second.size(), value);
    return result.ec == std::errc {} && result.ptr == it->second.data() + it->second.size() ? value : 0;
}

void
advanceVersion(FeedModule::Info& info, const char* key)
{
    info[key] = std::to_string(std::max(uint64_t(toMillisecondsSinceEpoch(nowMs())), versionOf(info, key) + 1));
}

void
replaceDescriptor(FeedModule::Info& target, FeedModule::Info incoming)
{
    for (const auto* key : {"subscriptionVersion", "subscriptionWanted"})
        if (auto it = target.find(key); it != target.end())
            incoming[key] = it->second;
    const auto previous = versionOf(target, "catalogueVersion");
    incoming["catalogueVersion"] = std::to_string(previous);
    advanceVersion(incoming, "catalogueVersion");
    target = std::move(incoming);
}

void
cancelIntent(FeedModule::Info& info)
{
    if (info["subscriptionWanted"] != "false") {
        info["subscriptionWanted"] = "false";
        advanceVersion(info, "subscriptionVersion");
    }
}

std::set<std::string>
accessOf(const FeedModule::Info& info)
{
    const auto policy = FeedPolicy::fromInfos(info);
    return policy ? policy->authorized : std::set<std::string> {};
}

FeedModule::Info
descriptor(FeedModule::Info info, const std::string& id)
{
    info["id"] = id;
    info.erase("feedAccess");
    info.erase("description");
    return info;
}
} // namespace

FeedModule::FeedModule(const std::shared_ptr<JamiAccount>& account)
    : account_(account)
    , accountId_(account->getAccountID())
{
    const auto path = account->getPath() / "feed-catalogue";
    if (!std::filesystem::exists(path))
        return;
    try {
        const auto bytes = fileutils::loadFile(path);
        auto object = msgpack::unpack(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        object.get().convert(offers_);
        for (const auto& [id, info] : offers_)
            if (auto it = info.find("requested"); it != info.end() && it->second == "true")
                requested_.insert(id);
    } catch (const std::exception& e) {
        JAMI_ERROR("[Account {}] Cannot read Feed catalogue: {}", accountId_, e.what());
    }
}

bool
FeedModule::saveLocked() const
{
    auto account = account_.lock();
    if (!account)
        return false;
    try {
        auto saved = offers_;
        for (auto& [id, info] : saved)
            info["requested"] = requested_.count(id) ? "true" : "false";
        msgpack::sbuffer buffer;
        msgpack::pack(buffer, saved);
        const auto path = account->getPath() / "feed-catalogue";
        auto temporary = path;
        temporary += ".tmp";
        fileutils::saveFile(temporary, reinterpret_cast<const uint8_t*>(buffer.data()), buffer.size(), 0600);
        std::filesystem::rename(temporary, path);
        return true;
    } catch (const std::exception& e) {
        JAMI_ERROR("[Account {}] Cannot save Feed catalogue: {}", accountId_, e.what());
        return false;
    }
}

void
FeedModule::changed() const
{
    emitSignal<libjami::ConversationSignal::FeedsChanged>(accountId_);
}

std::map<std::string, FeedModule::Info>
FeedModule::syncData() const
{
    std::lock_guard lk(mutex_);
    return offers_;
}

void
FeedModule::syncToDevices()
{
    if (auto account = account_.lock())
        account->convModule()->notifyFeedStateChanged();
}

void
FeedModule::onSyncData(const std::map<std::string, Info>& state)
{
    if (state.size() > 4096) {
        error({}, "Synced Feed catalogue is too large");
        return;
    }
    std::vector<std::pair<std::string, bool>> choices;
    bool modified = false;
    {
        std::lock_guard lk(mutex_);
        for (const auto& [id, incoming] : state) {
            auto owner = incoming.find("feedOwner");
            if (!FeedPolicy::validUri(id) || owner == incoming.end() || !FeedPolicy::validUri(owner->second)
                || incoming.size() > 24 || std::any_of(incoming.begin(), incoming.end(), [](const auto& item) {
                       return item.first.size() > 64 || item.second.size() > 64 * 1024;
                   })) {
                error(id, "Invalid synced Feed descriptor");
                continue;
            }
            auto& local = offers_[id];
            if (local.count("feedOwner") && local.at("feedOwner") != owner->second) {
                error(id, "Conflicting synced Feed owner");
                continue;
            }
            const auto localChoice = versionOf(local, "subscriptionVersion");
            const auto remoteChoice = versionOf(incoming, "subscriptionVersion");
            auto wanted = incoming.find("subscriptionWanted");
            if (versionOf(incoming, "catalogueVersion") > versionOf(local, "catalogueVersion")) {
                auto copy = incoming;
                for (const auto* key : {"subscriptionVersion", "subscriptionWanted"})
                    if (auto it = local.find(key); it != local.end())
                        copy[key] = it->second;
                local = std::move(copy);
                modified = true;
            }
            const bool cancelledTie = remoteChoice != 0 && remoteChoice == localChoice && wanted != incoming.end()
                                      && wanted->second == "false" && local["subscriptionWanted"] == "true";
            if ((remoteChoice > localChoice || cancelledTie) && wanted != incoming.end()
                && (wanted->second == "true" || wanted->second == "false")) {
                local["subscriptionVersion"] = std::to_string(remoteChoice);
                local["subscriptionWanted"] = wanted->second;
                choices.emplace_back(id, wanted->second == "true");
                modified = true;
            }
        }
        if (modified && !saveLocked())
            error({}, "Unable to persist synced Feed catalogue");
    }
    for (const auto& [id, wanted] : choices)
        subscribe(id, wanted, false);
    if (modified) {
        changed();
        syncToDevices();
    }
}

void
FeedModule::error(const std::string& id, const std::string& message) const
{
    JAMI_WARNING("[Account {}] [Feed {}] {}", accountId_, id, message);
    dht::ThreadPool::io().run([account = accountId_, id, message] {
        emitSignal<libjami::ConversationSignal::OnConversationError>(account, id, EUNAUTHORIZED, message);
    });
}

void
FeedModule::send(const std::string& to, const Json::Value& message)
{
    auto account = account_.lock();
    if (!account)
        return;
    auto payload = json::toString(message);
    if (payload.size() > MAX_CATALOGUE_SIZE) {
        error({}, "Feed catalogue exceeds the message limit");
        return;
    }
    if (!account->sendTextMessage(to, "", {{MIME_TYPE_FEED, std::move(payload)}}))
        error({}, "Unable to queue Feed discovery message");
}

void
FeedModule::query(const std::string& owner)
{
    if (!FeedPolicy::validUri(owner))
        return;
    std::random_device random;
    const auto token = std::to_string(random()) + std::to_string(random());
    {
        std::lock_guard lk(mutex_);
        queries_[owner] = token;
    }
    Json::Value request;
    request["op"] = "list";
    request["query"] = token;
    send(owner, request);
}

std::vector<FeedModule::Info>
FeedModule::list() const
{
    auto account = account_.lock();
    auto* cm = account ? account->convModule() : nullptr;
    if (!cm)
        return {};
    std::map<std::string, Info> feeds;
    {
        std::lock_guard lk(mutex_);
        feeds = offers_;
        for (auto& [id, info] : feeds) {
            info["subscribed"] = "false";
            info["owned"] = "false";
            info["requested"] = requested_.count(id) ? "true" : "false";
        }
    }
    for (const auto& id : cm->getConversations()) {
        auto conversation = cm->getConversation(id);
        if (!conversation || conversation->mode() != ConversationMode::FEED || conversation->isRemoving())
            continue;
        auto info = conversation->infos();
        const bool owned = info["feedOwner"] == account->getUsername();
        info["id"] = id;
        info["owned"] = owned ? "true" : "false";
        info["subscribed"] = "true";
        info["requested"] = "false";
        const auto policy = FeedPolicy::fromInfos(info);
        info["available"] = policy && !policy->closed && (owned || policy->authorized.count(account->getUsername()))
                                ? "true"
                                : "false";
        if (!owned)
            info.erase("feedAccess");
        feeds[id] = std::move(info);
    }
    std::vector<Info> result;
    for (auto& [id, info] : feeds)
        result.push_back(std::move(info));
    return result;
}

std::string
FeedModule::create(const std::string& title, const std::string& avatar, bool replies)
{
    if (title.empty() || title.size() > 256 || title.find_first_of("\r\n") != std::string::npos
        || avatar.size() > 64 * 1024 || avatar.find_first_of("\r\n") != std::string::npos) {
        error({}, "Invalid Feed name or avatar");
        return {};
    }
    auto account = account_.lock();
    auto* cm = account ? account->convModule() : nullptr;
    if (!cm)
        return {};
    auto id = cm->startConversation(ConversationMode::FEED);
    if (id.empty())
        return {};
    update(id, {{"title", title}, {"avatar", avatar}, {"feedReplies", replies ? "true" : "false"}});
    return id;
}

bool
FeedModule::update(const std::string& id, const Info& changes)
{
    auto account = account_.lock();
    auto* cm = account ? account->convModule() : nullptr;
    auto conv = cm ? cm->getConversation(id) : nullptr;
    if (!conv || conv->mode() != ConversationMode::FEED || conv->getInitialMembers().front() != account->getUsername()) {
        error(id, "Only the Feed owner can change its settings");
        return false;
    }
    for (const auto& [key, value] : changes) {
        if (key != "title" && key != "avatar" && key != "feedReplies" && key != "feedClosed" && key != "feedAdd"
            && key != "feedRemove") {
            error(id, "Unknown Feed setting");
            return false;
        }
        if (value.find_first_of("\r\n") != std::string::npos || (key == "avatar" && value.size() > 64 * 1024)) {
            error(id, "Invalid Feed setting value");
            return false;
        }
    }
    auto previous = accessOf(conv->infos());
    for (const auto* operation : {"feedAdd", "feedRemove"})
        if (auto it = changes.find(operation); it != changes.end())
            previous.insert(it->second);
    conv->updateInfos(changes, [w = weak_from_this(), id, previous](bool ok, const std::string& commit) {
        if (auto self = w.lock()) {
            if (!ok) {
                self->error(id, "Unable to update Feed settings");
                return;
            }
            if (auto account = self->account_.lock())
                account->convModule()->notifyFeedUpdate(id, commit);
            self->announceChange(id, previous);
        }
    });
    return true;
}

bool
FeedModule::setAccess(const std::string& id, const std::string& uri, bool allowed)
{
    auto account = account_.lock();
    if (!account || !FeedPolicy::validUri(uri) || uri == account->getUsername()) {
        error(id, "Invalid Feed contact");
        return false;
    }
    if (allowed) {
        const auto contact = account->accountManager()->getContactInfo(uri);
        if (!contact || !contact->isActive() || contact->isBanned() || !contact->confirmed) {
            error(id, "Feed access can only be granted to a confirmed contact");
            return false;
        }
    }
    return update(id, {{allowed ? "feedAdd" : "feedRemove", uri}});
}

void
FeedModule::announceChange(const std::string& id, const std::set<std::string>& previous)
{
    auto account = account_.lock();
    auto* cm = account ? account->convModule() : nullptr;
    auto conv = cm ? cm->getConversation(id) : nullptr;
    if (!conv)
        return;
    auto info = conv->infos();
    const auto current = accessOf(info);
    auto recipients = previous;
    recipients.insert(current.begin(), current.end());
    Json::Value notice;
    notice["op"] = "changed";
    for (const auto& peer : recipients)
        send(peer, notice);
    for (const auto& uri : previous)
        if (!current.count(uri))
            cm->removeConversationMember(id, dht::InfoHash(uri));
    changed();
}

void
FeedModule::refresh()
{
    auto account = account_.lock();
    if (!account)
        return;
    for (const auto& [uri, contact] : account->accountManager()->getContacts(false))
        if (contact.confirmed && contact.isActive() && !contact.isBanned())
            query(uri.toString());
    resumeSubscriptions();
}

void
FeedModule::resumeSubscriptions()
{
    std::vector<std::string> pending;
    {
        std::lock_guard lk(mutex_);
        pending.assign(requested_.begin(), requested_.end());
    }
    for (const auto& id : pending)
        subscribe(id, true, false);
}

void
FeedModule::offerCatalogue(const std::string& to, const std::string& queryId)
{
    auto account = account_.lock();
    auto* cm = account ? account->convModule() : nullptr;
    if (!cm)
        return;
    Json::Value response;
    response["op"] = "catalogue";
    response["query"] = queryId;
    response["feeds"] = Json::arrayValue;
    for (const auto& id : cm->getConversations()) {
        auto conv = cm->getConversation(id);
        if (!conv || conv->mode() != ConversationMode::FEED || conv->isRemoving())
            continue;
        auto info = conv->infos();
        const auto policy = FeedPolicy::fromInfos(info);
        if (info["feedOwner"] != account->getUsername() || !policy || !policy->authorized.count(to))
            continue;
        Json::Value item;
        for (const auto& [key, value] : descriptor(std::move(info), id))
            item[key] = value;
        response["feeds"].append(std::move(item));
    }
    send(to, response);
}

void
FeedModule::receiveCatalogue(const std::string& from, const Json::Value& message)
{
    if (!message["query"].isString() || !message["feeds"].isArray() || message["feeds"].size() > 1024) {
        error({}, "Malformed Feed catalogue");
        return;
    }
    std::map<std::string, Info> received;
    for (const auto& item : message["feeds"]) {
        if (!item.isObject() || !item["id"].isString() || !item["title"].isString() || !item["feedOwner"].isString()
            || item["feedOwner"].asString() != from
            || !ConversationRepository::isValidConversationId(item["id"].asString())) {
            error({}, "Invalid Feed descriptor");
            return;
        }
        Info info;
        for (const auto* field : {"id", "title", "avatar", "feedOwner", "feedReplies", "feedClosed"})
            if (item[field].isString())
                info[field] = item[field].asString();
        if (info["title"].size() > 256 || info["avatar"].size() > 64 * 1024 || !FeedPolicy::fromInfos(info)) {
            error({}, "Invalid Feed descriptor attributes");
            return;
        }
        info["mode"] = std::to_string(static_cast<int>(ConversationMode::FEED));
        info["available"] = info["feedClosed"] == "true" ? "false" : "true";
        received.emplace(info["id"], std::move(info));
    }
    std::vector<std::string> withdrawn;
    {
        std::lock_guard lk(mutex_);
        auto query = queries_.find(from);
        if (query == queries_.end() || query->second != message["query"].asString())
            return; // Superseded response or unsolicited catalogue.
        queries_.erase(query);
        for (auto& [id, info] : offers_) {
            if (info["feedOwner"] != from)
                continue;
            if (!received.count(id)) {
                info["available"] = "false";
                advanceVersion(info, "catalogueVersion");
                cancelIntent(info);
                requested_.erase(id);
                withdrawn.push_back(id);
            }
        }
        for (auto& [id, info] : received) {
            if (auto existing = offers_.find(id); existing != offers_.end() && existing->second["feedOwner"] != from) {
                error(id, "Conflicting Feed owner");
                continue;
            }
            if (info["available"] == "false") {
                requested_.erase(id);
                withdrawn.push_back(id);
                cancelIntent(offers_[id]);
            }
            replaceDescriptor(offers_[id], std::move(info));
        }
        if (!saveLocked())
            error({}, "Unable to persist Feed catalogue");
    }
    auto account = account_.lock();
    auto* cm = account ? account->convModule() : nullptr;
    for (const auto& id : withdrawn) {
        auto conv = cm ? cm->getConversation(id) : nullptr;
        if (cm && (!conv || (conv->mode() == ConversationMode::FEED && conv->infos()["feedOwner"] == from)))
            cm->withdrawFeed(id);
    }
    changed();
    syncToDevices();
}

bool
FeedModule::subscribe(const std::string& id, bool subscribed, bool propagate)
{
    auto account = account_.lock();
    auto* cm = account ? account->convModule() : nullptr;
    if (!cm || !ConversationRepository::isValidConversationId(id)) {
        error(id, "Invalid Feed subscription");
        return false;
    }
    if (!subscribed) {
        auto conv = cm->getConversation(id);
        if (conv && conv->mode() != ConversationMode::FEED) {
            error(id, "This conversation is not a Feed");
            return false;
        }
        if (!conv) {
            std::lock_guard lk(mutex_);
            if (!offers_.count(id)) {
                error(id, "Unknown Feed subscription");
                return false;
            }
        }
        if (conv && conv->mode() == ConversationMode::FEED) {
            auto info = conv->infos();
            if (info["feedOwner"] == account->getUsername()) {
                error(id, "The owner must close the Feed, not unsubscribe");
                return false;
            }
            std::lock_guard lk(mutex_);
            replaceDescriptor(offers_[id], descriptor(std::move(info), id));
            offers_[id]["available"] = "true";
        }
        {
            std::lock_guard lk(mutex_);
            requested_.erase(id);
            if (propagate) {
                offers_[id]["subscriptionWanted"] = "false";
                advanceVersion(offers_[id], "subscriptionVersion");
            }
            if (!saveLocked()) {
                error(id, "Unable to persist Feed subscription");
                return false;
            }
        }
        const auto infos = ConversationModule::convInfos(accountId_);
        if (auto it = infos.find(id); it != infos.end() && it->second.mode == ConversationMode::FEED)
            if (!cm->removeConversation(id))
                return false;
        changed();
        if (propagate)
            syncToDevices();
        return true;
    }
    if (auto conv = cm->getConversation(id)) {
        if (conv->mode() != ConversationMode::FEED) {
            error(id, "This conversation is not a Feed");
            return false;
        }
        if (!conv->isRemoving())
            return true;
        cm->withdrawFeed(id);
    }
    std::string owner;
    {
        std::lock_guard lk(mutex_);
        auto it = offers_.find(id);
        if (it == offers_.end() || it->second["available"] != "true") {
            error(id, "This Feed is not available for subscription");
            return false;
        }
        owner = it->second["feedOwner"];
        if (propagate) {
            it->second["subscriptionWanted"] = "true";
            advanceVersion(it->second, "subscriptionVersion");
        }
        requested_.insert(id);
        if (!saveLocked()) {
            requested_.erase(id);
            error(id, "Unable to persist Feed subscription");
            return false;
        }
    }
    Json::Value request;
    request["op"] = "subscribe";
    request["id"] = id;
    send(owner, request);
    changed();
    if (propagate)
        syncToDevices();
    return true;
}

bool
FeedModule::subscriptionCancelled(const std::string& id) const
{
    std::lock_guard lk(mutex_);
    auto it = offers_.find(id);
    if (it == offers_.end())
        return false;
    auto choice = it->second.find("subscriptionWanted");
    return choice != it->second.end() && choice->second == "false";
}

bool
FeedModule::wantsSubscription(const std::string& id, const std::string& owner) const
{
    std::lock_guard lk(mutex_);
    auto it = offers_.find(id);
    return requested_.count(id) && it != offers_.end() && it->second.at("feedOwner") == owner;
}

void
FeedModule::onConversationReady(const std::string& id)
{
    auto account = account_.lock();
    auto* cm = account ? account->convModule() : nullptr;
    auto conv = cm ? cm->getConversation(id) : nullptr;
    if (!conv || conv->mode() != ConversationMode::FEED)
        return;
    auto info = conv->infos();
    {
        std::lock_guard lk(mutex_);
        requested_.erase(id);
        replaceDescriptor(offers_[id], descriptor(std::move(info), id));
        offers_[id]["available"] = "true";
        if (!saveLocked())
            error(id, "Unable to persist Feed subscription");
    }
    changed();
    syncToDevices();
}

void
FeedModule::onProfileChanged(const std::string& id)
{
    auto account = account_.lock();
    auto* cm = account ? account->convModule() : nullptr;
    auto conv = cm ? cm->getConversation(id) : nullptr;
    if (!conv || conv->mode() != ConversationMode::FEED)
        return;
    auto info = conv->infos();
    const auto policy = FeedPolicy::fromInfos(info);
    const bool owner = info["feedOwner"] == account->getUsername();
    if (owner) {
        Json::Value notice;
        notice["op"] = "changed";
        for (const auto& member : conv->memberUris(account->getUsername(), {}))
            send(member, notice);
        if (policy)
            for (const auto& uri : policy->authorized)
                send(uri, notice);
    }
    if (!owner && policy && (policy->closed || !policy->authorized.count(account->getUsername()))) {
        {
            std::lock_guard lk(mutex_);
            replaceDescriptor(offers_[id], descriptor(std::move(info), id));
            offers_[id]["available"] = "false";
            cancelIntent(offers_[id]);
            requested_.erase(id);
            if (!saveLocked())
                error(id, "Unable to persist withdrawn Feed");
        }
        cm->withdrawFeed(id);
        syncToDevices();
    }
    changed();
}

void
FeedModule::onMessage(const std::string& from, const Json::Value& message)
{
    auto account = account_.lock();
    if (!account)
        return;
    if (!message.isObject() || !message["op"].isString()) {
        error({}, "Malformed Feed discovery message");
        return;
    }
    const auto contact = account->accountManager()->getContactInfo(from);
    if (!contact || !contact->isActive() || contact->isBanned() || !contact->confirmed) {
        error({}, "Feed discovery from an unauthorized contact");
        return;
    }
    const auto op = message["op"].asString();
    if (op == "list" && message["query"].isString() && message["query"].asString().size() <= 64) {
        offerCatalogue(from, message["query"].asString());
    } else if (op == "changed") {
        query(from);
    } else if (op == "catalogue") {
        receiveCatalogue(from, message);
    } else if (op == "subscribe" && message["id"].isString()) {
        const auto id = message["id"].asString();
        auto* cm = account->convModule();
        auto conv = ConversationRepository::isValidConversationId(id) ? cm->getConversation(id) : nullptr;
        const auto info = conv ? conv->infos() : Info {};
        const auto policy = FeedPolicy::fromInfos(info);
        if (!conv || conv->mode() != ConversationMode::FEED
            || conv->getInitialMembers().front() != account->getUsername() || !policy || policy->closed
            || !policy->authorized.count(from)) {
            Json::Value denied;
            denied["op"] = "denied";
            denied["id"] = id;
            send(from, denied);
            return;
        }
        cm->addConversationMember(id, dht::InfoHash(from));
    } else if (op == "denied" && message["id"].isString()) {
        const auto id = message["id"].asString();
        if (!wantsSubscription(id, from))
            return;
        {
            std::lock_guard lk(mutex_);
            requested_.erase(id);
            offers_[id]["available"] = "false";
            cancelIntent(offers_[id]);
            if (!saveLocked())
                error(id, "Unable to persist declined Feed subscription");
        }
        error(id, "The Feed owner no longer allows this subscription");
        changed();
    } else {
        error({}, "Unknown Feed discovery operation");
    }
}

} // namespace jami
