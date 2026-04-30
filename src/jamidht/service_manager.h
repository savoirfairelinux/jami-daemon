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
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <shared_mutex>
#include <string>
#include <vector>

namespace jami {

/**
 * Access control policy for an exposed service.
 */
enum class AccessPolicy : uint8_t {
    CONTACTS_ONLY = 0,  ///< Any confirmed contact of the host account
    SPECIFIC_CONTACTS,  ///< Only account URIs explicitly listed in allowedContacts
    PUBLIC,             ///< Any peer with a valid Jami certificate
};

/**
 * Stable description of a service that the host wishes to expose to peers
 * through Jami. The service is reachable on the host at localHost:localPort
 * (typically 127.0.0.1) and is identified to remote peers by an opaque UUID.
 */
struct ServiceRecord
{
    std::string id;                                ///< RFC 4122 v4 UUID
    std::string name;                              ///< Human-readable name
    std::string description;                       ///< Optional description
    std::string scheme;                            ///< Optional URI scheme hint (e.g. "http", "https"); empty means raw TCP
    std::string localHost {"127.0.0.1"};           ///< Local TCP host
    uint16_t localPort {0};                        ///< Local TCP port
    AccessPolicy policy {AccessPolicy::CONTACTS_ONLY};
    std::vector<std::string> allowedContacts;      ///< Account URIs (used when policy == SPECIFIC_CONTACTS)
    bool enabled {true};
};

/**
 * Manages the catalog of services that an account exposes to its peers.
 *
 * Persistence: services are stored as a JSON array on disk.
 * Thread-safety: all public methods are safe to call from multiple threads.
 *
 * The class is intentionally decoupled from ContactList: callers pass a
 * predicate (`ContactChecker`) that maps a peer account URI to a boolean
 * indicating whether the peer is a confirmed contact of the host.
 */
class ServiceManager
{
public:
    /// Predicate: returns true iff the given peer account URI is a confirmed contact.
    using ContactChecker = std::function<bool(const std::string& peerAccountUri)>;

    /**
     * @param storagePath  Directory where the service registry will be persisted.
     *                     The file `<storagePath>/exposed_services.json` is read
     *                     immediately if it already exists.
     */
    explicit ServiceManager(std::filesystem::path storagePath);

    /**
     * Add a new service. If `rec.id` is empty, a new UUID is generated and
     * assigned to the record. Returns the assigned service id, or an empty
     * string if the record is invalid (e.g. localPort == 0 or empty name).
     */
    std::string addService(ServiceRecord rec);

    /**
     * Update an existing service in-place. The record's `id` must match an
     * existing service. Returns true on success.
     */
    bool updateService(const ServiceRecord& rec);

    /// Remove a service by id. Returns true if a service was removed.
    bool removeService(const std::string& id);

    /// Snapshot of all registered services.
    std::vector<ServiceRecord> getServices() const;

    /// Lookup a single service by id.
    std::optional<ServiceRecord> getService(const std::string& id) const;

    /**
     * Check whether `peerAccountUri` is allowed to open a tunnel to the
     * service identified by `serviceId`.
     *
     * The service must exist and be enabled. Authorization rules:
     *  - PUBLIC            : always allowed
     *  - CONTACTS_ONLY     : allowed iff `isContact(peerAccountUri)` returns true
     *  - SPECIFIC_CONTACTS : allowed iff `peerAccountUri` is in `allowedContacts`
     */
    bool isAuthorized(const std::string& serviceId,
                      const std::string& peerAccountUri,
                      const ContactChecker& isContact) const;

    /**
     * Returns the subset of services visible to a given peer.
     * Disabled services are never visible. Authorization is evaluated using
     * the same rules as `isAuthorized`.
     */
    std::vector<ServiceRecord> getVisibleServices(const std::string& peerAccountUri,
                                                  const ContactChecker& isContact) const;

    /// Path of the JSON file used for persistence.
    std::filesystem::path filePath() const;

private:
    void loadLocked();
    void saveLocked() const;

    static bool isAuthorizedNoLock(const ServiceRecord& rec,
                                   const std::string& peerAccountUri,
                                   const ContactChecker& isContact);

    std::filesystem::path storagePath_;
    mutable std::shared_mutex mutex_;
    std::map<std::string, ServiceRecord> services_;
};

/// Generate an RFC 4122 v4 UUID using std::random_device.
std::string generateServiceUuid();

} // namespace jami
