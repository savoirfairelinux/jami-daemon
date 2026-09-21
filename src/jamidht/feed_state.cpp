/*
 * Copyright (C) 2026 Savoir-faire Linux Inc.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "feed_module.h"
#include "feed_policy.h"
#include "jamiaccount.h"
#include "conversation_module.h"
#include "conversationrepository.h"
#include "fileutils.h"
#include "logger.h"
#include "string_utils.h"

#include <charconv>
#include <filesystem>

namespace jami {
namespace {
bool
validSyncedDescriptor(const std::string& id, const FeedModule::Info& info)
{
    const auto owner = info.find("feedOwner");
    return FeedPolicy::validUri(id) && owner != info.end() && FeedPolicy::validUri(owner->second) && info.size() <= 24
           && std::none_of(info.begin(), info.end(), [](const auto& item) {
                  return item.first.size() > 64 || item.second.size() > 64 * 1024;
              });
}

std::optional<FeedModule::Info>
parseDescriptor(const std::string& from, const Json::Value& item)
{
    if (!item.isObject() || !item["id"].isString() || !item["title"].isString() || !item["feedOwner"].isString()
        || item["feedOwner"].asString() != from
        || !ConversationRepository::isValidConversationId(item["id"].asString()))
        return {};
    FeedModule::Info info;
    for (const auto* field : {"id", "title", "avatar", "feedOwner", "feedReplies", "feedClosed"})
        if (item[field].isString())
            info[field] = item[field].asString();
    if (info["title"].size() > 256 || info["avatar"].size() > 64 * 1024 || !FeedPolicy::fromInfos(info))
        return {};
    info["mode"] = std::to_string(static_cast<int>(ConversationMode::FEED));
    info["available"] = info["feedClosed"] == "true" ? "false" : "true";
    return info;
}

void
preserveSubscription(FeedModule::Info& target, const FeedModule::Info& source)
{
    for (const auto* key : {"subscriptionVersion", "subscriptionWanted"})
        if (auto it = source.find(key); it != source.end())
            target[key] = it->second;
}
} // namespace

uint64_t
FeedModule::versionOf(const Info& info, const char* key)
{
    const auto it = info.find(key);
    if (it == info.end())
        return 0;
    uint64_t value {};
    const auto result = std::from_chars(it->second.data(), it->second.data() + it->second.size(), value);
    return result.ec == std::errc {} && result.ptr == it->second.data() + it->second.size() ? value : 0;
}

void
FeedModule::advanceVersion(Info& info, const char* key)
{
    info[key] = std::to_string(std::max(uint64_t(toMillisecondsSinceEpoch(nowMs())), versionOf(info, key) + 1));
}

void
FeedModule::replaceDescriptor(Info& target, Info incoming)
{
    preserveSubscription(incoming, target);
    incoming["catalogueVersion"] = std::to_string(versionOf(target, "catalogueVersion"));
    advanceVersion(incoming, "catalogueVersion");
    target = std::move(incoming);
}

void
FeedModule::cancelIntent(Info& info)
{
    if (info["subscriptionWanted"] != "false") {
        info["subscriptionWanted"] = "false";
        advanceVersion(info, "subscriptionVersion");
    }
}

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

bool
FeedModule::mergeSyncedDescriptor(const std::string& id,
                                  const Info& incoming,
                                  std::vector<std::pair<std::string, bool>>& choices)
{
    if (!validSyncedDescriptor(id, incoming)) {
        error(id, "Invalid synced Feed descriptor");
        return false;
    }
    auto& local = offers_[id];
    if (local.count("feedOwner") && local.at("feedOwner") != incoming.at("feedOwner")) {
        error(id, "Conflicting synced Feed owner");
        return false;
    }
    const auto localChoice = versionOf(local, "subscriptionVersion");
    const auto remoteChoice = versionOf(incoming, "subscriptionVersion");
    bool modified = false;
    if (versionOf(incoming, "catalogueVersion") > versionOf(local, "catalogueVersion")) {
        auto copy = incoming;
        preserveSubscription(copy, local);
        local = std::move(copy);
        modified = true;
    }
    const auto wanted = incoming.find("subscriptionWanted");
    if (wanted == incoming.end() || (wanted->second != "true" && wanted->second != "false"))
        return modified;
    const bool cancelledTie = remoteChoice != 0 && remoteChoice == localChoice && wanted->second == "false"
                              && local["subscriptionWanted"] == "true";
    if (remoteChoice > localChoice || cancelledTie) {
        local["subscriptionVersion"] = std::to_string(remoteChoice);
        local["subscriptionWanted"] = wanted->second;
        choices.emplace_back(id, wanted->second == "true");
        modified = true;
    }
    return modified;
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
        for (const auto& [id, incoming] : state)
            modified = mergeSyncedDescriptor(id, incoming, choices) || modified;
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

std::vector<std::string>
FeedModule::applyCatalogue(const std::string& from, std::map<std::string, Info> received)
{
    std::vector<std::string> withdrawn;
    for (auto& [id, info] : offers_) {
        if (info["feedOwner"] != from || received.count(id))
            continue;
        info["available"] = "false";
        advanceVersion(info, "catalogueVersion");
        cancelIntent(info);
        requested_.erase(id);
        withdrawn.push_back(id);
    }
    for (auto& [id, info] : received) {
        const auto existing = offers_.find(id);
        if (existing != offers_.end() && existing->second["feedOwner"] != from) {
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
    return withdrawn;
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
        auto info = parseDescriptor(from, item);
        if (!info) {
            error({}, "Invalid Feed descriptor");
            return;
        }
        const auto id = info->at("id");
        received.emplace(id, std::move(*info));
    }
    std::vector<std::string> withdrawn;
    {
        std::lock_guard lk(mutex_);
        const auto query = queries_.find(from);
        if (query == queries_.end() || query->second != message["query"].asString())
            return; // Superseded response or unsolicited catalogue.
        queries_.erase(query);
        withdrawn = applyCatalogue(from, std::move(received));
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
} // namespace jami
