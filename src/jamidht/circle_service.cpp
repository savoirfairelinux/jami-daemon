/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#include "circle_service.h"

#include "account_manager.h"
#include "atomic_file.h"
#include "circle_record.h"
#include "client/jami_signal.h"
#include "conversation_module.h"
#include "conversationrepository.h"
#include "jamiaccount.h"
#include "json_utils.h"
#include "logger.h"
#include "manager.h"
#include "sync_module.h"
#include "vcard.h"

#include <opendht/thread_pool.h>

#include <algorithm>
#include <limits>

namespace jami {
namespace {

constexpr size_t MAX_CATALOGUE_SIZE = 16 * 1024 * 1024;
constexpr size_t MAX_ANNOUNCEMENT_SIZE = 1024;
constexpr size_t UNCONNECTED_MEMBERS = 2;
constexpr std::chrono::hours EXPIRY_PERIOD {1};
constexpr const char* CATALOGUE_FILE = "circleInfo";
constexpr const char* REQUESTS_FILE = "circleRequests";

bool
isHex(std::string_view value, size_t size)
{
    return value.size() == size && std::all_of(value.begin(), value.end(), [](char character) {
               return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f');
           });
}

std::string
field(const Json::Value& json, const char* name)
{
    const auto& value = json[name];
    return value.isString() ? value.asString() : std::string();
}

int64_t
now()
{
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

// Mounted paths are canonical, while the account root may go through a symbolic link.
bool
mountedAt(const std::optional<std::filesystem::path>& mounted, const std::filesystem::path& expected)
{
    std::error_code error;
    return mounted && std::filesystem::equivalent(*mounted, expected, error);
}

// Who may learn a route's head: members who can read the circle, or the channel.
bool
mayRead(const CirclePolicy& policy, const CircleRoute& route, const std::string& member)
{
    const auto found = policy.members.find(member);
    if (found == policy.members.end())
        return false;
    if (route.circle)
        return found->second.state == CircleMemberState::ACTIVE || found->second.state == CircleMemberState::INVITED;
    return found->second.state == CircleMemberState::ACTIVE
           && policy.hasPermission(member, Permission::ReadChannel, route.id);
}

template<typename T>
void
load(const std::filesystem::path& path, T& value)
{
    if (const auto data = fileutils::readBounded(path, MAX_CATALOGUE_SIZE)) {
        try {
            msgpack::unpack(data->data(), data->size()).get().convert(value);
        } catch (const std::exception& e) {
            JAMI_WARNING("Unable to load {}: {}", path, e.what());
        }
    }
}

template<typename T>
void
store(const std::filesystem::path& path, const T& value)
{
    msgpack::sbuffer buffer;
    msgpack::pack(buffer, value);
    if (!fileutils::writeAtomic(path, std::string_view(buffer.data(), buffer.size())))
        JAMI_WARNING("Unable to save {}", path);
}

std::filesystem::path
stagingPath(const std::filesystem::path& directory)
{
    return directory / (".create-" + dht::InfoHash::getRandom().toString().substr(0, 16));
}

std::optional<CircleMemberState>
stateOf(const CirclePolicy& policy, const std::string& member)
{
    const auto found = policy.members.find(member);
    if (found == policy.members.end())
        return std::nullopt;
    return found->second.state;
}

// A channel entry as a conversation message, with the signing device; nothing for a merge.
SwarmHistory::Message
channelMessage(const CircleChannelEntry& entry)
{
    // Like the initial commit of a conversation, the genesis tells clients where the history starts.
    CommitMessage genesis;
    genesis.type = CommitType::INITIAL;
    auto fields = historyEntryMap(entry.id,
                                  entry.parents,
                                  entry.author,
                                  entry.timestamp,
                                  entry.message           ? &*entry.message
                                  : entry.parents.empty() ? &genesis
                                                          : nullptr);
    if (!fields || fields->at(CommitKey::TYPE) == CommitType::MERGE)
        return {};
    (*fields)[CommitKey::DEVICE] = entry.device;
    auto message = std::make_shared<libjami::SwarmMessage>();
    message->fromMapStringString(*fields);
    return message;
}

} // namespace

CircleService::CircleService(const std::shared_ptr<JamiAccount>& account, std::filesystem::path root)
    : account_(account)
    , accountId_(account->getAccountID())
    , root_(std::move(root))
{
    std::error_code error;
    std::filesystem::create_directories(root_, error);
    load(root_ / CATALOGUE_FILE, catalogue_);
    load(root_ / REQUESTS_FILE, requests_);
}

CircleService::~CircleService()
{
    if (expiryTimer_)
        expiryTimer_->cancel();
    for (const auto& [circle, membership] : swarms_) {
        membership->stopTracking();
        membership->manager()->shutdown();
    }
}

std::filesystem::path
CircleService::repositoryPath(std::string_view circle) const
{
    return root_ / circle / "repo";
}

std::filesystem::path
CircleService::channelPath(std::string_view circle, std::string_view channel) const
{
    return root_ / circle / "channels" / channel;
}

std::map<std::string, CircleInfo>
CircleService::circles() const
{
    std::lock_guard lock(mutex_);
    return catalogue_;
}

std::map<std::string, CircleRequest>
CircleService::requests() const
{
    std::lock_guard lock(mutex_);
    return requests_;
}

bool
CircleService::departed(const std::string& circle) const
{
    std::lock_guard lock(mutex_);
    const auto found = catalogue_.find(circle);
    return found == catalogue_.end() || found->second.departed;
}

std::shared_ptr<CircleRepository>
CircleService::repository(const std::string& circle)
{
    const auto circles = module();
    return circles ? circles->circle(circle) : nullptr;
}

void
CircleService::save() const
{
    store(root_ / CATALOGUE_FILE, catalogue_);
    store(root_ / REQUESTS_FILE, requests_);
}

std::shared_ptr<CircleModule>
CircleService::module()
{
    const auto account = account_.lock();
    const auto current = account ? account->circleModule() : nullptr;
    if (!current)
        return {};
    std::map<std::string, CircleInfo> catalogue;
    {
        std::lock_guard lock(mutex_);
        if (mounted_.lock() == current)
            return current;
        mounted_ = current;
        catalogue = catalogue_;
    }
    for (const auto& [circle, info] : catalogue) {
        if (!current->mountCircle(repositoryPath(circle), circle, info.departed != 0)) {
            JAMI_WARNING("[Account {}] Unable to mount circle {}", account->getAccountID(), circle);
            continue;
        }
        for (const auto& [channel, joined] : info.channels) {
            if (!current->mountChannel(channelPath(circle, channel), circle, channel))
                JAMI_WARNING("[Account {}] Unable to mount channel {} of circle {}",
                             account->getAccountID(),
                             channel,
                             circle);
        }
    }
    return current;
}

std::optional<CirclePolicy>
CircleService::policy(const std::string& circle)
{
    const auto mounted = repository(circle);
    if (!mounted)
        return std::nullopt;
    return mounted->policy();
}

std::vector<std::string>
CircleService::activeMembers(const std::string& circle)
{
    std::vector<std::string> members;
    if (const auto current = policy(circle)) {
        for (const auto& [member, entry] : current->members) {
            if (entry.state == CircleMemberState::ACTIVE)
                members.emplace_back(member);
        }
    }
    return members;
}

bool
CircleService::isActiveMember(const std::string& circle, const std::string& member)
{
    const auto current = policy(circle);
    if (!current)
        return false;
    const auto found = current->members.find(member);
    return found != current->members.end() && found->second.state == CircleMemberState::ACTIVE;
}

std::string
CircleService::circleOf(const CircleRoute& route) const
{
    std::lock_guard lock(mutex_);
    if (route.circle)
        return catalogue_.contains(route.id) ? route.id : std::string();
    for (const auto& [circle, info] : catalogue_) {
        if (info.channels.contains(route.id))
            return circle;
    }
    return {};
}

std::shared_ptr<SwarmMembership>
CircleService::swarm(const std::string& circle)
{
    const auto account = account_.lock();
    if (!account)
        return {};
    {
        std::lock_guard lock(mutex_);
        if (const auto found = swarms_.find(circle); found != swarms_.end())
            return found->second;
        if (!catalogue_.contains(circle))
            return {};
    }
    const auto route = CircleModule::circleRoute(circle);
    auto membership = std::make_shared<SwarmMembership>(account,
                                                        route,
                                                        root_ / circle / "mobileNodes",
                                                        [w = weak_from_this(), circle](const dht::InfoHash& issuer) {
                                                            const auto shared = w.lock();
                                                            return shared
                                                                   && shared->isActiveMember(circle, issuer.toString());
                                                        });
    membership->setMembers(
        [w = weak_from_this(), circle] {
            const auto shared = w.lock();
            return shared ? shared->activeMembers(circle) : std::vector<std::string>();
        },
        [w = weak_from_this(), circle](const std::string& member) {
            const auto shared = w.lock();
            return shared && shared->isActiveMember(circle, member);
        });
    membership->setOpenChannel(
        [w = weak_from_this(), route](const std::string& deviceId, SwarmMembership::ChannelCb&& cb, bool noNewSocket) {
            const auto shared = w.lock();
            const auto account = shared ? shared->account_.lock() : nullptr;
            if (!account) {
                cb(nullptr);
                return;
            }
            account->openSwarmChannel(
                route,
                deviceId,
                [w, route, cb = std::move(cb)](const std::shared_ptr<dhtnet::ChannelSocket>& socket) {
                    const auto shared = w.lock();
                    const auto cert = socket ? socket->peerCertificate() : nullptr;
                    if (socket && (!shared || !cert || !shared->authorizeSwarmChannel(route, *cert))) {
                        socket->shutdown();
                        return cb(nullptr);
                    }
                    return cb(socket);
                },
                noNewSocket);
        });
    std::lock_guard lock(mutex_);
    return swarms_.try_emplace(circle, std::move(membership)).first->second;
}

void
CircleService::bootstrapSwarm(const std::string& circle)
{
    const auto membership = swarm(circle);
    if (!membership)
        return;
    membership->monitor({});
    const auto& manager = membership->manager();
    if (manager->isShutdown()) {
        manager->restart();
        manager->maintainBuckets();
    } else if (!manager->isConnected()) {
        manager->maintainBuckets();
    }
}

void
CircleService::stopSwarm(const std::string& circle)
{
    std::shared_ptr<SwarmMembership> membership;
    {
        std::lock_guard lock(mutex_);
        if (const auto found = swarms_.find(circle); found != swarms_.end()) {
            membership = std::move(found->second);
            swarms_.erase(found);
        }
    }
    if (membership) {
        membership->stopTracking();
        membership->manager()->shutdown();
    }
}

void
CircleService::bootstrap()
{
    std::vector<std::string> circles;
    {
        std::lock_guard lock(mutex_);
        bootstrapped_ = true;
        for (const auto& [circle, info] : catalogue_)
            circles.emplace_back(circle);
    }
    removeExpired();
    module();
    for (const auto& circle : circles)
        bootstrapSwarm(circle);
    scheduleExpiry();
}

void
CircleService::shutdownConnections()
{
    std::vector<std::shared_ptr<SwarmMembership>> swarms;
    {
        std::lock_guard lock(mutex_);
        for (const auto& [circle, membership] : swarms_)
            swarms.emplace_back(membership);
    }
    for (const auto& membership : swarms)
        membership->manager()->shutdown();
}

void
CircleService::connectivityChanged()
{
    std::vector<std::shared_ptr<SwarmMembership>> swarms;
    {
        std::lock_guard lock(mutex_);
        for (const auto& [circle, membership] : swarms_)
            swarms.emplace_back(membership);
    }
    for (const auto& membership : swarms)
        membership->manager()->maintainBuckets();
}

bool
CircleService::add(const std::string& circle)
{
    const auto circles = module();
    if (!circles || !mountedAt(circles->path(CircleModule::circleRoute(circle)), repositoryPath(circle)))
        return false;
    bool bootstrapped;
    bool added;
    {
        std::lock_guard lock(mutex_);
        added = catalogue_.try_emplace(circle).second;
        requests_.erase(circle);
        save();
        bootstrapped = bootstrapped_;
    }
    if (bootstrapped)
        bootstrapSwarm(circle);
    enroll(circle);
    syncDevices();
    if (added)
        emitSignal<libjami::CircleSignal::CircleReady>(accountId_, circle);
    return true;
}

bool
CircleService::addChannel(const std::string& circle, const std::string& channel)
{
    const auto circles = module();
    if (!circles || !mountedAt(circles->path(CircleModule::channelRoute(channel)), channelPath(circle, channel)))
        return false;
    bool joined;
    {
        std::lock_guard lock(mutex_);
        const auto found = catalogue_.find(circle);
        if (found == catalogue_.end() || found->second.departed)
            return false;
        joined = !found->second.channels.contains(channel);
        found->second.channels[channel] = now();
        found->second.leftChannels.erase(channel);
        save();
    }
    syncDevices();
    if (joined)
        emitSignal<libjami::CircleSignal::CircleChannelReady>(accountId_, circle, channel);
    return true;
}

void
CircleService::removeChannel(const std::string& circle, const std::string& channel)
{
    if (const auto circles = module())
        circles->unmountChannel(channel);
    {
        std::lock_guard lock(mutex_);
        const auto found = catalogue_.find(circle);
        if (found == catalogue_.end() || !found->second.channels.erase(channel))
            return;
        found->second.leftChannels[channel] = now();
        histories_.erase(channel);
        save();
    }
    std::error_code error;
    std::filesystem::remove_all(channelPath(circle, channel), error);
    syncDevices();
}

void
CircleService::depart(const std::string& circle, const std::string& leaveHead)
{
    const auto circles = module();
    if (!circles || !circles->depart(circle))
        return;
    std::map<std::string, int64_t> channels;
    {
        std::lock_guard lock(mutex_);
        const auto found = catalogue_.find(circle);
        if (found == catalogue_.end())
            return;
        found->second.departed = now();
        found->second.leaveHead = leaveHead;
        channels = std::exchange(found->second.channels, {});
        for (const auto& [channel, joined] : channels)
            histories_.erase(channel);
        save();
    }
    std::error_code error;
    for (const auto& [channel, joined] : channels)
        std::filesystem::remove_all(channelPath(circle, channel), error);
    announce(CircleModule::circleRoute(circle), leaveHead);
    syncDevices();
    emitSignal<libjami::CircleSignal::CircleRemoved>(accountId_, circle);
}

void
CircleService::remove(const std::string& circle)
{
    stopSwarm(circle);
    if (const auto circles = module())
        circles->unmount(circle);
    bool member;
    {
        std::lock_guard lock(mutex_);
        const auto found = catalogue_.find(circle);
        if (found == catalogue_.end())
            return;
        // Clients already saw a departed circle go.
        member = !found->second.departed;
        for (const auto& [channel, joined] : found->second.channels)
            histories_.erase(channel);
        catalogue_.erase(found);
        save();
    }
    std::error_code error;
    std::filesystem::remove_all(root_ / circle, error);
    if (member)
        emitSignal<libjami::CircleSignal::CircleRemoved>(accountId_, circle);
}

void
CircleService::removeExpired()
{
    std::vector<std::string> expired;
    {
        std::lock_guard lock(mutex_);
        const auto limit = now() - std::chrono::duration_cast<std::chrono::seconds>(DEPARTURE_GRACE).count();
        for (const auto& [circle, info] : catalogue_) {
            if (info.departed && info.departed < limit)
                expired.emplace_back(circle);
        }
    }
    for (const auto& circle : expired)
        remove(circle);
}

void
CircleService::scheduleExpiry()
{
    std::lock_guard lock(mutex_);
    if (!expiryTimer_)
        expiryTimer_ = std::make_unique<asio::steady_timer>(*Manager::instance().ioContext());
    expiryTimer_->expires_after(EXPIRY_PERIOD);
    expiryTimer_->async_wait([w = weak_from_this()](const asio::error_code& error) {
        if (error)
            return;
        if (const auto shared = w.lock()) {
            dht::ThreadPool::io().run([w] {
                if (const auto shared = w.lock()) {
                    shared->removeExpired();
                    shared->scheduleExpiry();
                }
            });
        }
    });
}

void
CircleService::send(const std::string& route,
                    const std::string& member,
                    const DeviceId& device,
                    const std::string& payload)
{
    const auto account = account_.lock();
    if (!account)
        return;
    const auto deviceId = device ? device.toString() : std::string();
    std::lock_guard lock(refreshMtx_);
    auto& refresh = refresh_[route + "/" + (device ? deviceId : member)];
    refresh = account->sendTextMessage(member, deviceId, {{MIME_TYPE_HEAD, payload}}, refresh);
}

void
CircleService::announce(const std::string& route, const std::string& head, const std::string& from)
{
    const auto parsed = CircleModule::parseRoute(route);
    const auto account = account_.lock();
    if (!parsed || !account)
        return;
    const auto circle = circleOf(*parsed);
    const auto current = circle.empty() ? std::nullopt : policy(circle);
    if (!current)
        return;
    Json::Value message;
    message["route"] = route;
    message["circle"] = circle;
    message["head"] = head;
    message["deviceId"] = account->currentDeviceId();
    const auto payload = json::toString(message);
    const auto self = account->getUsername();

    // Our other devices learn of local writes; they hear of fetched ones from the same peers we did.
    if (from.empty())
        send(route, self, {}, payload);

    const auto membership = swarm(circle);
    if (!membership)
        return;
    std::set<std::string> readers;
    for (const auto& [member, entry] : current->members) {
        if (member != self && mayRead(*current, *parsed, member))
            readers.emplace(member);
    }

    const auto peers = membership->manager()->getConnectedNodes();
    for (const auto& member : membership->unconnectedMembers(peers, readers, UNCONNECTED_MEMBERS))
        send(route, member, {}, payload);
    for (const auto& peer : peers) {
        const auto cert = account->certStore().getCertificate(peer.toString());
        if (!cert || !cert->issuer || peer.toString() == from)
            continue;
        const auto member = cert->issuer->getId().toString();
        if (member == self || readers.contains(member))
            send(route, member, peer, payload);
    }
    for (const auto& target : membership->mobileTargets([](const std::string&) { return std::string(); },
                                                        [&](const std::string& member, const std::string& device) {
                                                            return device != from
                                                                   && (member == self || readers.contains(member));
                                                        }))
        send(route, target.member, target.device, payload);
}

void
CircleService::sendHeads(const std::string& circle, const DeviceId& device)
{
    const auto account = account_.lock();
    const auto circles = module();
    const auto current = policy(circle);
    const auto cert = account ? account->certStore().getCertificate(device.toString()) : nullptr;
    if (!circles || !current || !cert || !cert->issuer)
        return;
    const auto member = cert->issuer->getId().toString();
    const auto self = account->getUsername();
    std::vector<std::string> routes {CircleModule::circleRoute(circle)};
    {
        std::lock_guard lock(mutex_);
        if (const auto found = catalogue_.find(circle); found != catalogue_.end()) {
            for (const auto& [channel, joined] : found->second.channels)
                routes.emplace_back(CircleModule::channelRoute(channel));
        }
    }
    for (const auto& route : routes) {
        const auto parsed = CircleModule::parseRoute(route);
        const auto head = circles->head(route);
        if (!parsed || !head || (member != self && !mayRead(*current, *parsed, member)))
            continue;
        Json::Value message;
        message["route"] = route;
        message["circle"] = circle;
        message["head"] = *head;
        message["deviceId"] = account->currentDeviceId();
        send(route, member, device, json::toString(message));
    }
}

void
CircleService::onHeadAnnounced(const std::string& from, const std::string& payload)
{
    const auto account = account_.lock();
    Json::Value json;
    if (!account || payload.size() > MAX_ANNOUNCEMENT_SIZE || !json::parse(payload, json) || !json.isObject())
        return;
    const auto route = field(json, "route");
    const auto circle = field(json, "circle");
    const auto head = field(json, "head");
    const auto device = field(json, "deviceId");
    const auto parsed = CircleModule::parseRoute(route);
    if (!parsed || !isHex(head, 40) || !isHex(device, 64) || device == account->currentDeviceId()
        || circleOf(*parsed) != circle || circle.empty())
        return;
    {
        std::lock_guard lock(mutex_);
        const auto found = catalogue_.find(circle);
        if (found == catalogue_.end() || found->second.departed)
            return;
    }
    const auto circles = module();
    const auto current = policy(circle);
    if (!circles || !current || circles->accepted(route, head))
        return;
    const auto sender = current->members.find(from);
    if (from != account->getUsername()
        && (sender == current->members.end() || sender->second.state == CircleMemberState::BANNED))
        return;
    pull(route, device, head);
}

void
CircleService::pull(const std::string& route, const std::string& device, const std::string& head)
{
    {
        std::lock_guard lock(mutex_);
        auto [pending, inserted] = pulls_.try_emplace({route, device});
        pending->second.wanted = head;
        if (!inserted) {
            pending->second.again = true;
            return;
        }
    }
    fetch(route, device, false);
}

void
CircleService::fetch(const std::string& route, const std::string& device, bool dependencyFetched)
{
    const auto account = account_.lock();
    const auto circles = module();
    if (!account || !circles) {
        finishPull(route, device);
        return;
    }
    const auto previous = circles->head(route).value_or(std::string());
    account->fetchCircleRoute(
        route, device, [w = weak_from_this(), route, device, dependencyFetched, previous](CircleFetchResult result) {
            const auto shared = w.lock();
            if (!shared)
                return;
            const auto parsed = CircleModule::parseRoute(route);
            if (result.status != CircleFetchStatus::PENDING_DEPENDENCY || dependencyFetched || !parsed
                || parsed->circle) {
                shared->onFetched(route, device, result, previous);
                shared->finishPull(route, device);
                return;
            }
            // New channel content cites circle policy we do not have yet.
            const auto circle = shared->circleOf(*parsed);
            const auto account = shared->account_.lock();
            const auto circles = shared->module();
            if (!account || !circles || circle.empty()) {
                shared->finishPull(route, device);
                return;
            }
            const auto circleRoute = CircleModule::circleRoute(circle);
            account->fetchCircleRoute(circleRoute,
                                      device,
                                      [w,
                                       route,
                                       device,
                                       circleRoute,
                                       circlePrevious = circles->head(circleRoute).value_or(std::string())](
                                          CircleFetchResult result) {
                                          if (const auto shared = w.lock()) {
                                              shared->onFetched(circleRoute, device, result, circlePrevious);
                                              shared->fetch(route, device, true);
                                          }
                                      });
        });
}

void
CircleService::onFetched(const std::string& route,
                         const std::string& device,
                         const CircleFetchResult& result,
                         const std::string& previous)
{
    const auto parsed = CircleModule::parseRoute(route);
    if (result.status == CircleFetchStatus::FAILED || !parsed)
        return;
    if (parsed->circle) {
        std::lock_guard lock(mutex_);
        sources_[parsed->id] = device;
    }
    if (result.status != CircleFetchStatus::UPDATED)
        return;
    if (parsed->circle)
        onCircleChanged(parsed->id, true);
    else if (const auto circle = circleOf(*parsed); !circle.empty())
        emitMessages(circle, parsed->id, previous);
    announce(route, result.head, device);
}

std::shared_ptr<SwarmHistory>
CircleService::history(const std::string& circle, const std::string& channel)
{
    const auto account = account_.lock();
    const auto current = policy(circle);
    if (!account || !current || !mayRead(*current, CircleRoute {false, channel}, account->getUsername()))
        return {};
    std::lock_guard lock(mutex_);
    const auto found = catalogue_.find(circle);
    if (found == catalogue_.end() || !found->second.channels.contains(channel))
        return {};
    auto& history = histories_[channel];
    if (!history)
        history = std::make_shared<SwarmHistory>();
    return history;
}

void
CircleService::emitMessages(const std::string& circle, const std::string& channel, const std::string& previous)
{
    const auto circles = module();
    const auto mounted = circles ? circles->circle(circle) : nullptr;
    const auto path = circles ? circles->path(CircleModule::channelRoute(channel)) : std::nullopt;
    const auto history = mounted && path ? this->history(circle, channel) : nullptr;
    if (!history)
        return;
    std::vector<SwarmHistory::Message> messages;
    std::set<std::string> commits;
    bool merged = false;
    if (!mounted->channelLog(
            *path,
            channel,
            [&](CircleChannelEntry&& entry) {
                merged |= entry.parents.size() > 1;
                commits.emplace(entry.id);
                if (auto message = channelMessage(entry))
                    messages.emplace_back(std::move(message));
                return true;
            },
            {},
            previous))
        return;
    std::vector<std::string> edited;
    {
        std::lock_guard lock(history->mutex);
        for (auto message = messages.rbegin(); message != messages.rend(); ++message) {
            if (history->quickAccess.contains((*message)->id))
                continue;
            history->add(accountId_, channel, *message, true);
            if (const auto edit = (*message)->body.find(CommitKey::EDIT); edit != (*message)->body.end())
                edited.emplace_back(edit->second);
        }
        // A merge may have interleaved these messages with ones already shown.
        if (merged)
            history->relinearize(accountId_, channel, std::move(commits), [&](const SwarmHistory::Visitor& visit) {
                mounted->channelLog(*path, channel, [&](CircleChannelEntry&& entry) {
                    return !visit(entry.id, entry.parents.size() > 1);
                });
            });
    }
    for (const auto& id : edited)
        history->signalEdited(accountId_, channel, id);
}

void
CircleService::finishPull(const std::string& route, const std::string& device)
{
    const auto circles = module();
    bool again = false;
    {
        std::lock_guard lock(mutex_);
        const auto pending = pulls_.find({route, device});
        if (pending == pulls_.end())
            return;
        if (pending->second.again && circles && !circles->accepted(route, pending->second.wanted)) {
            pending->second.again = false;
            again = true;
        } else {
            pulls_.erase(pending);
        }
    }
    if (again)
        fetch(route, device, false);
}

bool
CircleService::authorizeSwarmChannel(std::string_view route, const dht::crypto::Certificate& peer)
{
    const auto parsed = CircleModule::parseRoute(route);
    if (!parsed || !parsed->circle || circleOf(*parsed).empty())
        return false;
    const auto circles = module();
    return circles && circles->authorize(route, peer);
}

void
CircleService::addSwarmChannel(std::string_view route, std::shared_ptr<dhtnet::ChannelSocket> channel)
{
    const auto parsed = CircleModule::parseRoute(route);
    const auto cert = channel ? channel->peerCertificate() : nullptr;
    const auto membership = parsed && parsed->circle ? swarm(parsed->id) : nullptr;
    if (!membership || !cert || !cert->issuer) {
        if (channel)
            channel->shutdown();
        return;
    }
    const auto device = channel->deviceId();
    membership->addChannel(std::move(channel));
    // Both sides send their heads, so whoever is behind pulls.
    dht::ThreadPool::io().run([w = weak_from_this(), circle = parsed->id, device] {
        if (const auto shared = w.lock())
            shared->sendHeads(circle, device);
    });
}

void
CircleService::addKnownDevice(const std::string& member, const DeviceId& device)
{
    std::vector<std::pair<std::string, std::shared_ptr<SwarmMembership>>> swarms;
    {
        std::lock_guard lock(mutex_);
        swarms.assign(swarms_.begin(), swarms_.end());
    }
    for (const auto& [circle, membership] : swarms) {
        if (isActiveMember(circle, member))
            membership->addKnownDevices({device}, member);
    }
}

void
CircleService::onServed(const std::string& route, const std::string& commit)
{
    const auto parsed = CircleModule::parseRoute(route);
    if (!parsed || !parsed->circle)
        return;
    {
        std::lock_guard lock(mutex_);
        const auto found = catalogue_.find(parsed->id);
        if (found == catalogue_.end() || !found->second.departed || found->second.leaveHead != commit)
            return;
    }
    // A peer holds our leave now and will spread it: the circle is no longer ours to serve.
    dht::ThreadPool::io().run([w = weak_from_this(), circle = parsed->id] {
        if (const auto shared = w.lock())
            shared->remove(circle);
    });
}

void
CircleService::onCircleChanged(const std::string& circle, bool fetched)
{
    std::shared_ptr<SwarmMembership> membership;
    {
        std::lock_guard lock(mutex_);
        if (const auto found = swarms_.find(circle); found != swarms_.end())
            membership = found->second;
    }
    if (membership) {
        const auto members = activeMembers(circle);
        membership->pruneMembers({members.begin(), members.end()});
    }
    const auto account = account_.lock();
    const auto current = policy(circle);
    if (!account || !current)
        return;
    const auto self = stateOf(*current, account->getUsername());
    const bool gone = !self || *self == CircleMemberState::LEFT || *self == CircleMemberState::BANNED;
    // Another device of ours left, and serves the leave; a ban leaves nothing to serve.
    if (fetched && gone && !departed(circle)) {
        remove(circle);
        return;
    }
    enroll(circle);
    emitSignal<libjami::CircleSignal::CircleUpdated>(accountId_, circle);
}

void
CircleService::enroll(const std::string& circle)
{
    const auto account = account_.lock();
    const auto mounted = repository(circle);
    if (!account || !mounted || departed(circle))
        return;
    const auto current = mounted->policy();
    const auto self = account->getUsername();
    const std::string device(account->currentDeviceId());
    if (current.ended || stateOf(current, self) != CircleMemberState::ACTIVE || mounted->enrolled(device))
        return;
    if (const auto head = mounted->append(account->identity(), RegisterCircleDevice {self, device}))
        announce(CircleModule::circleRoute(circle), *head);
}

std::optional<std::string>
CircleService::write(const std::string& circle, const CirclePolicyChange& change)
{
    const auto account = account_.lock();
    const auto mounted = repository(circle);
    if (!account || !mounted || departed(circle))
        return std::nullopt;
    auto head = mounted->append(account->identity(), change);
    if (head) {
        onCircleChanged(circle, false);
        announce(CircleModule::circleRoute(circle), *head);
    }
    return head;
}

std::optional<std::string>
CircleService::moderate(const std::string& circle, const CircleModerationVote& vote)
{
    const auto account = account_.lock();
    const auto mounted = repository(circle);
    if (!account || !mounted || departed(circle))
        return std::nullopt;
    const auto result = mounted->moderate(account->identity(), vote);
    if (!result)
        return std::nullopt;
    const auto head = result->resolution.value_or(result->vote);
    onCircleChanged(circle, false);
    announce(CircleModule::circleRoute(circle), head);
    return head;
}

std::optional<std::string>
CircleService::create(const PermissionRule& defaults)
{
    const auto account = account_.lock();
    if (!account)
        return std::nullopt;
    const auto staging = stagingPath(root_);
    std::string circle;
    if (auto created = CircleRepository::create(staging, account->identity(), defaults))
        circle = created->id();
    else
        return std::nullopt;
    std::error_code error;
    const auto path = repositoryPath(circle);
    std::filesystem::create_directories(path.parent_path(), error);
    std::filesystem::rename(staging, path, error);
    const auto circles = module();
    if (error || !circles || !circles->mountCircle(path, circle) || !add(circle)) {
        if (circles)
            circles->unmount(circle);
        std::filesystem::remove_all(staging, error);
        std::filesystem::remove_all(root_ / circle, error);
        return std::nullopt;
    }
    return circle;
}

std::optional<std::string>
CircleService::invite(const std::string& circle, const std::string& member)
{
    const auto account = account_.lock();
    const auto head = account ? write(circle, InviteCircleMember {member}) : std::nullopt;
    if (!head)
        return std::nullopt;
    Json::Value message;
    message["circle"] = circle;
    message["deviceId"] = account->currentDeviceId();
    const auto name = profile(circle)["title"];
    if (name.size() <= 256)
        message["name"] = name;
    auto payload = json::toString(message);
    if (payload.size() > MAX_ANNOUNCEMENT_SIZE) {
        message.removeMember("name");
        payload = json::toString(message);
    }
    account->sendTextMessage(member, {}, {{MIME_TYPE_INVITE, std::move(payload)}});
    return head;
}

std::optional<std::string>
CircleService::ban(const std::string& circle, const std::string& member)
{
    return moderate(circle, {CircleModerationAction::BAN_MEMBER, member});
}

std::optional<std::string>
CircleService::unban(const std::string& circle, const std::string& member)
{
    return moderate(circle, {CircleModerationAction::UNBAN_MEMBER, member});
}

std::optional<std::string>
CircleService::updatePolicy(const std::string& circle, std::string_view text)
{
    const auto change = circlePolicyChangeFromJson(text);
    // Membership, channel lifecycle and ownership have their own operations.
    const bool editable = change
                          && std::visit(
                              [](const auto& operation) {
                                  using Operation = std::decay_t<decltype(operation)>;
                                  return std::is_same_v<Operation, SetCircleDefaults>
                                         || std::is_same_v<Operation, SetCircleRole>
                                         || std::is_same_v<Operation, SetCircleRoleAssignment>
                                         || std::is_same_v<Operation, SetCircleChannelRule>
                                         || std::is_same_v<Operation, SetCircleRoleDelegation>;
                              },
                              *change);
    return editable ? write(circle, *change) : std::nullopt;
}

std::map<std::string, std::string>
CircleService::profile(const std::string& circle, const std::string& channel)
{
    const auto mounted = repository(circle);
    return mounted ? ConversationRepository::infosFromVCard(vCard::utils::toMap(
                         mounted->profile(channel.empty() ? std::nullopt : std::optional<std::string_view>(channel))))
                   : std::map<std::string, std::string>();
}

bool
CircleService::updateProfile(const std::string& circle,
                             const std::map<std::string, std::string>& fields,
                             const std::string& channel)
{
    const auto account = account_.lock();
    const auto mounted = repository(circle);
    if (!account || !mounted || departed(circle) || fields.empty())
        return false;
    const auto target = channel.empty() ? std::nullopt : std::optional<std::string_view>(channel);
    auto card = vCard::utils::toMap(mounted->profile(target));
    card[std::string(vCard::Property::VCARD_VERSION)] = "2.1";
    for (const auto& [key, value] : fields) {
        std::string property;
        if (key == "title")
            property = vCard::Property::FORMATTED_NAME;
        else if (key == "description")
            property = vCard::Property::DESCRIPTION;
        else if (key == "avatar" && channel.empty()) {
            vCard::utils::removeByKey(card, vCard::Property::PHOTO);
            property = vCard::Property::PHOTO_PNG;
        } else {
            return false;
        }
        card[property] = value;
    }
    const auto head = mounted->updateProfile(account->identity(), vCard::utils::toString(card), target);
    if (!head)
        return false;
    onCircleChanged(circle, false);
    announce(CircleModule::circleRoute(circle), *head);
    return true;
}

bool
CircleService::leave(const std::string& circle, const std::string& successor)
{
    const auto account = account_.lock();
    const auto current = policy(circle);
    if (!account || !current)
        return false;
    const auto self = account->getUsername();
    std::optional<std::string> head;
    if (current->admin != self)
        head = write(circle, LeaveCircleMember {self});
    else if (successor.empty())
        head = write(circle, EndCircle {});
    else if (write(circle, TransferCircleAdmin {successor}))
        head = write(circle, LeaveCircleMember {self});
    if (!head)
        return false;
    depart(circle, *head);
    return true;
}

std::optional<std::string>
CircleService::createChannel(const std::string& circle, const ChannelPolicy& channelPolicy)
{
    const auto account = account_.lock();
    const auto mounted = repository(circle);
    if (!account || !mounted || departed(circle))
        return std::nullopt;
    std::error_code error;
    const auto directory = root_ / circle / "channels";
    std::filesystem::create_directories(directory, error);
    const auto staging = stagingPath(directory);
    const auto created = mounted->createChannel(staging, account->identity(), channelPolicy);
    if (!created || !created->registration) {
        std::filesystem::remove_all(staging, error);
        return std::nullopt;
    }
    const auto path = channelPath(circle, created->channel);
    std::filesystem::rename(staging, path, error);
    const auto circles = module();
    if (error || !circles || !circles->mountChannel(path, circle, created->channel)
        || !addChannel(circle, created->channel)) {
        if (circles)
            circles->unmountChannel(created->channel);
        std::filesystem::remove_all(staging, error);
        std::filesystem::remove_all(path, error);
        return std::nullopt;
    }
    onCircleChanged(circle, false);
    announce(CircleModule::circleRoute(circle), *created->registration);
    const auto route = CircleModule::channelRoute(created->channel);
    if (const auto head = circles->head(route))
        announce(route, *head);
    return created->channel;
}

std::optional<std::string>
CircleService::archiveChannel(const std::string& circle, const std::string& channel)
{
    return write(circle, ArchiveCircleChannel {channel});
}

void
CircleService::joinChannel(const std::string& circle, const std::string& channel, Done done, const std::string& deviceId)
{
    const auto account = account_.lock();
    const auto circles = module();
    const auto current = policy(circle);
    const auto route = CircleModule::channelRoute(channel);
    const auto self = account ? account->getUsername() : std::string();
    if (!account || !circles || !current || departed(circle) || !current->channels.contains(channel)
        || !mayRead(*current, CircleRoute {false, channel}, self)) {
        done(false);
        return;
    }
    if (mountedAt(circles->path(route), channelPath(circle, channel))) {
        done(addChannel(circle, channel));
        return;
    }
    std::vector<std::string> devices;
    if (!deviceId.empty()) {
        devices.emplace_back(deviceId);
    } else {
        // Clones try devices from the back: connected readers before the last circle source.
        {
            std::lock_guard lock(mutex_);
            if (const auto source = sources_.find(circle); source != sources_.end())
                devices.emplace_back(source->second);
        }
        if (const auto membership = swarm(circle)) {
            for (const auto& peer : membership->manager()->getConnectedNodes()) {
                const auto cert = account->certStore().getCertificate(peer.toString());
                if (cert && cert->issuer
                    && (cert->issuer->getId().toString() == self
                        || mayRead(*current, CircleRoute {false, channel}, cert->issuer->getId().toString())))
                    devices.emplace_back(peer.toString());
            }
        }
    }
    std::error_code error;
    std::filesystem::create_directories(root_ / circle / "channels", error);
    clone(route,
          circle,
          std::move(devices),
          channelPath(circle, channel),
          [w = weak_from_this(), circle, channel, done = std::move(done)](bool cloned) {
              const auto shared = w.lock();
              done(cloned && shared && shared->addChannel(circle, channel));
          });
}

void
CircleService::leaveChannel(const std::string& circle, const std::string& channel)
{
    removeChannel(circle, channel);
}

bool
CircleService::isChannel(const std::string& id) const
{
    return !circleOf(CircleRoute {false, id}).empty();
}

std::optional<std::string>
CircleService::sendMessage(const std::string& channel, const CommitMessage& message)
{
    const auto circles = module();
    const auto circle = circleOf(CircleRoute {false, channel});
    if (!circles || circle.empty())
        return std::nullopt;
    const auto route = CircleModule::channelRoute(channel);
    const auto previous = circles->head(route).value_or(std::string());
    const auto head = circles->appendChannelMessage(channel, message);
    if (head) {
        emitMessages(circle, channel, previous);
        announce(route, *head);
    }
    return head;
}

std::vector<libjami::SwarmMessage>
CircleService::loadMessages(const std::string& channel, const std::string& from, const std::string& to, size_t count)
{
    const auto circle = circleOf(CircleRoute {false, channel});
    const auto circles = module();
    const auto mounted = circles ? circles->circle(circle) : nullptr;
    const auto path = circles ? circles->path(CircleModule::channelRoute(channel)) : std::nullopt;
    const auto history = mounted && path ? this->history(circle, channel) : nullptr;
    if (!history)
        return {};
    std::vector<SwarmHistory::Message> loaded;
    std::lock_guard lock(history->mutex);
    const auto oldest = history->messageList.empty() ? nullptr : history->messageList.back();
    bool last = false;
    mounted->channelLog(
        *path,
        channel,
        [&](CircleChannelEntry&& entry) {
            if (last || (count && loaded.size() == count))
                return false;
            last = entry.id == to;
            const auto message = channelMessage(entry);
            if (message && !history->quickAccess.contains(message->id)
                && history->add(accountId_, channel, message, false))
                loaded.emplace_back(message);
            return true;
        },
        from);
    // The oldest message clients had got its parent.
    if (oldest && !loaded.empty())
        emitSignal<libjami::ConversationSignal::SwarmMessageUpdated>(accountId_, channel, *oldest);
    // Copied once loaded: a message gets its parent when the next one is.
    std::vector<libjami::SwarmMessage> messages;
    messages.reserve(loaded.size());
    for (const auto& message : loaded)
        messages.emplace_back(*message);
    return messages;
}

void
CircleService::clearCache(const std::string& channel)
{
    std::shared_ptr<SwarmHistory> history;
    {
        std::lock_guard lock(mutex_);
        if (const auto found = histories_.find(channel); found != histories_.end())
            history = found->second;
    }
    if (history) {
        std::lock_guard lock(history->mutex);
        history->clear();
    }
}

std::vector<std::string>
CircleService::permissions(const std::string& channel)
{
    std::vector<std::string> names;
    const auto account = account_.lock();
    const auto current = policy(circleOf(CircleRoute {false, channel}));
    if (account && current) {
        for (const auto permission : current->permissions(account->getUsername(), channel))
            names.emplace_back(permissionName(permission));
    }
    return names;
}

void
CircleService::clone(const std::string& route,
                     const std::string& circle,
                     std::vector<std::string> devices,
                     const std::filesystem::path& path,
                     Done done)
{
    const auto account = account_.lock();
    if (!account || devices.empty()) {
        done(false);
        return;
    }
    const auto device = devices.back();
    devices.pop_back();
    account->cloneCircleRoute(route,
                              device,
                              path,
                              circle,
                              [w = weak_from_this(),
                               route,
                               circle,
                               device,
                               devices = std::move(devices),
                               path,
                               done = std::move(done)](bool cloned) mutable {
                                  const auto shared = w.lock();
                                  if (cloned && shared && route == CircleModule::circleRoute(circle)) {
                                      std::lock_guard lock(shared->mutex_);
                                      shared->sources_[circle] = device;
                                  }
                                  if (cloned || !shared)
                                      done(cloned);
                                  else
                                      shared->clone(route, circle, std::move(devices), path, std::move(done));
                              });
}

void
CircleService::onInvitation(const std::string& from, const std::string& payload)
{
    const auto account = account_.lock();
    Json::Value json;
    if (!account || payload.size() > MAX_ANNOUNCEMENT_SIZE || !json::parse(payload, json) || !json.isObject())
        return;
    const auto circle = field(json, "circle");
    const auto device = field(json, "deviceId");
    const auto manager = account->accountManager();
    if (!isHex(circle, 40) || !isHex(device, 64) || !manager
        || manager->getCertificateStatus(from) == dhtnet::tls::TrustStore::PermissionStatus::BANNED)
        return;
    std::optional<std::map<std::string, std::string>> received;
    {
        std::lock_guard lock(mutex_);
        const auto found = requests_.find(circle);
        if (catalogue_.contains(circle) || (found != requests_.end() && found->second.declined)
            || (found == requests_.end() && requests_.size() >= MAX_REQUESTS))
            return;
        const bool fresh = found == requests_.end();
        const auto& request = requests_[circle] = CircleRequest {from, device, now(), 0, field(json, "name")};
        if (fresh)
            received = requestMap(circle, request);
        save();
    }
    syncDevices();
    if (received)
        emitSignal<libjami::CircleSignal::CircleRequestReceived>(accountId_, circle, std::move(*received));
}

void
CircleService::acceptRequest(const std::string& circle, Done done)
{
    CircleRequest request;
    {
        std::lock_guard lock(mutex_);
        const auto found = requests_.find(circle);
        if (found == requests_.end() || catalogue_.contains(circle) || !cloning_.emplace(circle).second) {
            done(false);
            return;
        }
        request = found->second;
    }
    std::error_code error;
    std::filesystem::create_directories(root_ / circle, error);
    clone(CircleModule::circleRoute(circle),
          circle,
          {request.device},
          repositoryPath(circle),
          [w = weak_from_this(), circle, done = std::move(done)](bool cloned) {
              const auto shared = w.lock();
              if (!shared) {
                  done(false);
                  return;
              }
              {
                  std::lock_guard lock(shared->mutex_);
                  shared->cloning_.erase(circle);
              }
              const auto account = shared->account_.lock();
              if (!cloned || !account || !shared->add(circle)) {
                  std::error_code error;
                  if (!cloned)
                      std::filesystem::remove(shared->root_ / circle, error);
                  done(false);
                  return;
              }
              const auto current = shared->policy(circle);
              const auto self = current ? stateOf(*current, account->getUsername()) : std::nullopt;
              // Another of our devices may have joined already; add() enrolled this one then.
              if (self == CircleMemberState::INVITED)
                  done(shared->write(circle, JoinCircleMember {account->getUsername()}).has_value());
              else
                  done(self == CircleMemberState::ACTIVE);
          });
}

void
CircleService::declineRequest(const std::string& circle)
{
    {
        std::lock_guard lock(mutex_);
        const auto found = requests_.find(circle);
        if (found == requests_.end() || found->second.declined)
            return;
        found->second.declined = now();
        save();
    }
    syncDevices();
}

void
CircleService::syncDevices()
{
    const auto account = account_.lock();
    auto* sync = account ? account->syncModule() : nullptr;
    if (!sync)
        return;
    auto message = std::make_shared<SyncMsg>();
    {
        std::lock_guard lock(mutex_);
        message->ci = catalogue_;
        message->cir = requests_;
    }
    sync->syncWithConnected(message);
}

void
CircleService::onSyncData(const std::map<std::string, CircleInfo>& circles,
                          const std::map<std::string, CircleRequest>& requests,
                          const std::string& deviceId)
{
    for (const auto& [circle, remote] : circles) {
        if (!isHex(circle, 40))
            continue;
        std::optional<CircleInfo> local;
        {
            std::lock_guard lock(mutex_);
            if (const auto found = catalogue_.find(circle); found != catalogue_.end())
                local = found->second;
        }
        if (remote.departed) {
            // The leaving device serves the leave; this one has nothing left to do.
            if (local && !local->departed)
                remove(circle);
            continue;
        }
        if (local) {
            if (!local->departed)
                syncChannels(circle, remote, deviceId);
            continue;
        }
        {
            std::lock_guard lock(mutex_);
            if (!cloning_.emplace(circle).second)
                continue;
        }
        std::error_code error;
        std::filesystem::create_directories(root_ / circle, error);
        clone(CircleModule::circleRoute(circle),
              circle,
              {deviceId},
              repositoryPath(circle),
              [w = weak_from_this(), circle, remote, deviceId](bool cloned) {
                  const auto shared = w.lock();
                  if (!shared)
                      return;
                  {
                      std::lock_guard lock(shared->mutex_);
                      shared->cloning_.erase(circle);
                  }
                  if (cloned && shared->add(circle))
                      shared->syncChannels(circle, remote, deviceId);
              });
    }

    std::vector<std::pair<std::string, std::map<std::string, std::string>>> received;
    {
        std::lock_guard lock(mutex_);
        bool changed = false;
        for (const auto& [circle, remote] : requests) {
            if (!isHex(circle, 40) || catalogue_.contains(circle))
                continue;
            auto found = requests_.find(circle);
            if (found == requests_.end()) {
                if (requests_.size() >= MAX_REQUESTS)
                    continue;
                requests_.emplace(circle, remote);
                if (!remote.declined)
                    received.emplace_back(circle, requestMap(circle, remote));
                changed = true;
            } else if (remote.declined && !found->second.declined) {
                found->second.declined = remote.declined;
                changed = true;
            }
        }
        if (changed)
            save();
    }
    for (auto& [circle, request] : received)
        emitSignal<libjami::CircleSignal::CircleRequestReceived>(accountId_, circle, std::move(request));
}

std::map<std::string, std::string>
CircleService::requestMap(const std::string& circle, const CircleRequest& request)
{
    return {{"id", circle},
            {"from", request.from},
            {"received", std::to_string(request.received)},
            {"name", request.name}};
}

void
CircleService::syncChannels(const std::string& circle, const CircleInfo& remote, const std::string& deviceId)
{
    std::optional<CircleInfo> local;
    {
        std::lock_guard lock(mutex_);
        if (const auto found = catalogue_.find(circle); found != catalogue_.end())
            local = found->second;
    }
    if (!local)
        return;
    // The latest choice wins: joined after we left, or left after we joined.
    for (const auto& [channel, joined] : remote.channels) {
        const auto left = local->leftChannels.find(channel);
        if (!local->channels.contains(channel) && (left == local->leftChannels.end() || left->second < joined))
            joinChannel(circle, channel, [](bool) {}, deviceId);
    }
    for (const auto& [channel, left] : remote.leftChannels) {
        const auto joined = local->channels.find(channel);
        if (joined != local->channels.end() && joined->second < left)
            removeChannel(circle, channel);
    }
}

} // namespace jami
