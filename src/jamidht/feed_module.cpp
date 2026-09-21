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
#include <random>

namespace jami {

namespace {
constexpr size_t MAX_CATALOGUE_SIZE = 1024 * 1024;

std::set<std::string>
accessOf(const FeedModule::Info& info)
{
    const auto policy = FeedPolicy::fromInfos(info);
    return policy ? policy->authorized : std::set<std::string> {};
}
} // namespace

FeedModule::Info
FeedModule::descriptor(Info info, const std::string& id)
{
    info["id"] = id;
    info.erase("feedAccess");
    info.erase("description");
    return info;
}

void
FeedModule::changed() const
{
    emitSignal<libjami::ConversationSignal::FeedsChanged>(accountId_);
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
        if (!conversation)
            continue;
        if (auto info = describe(id, *conversation, account->getUsername()))
            feeds[id] = std::move(*info);
    }
    std::vector<Info> result;
    for (auto& [id, info] : feeds)
        result.push_back(std::move(info));
    return result;
}

std::optional<FeedModule::Info>
FeedModule::describe(const std::string& id, Conversation& conversation, const std::string& accountUri) const
{
    if (conversation.mode() != ConversationMode::FEED || conversation.isRemoving())
        return {};
    auto info = conversation.infos();
    const bool owned = info["feedOwner"] == accountUri;
    const auto policy = FeedPolicy::fromInfos(info);
    const bool available = policy && !policy->closed && (owned || policy->authorized.count(accountUri));
    info["id"] = id;
    info["owned"] = owned ? "true" : "false";
    info["subscribed"] = "true";
    info["requested"] = "false";
    info["available"] = available ? "true" : "false";
    if (!owned)
        info.erase("feedAccess");
    return info;
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
    if (!conv || conv->mode() != ConversationMode::FEED) {
        error(id, "This conversation is not a Feed");
        return false;
    }
    const auto owner = ownerOf(id, *conv);
    if (!owner)
        return false;
    if (*owner != account->getUsername()) {
        error(id, "Only the Feed owner can change its settings");
        return false;
    }
    if (!validSettings(id, changes))
        return false;
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

std::optional<std::string>
FeedModule::ownerOf(const std::string& id, const Conversation& conversation) const
{
    auto owner = FeedPolicy::owner(conversation.getInitialMembers());
    if (!owner)
        error(id, "Invalid Feed ownership metadata");
    return owner;
}

bool
FeedModule::validSettings(const std::string& id, const Info& changes) const
{
    static const std::set<std::string_view> keys {"title",
                                                  "avatar",
                                                  "feedReplies",
                                                  "feedClosed",
                                                  "feedAdd",
                                                  "feedRemove"};
    for (const auto& [key, value] : changes) {
        if (!keys.count(key)) {
            error(id, "Unknown Feed setting");
            return false;
        }
        if (value.find_first_of("\r\n") != std::string::npos || (key == "avatar" && value.size() > 64 * 1024)) {
            error(id, "Invalid Feed setting value");
            return false;
        }
    }
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

bool
FeedModule::subscribe(const std::string& id, bool subscribed, bool propagate)
{
    auto account = account_.lock();
    auto* cm = account ? account->convModule() : nullptr;
    if (!cm || !ConversationRepository::isValidConversationId(id)) {
        error(id, "Invalid Feed subscription");
        return false;
    }
    return subscribed ? requestSubscription(id, propagate) : unsubscribe(id, propagate);
}

bool
FeedModule::unsubscribe(const std::string& id, bool propagate)
{
    auto account = account_.lock();
    if (!account)
        return false;
    auto* cm = account->convModule();
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

bool
FeedModule::requestSubscription(const std::string& id, bool propagate)
{
    auto account = account_.lock();
    if (!account)
        return false;
    auto* cm = account->convModule();
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
        onSubscribeRequest(from, message["id"].asString());
    } else if (op == "denied" && message["id"].isString()) {
        onSubscriptionDenied(from, message["id"].asString());
    } else {
        error({}, "Unknown Feed discovery operation");
    }
}

void
FeedModule::onSubscribeRequest(const std::string& from, const std::string& id)
{
    auto account = account_.lock();
    auto* cm = account ? account->convModule() : nullptr;
    auto conv = cm && ConversationRepository::isValidConversationId(id) ? cm->getConversation(id) : nullptr;
    if (conv && conv->mode() == ConversationMode::FEED) {
        const auto policy = FeedPolicy::fromInfos(conv->infos());
        const auto owner = ownerOf(id, *conv);
        if (owner && *owner == account->getUsername() && policy && !policy->closed && policy->authorized.count(from)) {
            cm->addConversationMember(id, dht::InfoHash(from));
            return;
        }
    }
    Json::Value denied;
    denied["op"] = "denied";
    denied["id"] = id;
    send(from, denied);
}

void
FeedModule::onSubscriptionDenied(const std::string& from, const std::string& id)
{
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
}

} // namespace jami
