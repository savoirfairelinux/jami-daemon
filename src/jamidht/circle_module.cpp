#include "circle_module.h"

#include "git_def.h"

#include <algorithm>
#include <ctime>
#include <map>
#include <mutex>

namespace jami {
namespace {

std::chrono::steady_clock::time_point
expiration(const dht::crypto::Certificate& certificate, std::chrono::steady_clock::time_point limit)
{
    for (const auto* current = &certificate; current && current->cert; current = current->issuer.get()) {
        const auto expires = gnutls_x509_crt_get_expiration_time(current->cert);
        if (expires == time_t(-1))
            return std::chrono::steady_clock::now();
        limit = std::min(limit, std::chrono::steady_clock::now() + std::chrono::seconds(expires - std::time(nullptr)));
    }
    return limit;
}

void
removeReference(const std::filesystem::path& path, const char* name)
{
    git_repository* raw = nullptr;
    if (git_repository_open_bare(&raw, path.string().c_str()) < 0)
        return;
    GitRepository repo(raw);
    git_reference* reference = nullptr;
    if (git_reference_lookup(&reference, repo.get(), name) == 0) {
        git_reference_delete(reference);
        git_reference_free(reference);
    }
}

} // namespace

CircleSession::CircleSession(std::filesystem::path path,
                             std::string repositoryId,
                             std::string sessionRoute,
                             std::string peerAccount,
                             std::string peerDevice,
                             std::function<std::optional<std::string>()> grant,
                             std::function<void()> shutdown)
    : repository(std::move(path))
    , id(std::move(repositoryId))
    , route(std::move(sessionRoute))
    , account(std::move(peerAccount))
    , device(std::move(peerDevice))
    , grant_(std::move(grant))
    , shutdown_(std::move(shutdown))
{}

CircleSession::~CircleSession()
{
    close();
}

std::optional<std::string>
CircleSession::grant()
{
    if (closed_)
        return std::nullopt;
    try {
        if (auto head = grant_ ? grant_() : std::nullopt; head && !closed_)
            return head;
    } catch (const std::exception&) {
    }
    close();
    return std::nullopt;
}

void
CircleSession::close()
{
    if (!closed_.exchange(true) && shutdown_) {
        try {
            shutdown_();
        } catch (const std::exception&) {
        }
    }
}

struct CircleModule::Impl
{
    struct Circle
    {
        std::filesystem::path path;
        std::shared_ptr<CircleRepository> repository;
        bool departing {false};
    };
    struct Channel
    {
        std::string circle;
        std::filesystem::path path;
        std::string head;
    };
    struct Grant
    {
        std::filesystem::path path;
        std::string head;
        std::shared_ptr<CircleRepository> repository;
        uint64_t generation {0};
        uint64_t repositoryGeneration {0};
        std::chrono::steady_clock::time_point expires;
    };
    struct CachedGrant
    {
        std::mutex mutex;
        std::optional<Grant> grant;
    };

    dht::crypto::Identity identity;
    CertificatePolicy allowed;
    std::chrono::seconds lifetime;
    // Bumped by every mount or trust change so cached grants are checked again.
    std::atomic<uint64_t> generation {0};
    mutable std::mutex mutex;
    std::map<std::string, Circle, std::less<>> circles;
    std::map<std::string, Channel, std::less<>> channels;
    std::vector<std::weak_ptr<CircleSession>> sessions;
    bool stopped {false};

    std::optional<Grant> resolve(std::string_view route, const dht::crypto::Certificate& peer) const
    {
        Grant grant;
        grant.generation = generation;
        const auto parsed = parseRoute(route);
        const auto& local = identity.second;
        if (!parsed || !local || !allowed || !allowed(peer) || !allowed(*local))
            return std::nullopt;
        Circle circle;
        Channel channel;
        {
            std::lock_guard lock(mutex);
            if (stopped)
                return std::nullopt;
            std::string id = parsed->id;
            if (!parsed->circle) {
                const auto found = channels.find(id);
                if (found == channels.end() || found->second.head.empty())
                    return std::nullopt;
                channel = found->second;
                id = channel.circle;
            }
            const auto found = circles.find(id);
            if (found == circles.end() || !found->second.repository)
                return std::nullopt;
            circle = found->second;
        }
        // Adopts verified changes written by another instance instead of pausing serving.
        circle.repository->reload();
        grant.repositoryGeneration = circle.repository->generation();
        const auto scope = parsed->circle ? std::nullopt : std::optional<std::string_view>(parsed->id);
        if ((!circle.departing && !circle.repository->authorizePeer(*local, scope))
            || !circle.repository->authorizePeer(peer, scope))
            return std::nullopt;
        grant.path = parsed->circle ? circle.path : channel.path;
        grant.head = parsed->circle ? circle.repository->head() : channel.head;
        grant.repository = circle.repository;
        grant.expires = expiration(peer, std::chrono::steady_clock::now() + lifetime);
        return grant;
    }

    bool fresh(const Grant& grant) const
    {
        return grant.generation == generation && grant.repositoryGeneration == grant.repository->generation()
               && std::chrono::steady_clock::now() < grant.expires;
    }
};

CircleModule::CircleModule(dht::crypto::Identity identity, CertificatePolicy allowed, std::chrono::seconds grantLifetime)
    : pimpl_(std::make_shared<Impl>())
{
    pimpl_->identity = std::move(identity);
    pimpl_->allowed = std::move(allowed);
    pimpl_->lifetime = grantLifetime;
}

CircleModule::~CircleModule()
{
    {
        std::lock_guard lock(pimpl_->mutex);
        pimpl_->stopped = true;
    }
    revalidate();
}

bool
CircleModule::reserved(std::string_view route)
{
    return route == "circles" || route == "channels" || route.starts_with("circles/") || route.starts_with("channels/");
}

std::optional<CircleRoute>
CircleModule::parseRoute(std::string_view route)
{
    const bool circle = route.starts_with("circles/v1/");
    const auto prefix = circle ? std::string_view("circles/v1/") : std::string_view("channels/v1/");
    if (!route.starts_with(prefix))
        return std::nullopt;
    route.remove_prefix(prefix.size());
    if (route.size() != 40 || !std::all_of(route.begin(), route.end(), [](char character) {
            return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f');
        }))
        return std::nullopt;
    return CircleRoute {circle, std::string(route)};
}

std::string
CircleModule::circleRoute(std::string_view circle)
{
    return "circles/v1/" + std::string(circle);
}

std::string
CircleModule::channelRoute(std::string_view channel)
{
    return "channels/v1/" + std::string(channel);
}

bool
CircleModule::mountCircle(const std::filesystem::path& path, std::string_view circle, bool departing)
{
    std::error_code error;
    const auto resolved = std::filesystem::canonical(path, error);
    auto repository = error ? nullptr : CircleRepository::open(resolved, circle);
    const auto& local = pimpl_->identity.second;
    if (!repository || !local || (!departing && !repository->authorizePeer(*local)))
        return false;
    {
        std::lock_guard lock(pimpl_->mutex);
        pimpl_->circles[std::string(circle)] = {resolved, std::move(repository), departing};
        if (departing)
            std::erase_if(pimpl_->channels, [&](const auto& entry) { return entry.second.circle == circle; });
        for (auto& [id, channel] : pimpl_->channels) {
            if (channel.circle == circle)
                channel.head.clear();
        }
    }
    revalidate();
    return true;
}

bool
CircleModule::mountChannel(const std::filesystem::path& path, std::string_view circle, std::string_view channel)
{
    std::shared_ptr<CircleRepository> repository;
    {
        std::lock_guard lock(pimpl_->mutex);
        const auto found = pimpl_->circles.find(circle);
        if (found == pimpl_->circles.end() || found->second.departing)
            return false;
        repository = found->second.repository;
    }
    std::error_code error;
    const auto resolved = std::filesystem::canonical(path, error);
    const auto& local = pimpl_->identity.second;
    if (error || !repository || !local || !repository->authorizePeer(*local, channel))
        return false;
    const auto result = repository->validateChannel(resolved, channel);
    if (result.status != ChannelValidationStatus::VALID)
        return false;
    {
        std::lock_guard lock(pimpl_->mutex);
        const auto found = pimpl_->circles.find(circle);
        if (found == pimpl_->circles.end() || found->second.repository != repository)
            return false;
        const auto existing = pimpl_->channels.find(channel);
        if (existing != pimpl_->channels.end() && existing->second.circle != circle)
            return false;
        pimpl_->channels[std::string(channel)] = {std::string(circle), resolved, result.head};
    }
    revalidate();
    return true;
}

bool
CircleModule::refresh(std::string_view circle)
{
    std::filesystem::path path;
    std::shared_ptr<CircleRepository> previous;
    std::map<std::string, Impl::Channel> channels;
    {
        std::lock_guard lock(pimpl_->mutex);
        const auto found = pimpl_->circles.find(circle);
        if (found == pimpl_->circles.end())
            return false;
        path = found->second.path;
        previous = found->second.repository;
        for (const auto& [id, channel] : pimpl_->channels) {
            if (channel.circle == circle)
                channels.emplace(id, channel);
        }
    }
    std::shared_ptr<CircleRepository> repository = previous && previous->reload()
                                                       ? previous
                                                       : std::shared_ptr<CircleRepository>(
                                                             CircleRepository::open(path, circle));
    for (auto& [id, channel] : channels) {
        const auto result = repository ? repository->validateChannel(channel.path, id) : ChannelValidationResult {};
        channel.head = result.status == ChannelValidationStatus::VALID ? result.head : std::string();
    }
    {
        std::lock_guard lock(pimpl_->mutex);
        const auto found = pimpl_->circles.find(circle);
        if (found == pimpl_->circles.end() || found->second.path != path || found->second.repository != previous)
            return false;
        found->second.repository = repository;
        for (const auto& [id, channel] : channels) {
            const auto current = pimpl_->channels.find(id);
            if (current != pimpl_->channels.end() && current->second.path == channel.path
                && current->second.circle == circle)
                current->second = channel;
        }
    }
    revalidate();
    return bool(repository);
}

void
CircleModule::unmount(std::string_view circle)
{
    {
        std::lock_guard lock(pimpl_->mutex);
        pimpl_->circles.erase(std::string(circle));
        std::erase_if(pimpl_->channels, [&](const auto& entry) { return entry.second.circle == circle; });
    }
    revalidate();
}

void
CircleModule::unmountChannel(std::string_view channel)
{
    {
        std::lock_guard lock(pimpl_->mutex);
        pimpl_->channels.erase(std::string(channel));
    }
    revalidate();
}

bool
CircleModule::depart(std::string_view circle)
{
    {
        std::lock_guard lock(pimpl_->mutex);
        const auto found = pimpl_->circles.find(circle);
        if (found == pimpl_->circles.end())
            return false;
        found->second.departing = true;
        std::erase_if(pimpl_->channels, [&](const auto& entry) { return entry.second.circle == circle; });
    }
    revalidate();
    return true;
}

std::vector<std::string>
CircleModule::circles() const
{
    std::lock_guard lock(pimpl_->mutex);
    std::vector<std::string> result;
    for (const auto& [id, circle] : pimpl_->circles)
        result.push_back(id);
    return result;
}

std::vector<std::string>
CircleModule::channels(std::string_view circle) const
{
    std::lock_guard lock(pimpl_->mutex);
    std::vector<std::string> result;
    for (const auto& [id, channel] : pimpl_->channels) {
        if (channel.circle == circle)
            result.push_back(id);
    }
    return result;
}

std::optional<std::filesystem::path>
CircleModule::path(std::string_view route) const
{
    const auto parsed = parseRoute(route);
    if (!parsed)
        return std::nullopt;
    std::lock_guard lock(pimpl_->mutex);
    if (parsed->circle) {
        const auto found = pimpl_->circles.find(parsed->id);
        return found == pimpl_->circles.end() ? std::nullopt : std::optional(found->second.path);
    }
    const auto found = pimpl_->channels.find(parsed->id);
    return found == pimpl_->channels.end() ? std::nullopt : std::optional(found->second.path);
}

std::optional<std::string>
CircleModule::head(std::string_view route) const
{
    const auto parsed = parseRoute(route);
    if (!parsed)
        return std::nullopt;
    std::shared_ptr<CircleRepository> repository;
    {
        std::lock_guard lock(pimpl_->mutex);
        if (!parsed->circle) {
            const auto found = pimpl_->channels.find(parsed->id);
            if (found == pimpl_->channels.end() || found->second.head.empty())
                return std::nullopt;
            return found->second.head;
        }
        const auto found = pimpl_->circles.find(parsed->id);
        if (found == pimpl_->circles.end() || !found->second.repository)
            return std::nullopt;
        repository = found->second.repository;
    }
    return repository->head();
}

bool
CircleModule::accepted(std::string_view route, std::string_view commit) const
{
    const auto location = path(route);
    const auto current = head(route);
    git_oid target {}, tip {};
    if (!location || !current || commit.size() != GIT_OID_HEXSZ
        || git_oid_fromstr(&target, std::string(commit).c_str()) < 0 || git_oid_fromstr(&tip, current->c_str()) < 0)
        return false;
    if (git_oid_equal(&target, &tip))
        return true;
    git_repository* raw = nullptr;
    if (git_repository_open_bare(&raw, location->string().c_str()) < 0)
        return false;
    GitRepository repo(raw);
    return git_graph_descendant_of(repo.get(), &tip, &target) == 1;
}

void
CircleModule::revalidate()
{
    ++pimpl_->generation;
    std::vector<std::shared_ptr<CircleSession>> sessions;
    {
        std::lock_guard lock(pimpl_->mutex);
        std::erase_if(pimpl_->sessions, [](const auto& session) { return session.expired(); });
        for (const auto& session : pimpl_->sessions) {
            if (auto active = session.lock())
                sessions.push_back(std::move(active));
        }
    }
    for (const auto& session : sessions)
        session->authorized();
}

std::shared_ptr<CircleRepository>
CircleModule::circle(std::string_view id) const
{
    std::lock_guard lock(pimpl_->mutex);
    const auto found = pimpl_->circles.find(id);
    return found == pimpl_->circles.end() ? nullptr : found->second.repository;
}

std::optional<std::string>
CircleModule::appendChannelMessage(std::string_view channel, const CommitMessage& message)
{
    Impl::Channel entry;
    std::shared_ptr<CircleRepository> repository;
    {
        std::lock_guard lock(pimpl_->mutex);
        const auto found = pimpl_->channels.find(channel);
        if (found == pimpl_->channels.end())
            return std::nullopt;
        entry = found->second;
        const auto circle = pimpl_->circles.find(entry.circle);
        if (circle == pimpl_->circles.end() || !circle->second.repository)
            return std::nullopt;
        repository = circle->second.repository;
    }
    auto head = repository->appendChannelMessage(entry.path, channel, pimpl_->identity, message);
    if (!head)
        return std::nullopt;
    {
        std::lock_guard lock(pimpl_->mutex);
        const auto found = pimpl_->channels.find(channel);
        if (found != pimpl_->channels.end() && found->second.path == entry.path && found->second.circle == entry.circle)
            found->second.head = *head;
    }
    ++pimpl_->generation;
    return head;
}

CircleFetchResult
CircleModule::fetch(std::string_view route, const Download& download)
{
    CircleFetchResult result;
    try {
        const auto parsed = parseRoute(route);
        if (!parsed || !download)
            return result;
        std::filesystem::path path;
        std::shared_ptr<CircleRepository> repository;
        std::string previous;
        {
            std::lock_guard lock(pimpl_->mutex);
            std::string circle = parsed->id;
            if (!parsed->circle) {
                const auto found = pimpl_->channels.find(parsed->id);
                if (found == pimpl_->channels.end())
                    return result;
                path = found->second.path;
                previous = found->second.head;
                circle = found->second.circle;
            }
            const auto found = pimpl_->circles.find(circle);
            if (found == pimpl_->circles.end() || !found->second.repository || found->second.departing)
                return result;
            repository = found->second.repository;
            if (parsed->circle)
                path = found->second.path;
        }
        if (parsed->circle)
            previous = repository->head();
        const auto head = download(path, INCOMING_REF);
        if (head && parsed->circle) {
            auto accepted = repository->fastForward(*head);
            if (!accepted)
                accepted = repository->merge(pimpl_->identity, *head);
            if (accepted) {
                result.status = *accepted == previous ? CircleFetchStatus::UNCHANGED : CircleFetchStatus::UPDATED;
                result.head = *accepted;
            }
        } else if (head) {
            auto channel = repository->fastForwardChannel(path, parsed->id, *head);
            if (channel.status == ChannelValidationStatus::DIVERGED)
                channel = repository->mergeChannel(path, parsed->id, pimpl_->identity, *head);
            if (channel.status == ChannelValidationStatus::VALID) {
                result.status = channel.head == previous ? CircleFetchStatus::UNCHANGED : CircleFetchStatus::UPDATED;
                result.head = channel.head;
                std::lock_guard lock(pimpl_->mutex);
                const auto found = pimpl_->channels.find(parsed->id);
                if (found != pimpl_->channels.end() && found->second.path == path)
                    found->second.head = channel.head;
            } else if (channel.status == ChannelValidationStatus::PENDING_DEPENDENCY) {
                result.status = CircleFetchStatus::PENDING_DEPENDENCY;
            }
            ++pimpl_->generation;
        }
        removeReference(path, INCOMING_REF);
        return result;
    } catch (const std::exception&) {
        return {};
    }
}

bool
CircleModule::clone(std::string_view route,
                    const std::filesystem::path& path,
                    std::string_view circleId,
                    const Download& download)
{
    try {
        const auto parsed = parseRoute(route);
        std::error_code error;
        if (!parsed || !download || (parsed->circle && parsed->id != circleId) || std::filesystem::exists(path, error))
            return false;
        auto staging = path;
        staging += ".clone";
        std::filesystem::remove_all(staging, error);
        if (!std::filesystem::create_directory(staging, error))
            return false;
        std::filesystem::permissions(staging,
                                     std::filesystem::perms::owner_all,
                                     std::filesystem::perm_options::replace,
                                     error);
        bool valid = false;
        git_repository* raw = nullptr;
        if (!error && git_repository_init(&raw, staging.string().c_str(), 1) == 0) {
            GitRepository repo(raw);
            valid = git_repository_set_head(repo.get(), "refs/heads/main") == 0;
        }
        valid = valid && download(staging, "refs/heads/main");
        if (valid && parsed->circle) {
            valid = CircleRepository::open(staging, circleId) != nullptr;
        } else if (valid) {
            const auto repository = circle(circleId);
            valid = repository
                    && repository->validateChannel(staging, parsed->id).status == ChannelValidationStatus::VALID;
        }
        if (valid) {
            std::filesystem::rename(staging, path, error);
            valid = !error;
        }
        if (!valid) {
            std::filesystem::remove_all(staging, error);
            return false;
        }
        if (parsed->circle ? mountCircle(path, circleId) : mountChannel(path, circleId, parsed->id))
            return true;
        std::filesystem::remove_all(path, error);
        return false;
    } catch (const std::exception&) {
        return false;
    }
}

bool
CircleModule::authorizeAny(const dht::crypto::Certificate& peer) const
{
    std::vector<std::string> circles;
    {
        std::lock_guard lock(pimpl_->mutex);
        for (const auto& [id, circle] : pimpl_->circles)
            circles.push_back(id);
    }
    return std::any_of(circles.begin(), circles.end(), [&](const auto& circle) {
        return authorize(circleRoute(circle), peer);
    });
}

bool
CircleModule::authorize(std::string_view route, const dht::crypto::Certificate& peer) const
{
    try {
        return pimpl_->resolve(route, peer).has_value();
    } catch (const std::exception&) {
        return false;
    }
}

std::shared_ptr<CircleSession>
CircleModule::open(std::string_view route,
                   std::shared_ptr<dht::crypto::Certificate> peer,
                   std::function<void()> shutdown)
{
    try {
        if (!peer || !peer->issuer)
            return {};
        auto cache = std::make_shared<Impl::CachedGrant>();
        cache->grant = pimpl_->resolve(route, *peer);
        const auto parsed = parseRoute(route);
        if (!cache->grant || !parsed)
            return {};
        auto session = std::make_shared<CircleSession>(
            cache->grant->path,
            parsed->id,
            std::string(route),
            peer->issuer->getId().toString(),
            peer->getLongId().toString(),
            [weak = std::weak_ptr<Impl>(pimpl_), route = std::string(route), peer, path = cache->grant->path, cache]()
                -> std::optional<std::string> {
                const auto shared = weak.lock();
                if (!shared)
                    return std::nullopt;
                std::lock_guard lock(cache->mutex);
                if (!cache->grant || !shared->fresh(*cache->grant)) {
                    cache->grant = shared->resolve(route, *peer);
                    if (cache->grant && cache->grant->path != path)
                        cache->grant.reset();
                }
                return cache->grant ? std::optional<std::string>(cache->grant->head) : std::nullopt;
            },
            std::move(shutdown));
        std::vector<std::shared_ptr<CircleSession>> live, replaced;
        bool accepted = false;
        {
            std::lock_guard lock(pimpl_->mutex);
            for (const auto& previous : pimpl_->sessions) {
                if (auto active = previous.lock())
                    live.push_back(std::move(active));
            }
            size_t own = 0;
            for (const auto& active : live) {
                // A reconnecting device replaces its own stale session for the same route.
                if (active->device == session->device && active->route == session->route)
                    replaced.push_back(active);
                else if (active->account == session->account)
                    ++own;
            }
            if (!pimpl_->stopped && own < MAX_ACCOUNT_SESSIONS && live.size() - replaced.size() < MAX_SESSIONS) {
                pimpl_->sessions.clear();
                for (const auto& active : live) {
                    if (std::find(replaced.begin(), replaced.end(), active) == replaced.end())
                        pimpl_->sessions.emplace_back(active);
                }
                pimpl_->sessions.emplace_back(session);
                accepted = true;
            } else {
                replaced.clear();
            }
        }
        for (const auto& previous : replaced)
            previous->close();
        return accepted && session->authorized() ? session : nullptr;
    } catch (const std::exception&) {
        return {};
    }
}

} // namespace jami