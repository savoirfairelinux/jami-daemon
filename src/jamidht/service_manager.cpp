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
#include "jamidht/service_manager.h"

#include "fileutils.h"
#include "json_utils.h"
#include "logger.h"

#include <algorithm>
#include <fstream>
#include <random>
#include <system_error>

namespace jami {

namespace {

constexpr const char* SERVICES_FILENAME = "exposed_services.json";

const char*
policyToString(AccessPolicy p)
{
    switch (p) {
    case AccessPolicy::CONTACTS_ONLY:    return "contacts";
    case AccessPolicy::SPECIFIC_CONTACTS: return "specific";
    case AccessPolicy::PUBLIC:           return "public";
    }
    return "contacts";
}

AccessPolicy
policyFromString(const std::string& s)
{
    if (s == "specific") return AccessPolicy::SPECIFIC_CONTACTS;
    if (s == "public")   return AccessPolicy::PUBLIC;
    return AccessPolicy::CONTACTS_ONLY;
}

Json::Value
toJson(const ServiceRecord& r)
{
    Json::Value v(Json::objectValue);
    v["id"] = r.id;
    v["name"] = r.name;
    v["description"] = r.description;
    v["localHost"] = r.localHost;
    v["localPort"] = static_cast<Json::UInt>(r.localPort);
    v["policy"] = policyToString(r.policy);
    Json::Value allowed(Json::arrayValue);
    for (const auto& a : r.allowedContacts)
        allowed.append(a);
    v["allowedContacts"] = std::move(allowed);
    v["enabled"] = r.enabled;
    return v;
}

bool
fromJson(const Json::Value& v, ServiceRecord& r)
{
    if (!v.isObject())
        return false;
    r.id = v.get("id", "").asString();
    r.name = v.get("name", "").asString();
    r.description = v.get("description", "").asString();
    r.localHost = v.get("localHost", "127.0.0.1").asString();
    r.localPort = static_cast<uint16_t>(v.get("localPort", 0).asUInt());
    r.policy = policyFromString(v.get("policy", "contacts").asString());
    r.allowedContacts.clear();
    if (v.isMember("allowedContacts") && v["allowedContacts"].isArray()) {
        for (const auto& a : v["allowedContacts"])
            r.allowedContacts.push_back(a.asString());
    }
    r.enabled = v.get("enabled", true).asBool();
    return !r.id.empty();
}

} // namespace

std::string
generateServiceUuid()
{
    // RFC 4122 v4 UUID using thread-local std::mt19937_64 seeded from std::random_device.
    static thread_local std::mt19937_64 rng {std::random_device {}()};
    std::uniform_int_distribution<uint64_t> dist;
    uint64_t a = dist(rng);
    uint64_t b = dist(rng);
    // Set version (0100) and variant (10).
    a = (a & 0xffffffffffff0fffULL) | 0x0000000000004000ULL;
    b = (b & 0x3fffffffffffffffULL) | 0x8000000000000000ULL;
    char buf[37];
    std::snprintf(buf,
                  sizeof(buf),
                  "%08x-%04x-%04x-%04x-%012llx",
                  static_cast<unsigned>((a >> 32) & 0xffffffffULL),
                  static_cast<unsigned>((a >> 16) & 0xffffULL),
                  static_cast<unsigned>(a & 0xffffULL),
                  static_cast<unsigned>((b >> 48) & 0xffffULL),
                  static_cast<unsigned long long>(b & 0xffffffffffffULL));
    return std::string(buf, 36);
}

ServiceManager::ServiceManager(std::filesystem::path storagePath)
    : storagePath_(std::move(storagePath))
{
    std::unique_lock lk(mutex_);
    loadLocked();
}

std::filesystem::path
ServiceManager::filePath() const
{
    return storagePath_ / SERVICES_FILENAME;
}

void
ServiceManager::loadLocked()
{
    services_.clear();
    auto path = storagePath_ / SERVICES_FILENAME;
    std::error_code ec;
    if (!std::filesystem::exists(path, ec))
        return;
    std::ifstream in(path);
    if (!in)
        return;
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    Json::Value root;
    if (!json::parse(content, root) || !root.isArray())
        return;
    for (const auto& v : root) {
        ServiceRecord r;
        if (fromJson(v, r))
            services_.emplace(r.id, std::move(r));
    }
}

void
ServiceManager::saveLocked() const
{
    std::error_code ec;
    std::filesystem::create_directories(storagePath_, ec);
    Json::Value root(Json::arrayValue);
    for (const auto& [_id, r] : services_)
        root.append(toJson(r));
    auto path = storagePath_ / SERVICES_FILENAME;
    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        JAMI_WARNING("[ServiceManager] Unable to write {}", path.string());
        return;
    }
    out << json::toString(root);
}

std::string
ServiceManager::addService(ServiceRecord rec)
{
    if (rec.name.empty() || rec.localPort == 0)
        return {};
    if (rec.id.empty())
        rec.id = generateServiceUuid();
    std::unique_lock lk(mutex_);
    auto id = rec.id;
    services_[id] = std::move(rec);
    saveLocked();
    const auto& stored = services_[id];
    JAMI_LOG("[ServiceManager] added service id={} name=\"{}\" target={}:{} enabled={}",
             id,
             stored.name,
             stored.localHost,
             stored.localPort,
             stored.enabled);
    return id;
}

bool
ServiceManager::updateService(const ServiceRecord& rec)
{
    if (rec.id.empty() || rec.name.empty() || rec.localPort == 0)
        return false;
    std::unique_lock lk(mutex_);
    auto it = services_.find(rec.id);
    if (it == services_.end())
        return false;
    bool wasEnabled = it->second.enabled;
    it->second = rec;
    saveLocked();
    if (wasEnabled != rec.enabled)
        JAMI_LOG("[ServiceManager] service id={} name=\"{}\" {}",
                 rec.id,
                 rec.name,
                 rec.enabled ? "enabled" : "disabled");
    else
        JAMI_LOG("[ServiceManager] updated service id={} name=\"{}\" target={}:{}",
                 rec.id,
                 rec.name,
                 rec.localHost,
                 rec.localPort);
    return true;
}

bool
ServiceManager::removeService(const std::string& id)
{
    std::unique_lock lk(mutex_);
    auto erased = services_.erase(id) > 0;
    if (erased) {
        saveLocked();
        JAMI_LOG("[ServiceManager] removed service id={}", id);
    }
    return erased;
}

std::vector<ServiceRecord>
ServiceManager::getServices() const
{
    std::shared_lock lk(mutex_);
    std::vector<ServiceRecord> out;
    out.reserve(services_.size());
    for (const auto& [_id, r] : services_)
        out.push_back(r);
    return out;
}

std::optional<ServiceRecord>
ServiceManager::getService(const std::string& id) const
{
    std::shared_lock lk(mutex_);
    auto it = services_.find(id);
    if (it == services_.end())
        return std::nullopt;
    return it->second;
}

bool
ServiceManager::isAuthorizedNoLock(const ServiceRecord& rec,
                                   const std::string& peerAccountUri,
                                   const ContactChecker& isContact)
{
    if (!rec.enabled)
        return false;
    switch (rec.policy) {
    case AccessPolicy::PUBLIC:
        return true;
    case AccessPolicy::CONTACTS_ONLY:
        return isContact && isContact(peerAccountUri);
    case AccessPolicy::SPECIFIC_CONTACTS:
        return std::find(rec.allowedContacts.begin(), rec.allowedContacts.end(), peerAccountUri)
               != rec.allowedContacts.end();
    }
    return false;
}

bool
ServiceManager::isAuthorized(const std::string& serviceId,
                             const std::string& peerAccountUri,
                             const ContactChecker& isContact) const
{
    std::shared_lock lk(mutex_);
    auto it = services_.find(serviceId);
    if (it == services_.end())
        return false;
    return isAuthorizedNoLock(it->second, peerAccountUri, isContact);
}

std::vector<ServiceRecord>
ServiceManager::getVisibleServices(const std::string& peerAccountUri,
                                   const ContactChecker& isContact) const
{
    std::shared_lock lk(mutex_);
    std::vector<ServiceRecord> out;
    out.reserve(services_.size());
    for (const auto& [_id, r] : services_) {
        if (isAuthorizedNoLock(r, peerAccountUri, isContact))
            out.push_back(r);
    }
    return out;
}

} // namespace jami
