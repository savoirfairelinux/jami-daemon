#pragma once

#include "circlerepository.h"

#include <atomic>
#include <chrono>
#include <functional>

namespace jami {

class LIBJAMI_TEST_EXPORT CircleSession
{
public:
    const std::filesystem::path repository;
    const std::string id;
    const std::string route;
    const std::string account;
    const std::string device;

    CircleSession(std::filesystem::path path,
                  std::string repositoryId,
                  std::string sessionRoute,
                  std::string peerAccount,
                  std::string peerDevice,
                  std::function<std::optional<std::string>()> grant,
                  std::function<void()> shutdown);
    ~CircleSession();
    // Verified head to serve while the peer is still allowed; closes the session otherwise.
    std::optional<std::string> grant();
    bool authorized() { return grant().has_value(); }
    void close();

private:
    std::function<std::optional<std::string>()> grant_;
    std::function<void()> shutdown_;
    std::atomic_bool closed_ {false};
};

enum class CircleFetchStatus { UPDATED, UNCHANGED, PENDING_DEPENDENCY, FAILED };

struct CircleFetchResult
{
    CircleFetchStatus status {CircleFetchStatus::FAILED};
    std::string head;
};

// circles/v1/<id> or channels/v1/<id>.
struct CircleRoute
{
    bool circle {true};
    std::string id;
};

class LIBJAMI_TEST_EXPORT CircleModule
{
public:
    static constexpr size_t MAX_SESSIONS = 256;
    static constexpr size_t MAX_ACCOUNT_SESSIONS = 16;
    static constexpr std::chrono::seconds GRANT_LIFETIME {30};
    static constexpr const char* INCOMING_REF = "refs/jami/incoming";
    using CertificatePolicy = std::function<bool(const dht::crypto::Certificate&)>;
    // Downloads a peer's main into ref of a local repository and returns the downloaded head.
    using Download = std::function<std::optional<std::string>(const std::filesystem::path&, std::string_view ref)>;

    CircleModule(dht::crypto::Identity identity,
                 CertificatePolicy allowed,
                 std::chrono::seconds grantLifetime = GRANT_LIFETIME);
    ~CircleModule();
    // A departing circle is one this device left: it is mounted only to serve its circle route.
    bool mountCircle(const std::filesystem::path& path, std::string_view circle, bool departing = false);
    bool mountChannel(const std::filesystem::path& path, std::string_view circle, std::string_view channel);
    bool refresh(std::string_view circle);
    void unmount(std::string_view circle);
    void unmountChannel(std::string_view channel);
    // Keeps serving the circle route after the local member left, and unmounts its channels.
    bool depart(std::string_view circle);
    void revalidate();
    // The mounted instance, which local writers must use so serving follows their changes.
    std::shared_ptr<CircleRepository> circle(std::string_view id) const;
    std::vector<std::string> circles() const;
    std::vector<std::string> channels(std::string_view circle) const;
    std::optional<std::filesystem::path> path(std::string_view route) const;
    std::optional<std::string> head(std::string_view route) const;
    // Whether commit is the accepted head of a mounted route or one of its ancestors.
    bool accepted(std::string_view route, std::string_view commit) const;
    std::optional<std::string> appendChannelMessage(std::string_view channel, const CommitMessage& message);
    // Accepts a peer head for a mounted route: fast-forward, or a signed merge when histories diverged.
    CircleFetchResult fetch(std::string_view route, const Download& download);
    // Downloads a circle or one of its channels into a new path, validates it in full, then mounts it.
    bool clone(std::string_view route,
               const std::filesystem::path& path,
               std::string_view circleId,
               const Download& download);
    bool authorize(std::string_view route, const dht::crypto::Certificate& peer) const;
    bool authorizeAny(const dht::crypto::Certificate& peer) const;
    std::shared_ptr<CircleSession> open(std::string_view route,
                                        std::shared_ptr<dht::crypto::Certificate> peer,
                                        std::function<void()> shutdown);
    static bool reserved(std::string_view route);
    static std::optional<CircleRoute> parseRoute(std::string_view route);
    static std::string circleRoute(std::string_view circle);
    static std::string channelRoute(std::string_view channel);

private:
    struct Impl;
    std::shared_ptr<Impl> pimpl_;
};

} // namespace jami