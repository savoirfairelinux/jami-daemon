/*
 * Copyright (C) 2026 Savoir-faire Linux Inc.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "account_metadata.h"
#include "connectivity/utf8_utils.h"

#include <algorithm>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace jami {
namespace {

bool
validWriter(const std::string& writer)
{
    return writer.size() == 64 && std::all_of(writer.begin(), writer.end(), [](unsigned char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
           });
}

void
validateValue(const std::string& key, const std::string& value)
{
    if (key.empty() || key.size() > AccountMetadataStore::MAX_KEY_BYTES
        || value.size() > AccountMetadataStore::MAX_VALUE_BYTES || !utf8_validate(key) || !utf8_validate(value)
        || key.find('\0') != std::string::npos || value.find('\0') != std::string::npos)
        throw std::invalid_argument("Invalid account metadata key or value");
}

std::map<std::string, std::string>
snapshot(const AccountMetadata& state)
{
    std::map<std::string, std::string> result;
    for (const auto& [key, entry] : state.entries)
        result.emplace(key, entry.value);
    return result;
}

} // namespace

AccountMetadata
AccountMetadataStore::decode(std::string_view packed)
{
    if (packed.size() > MAX_STATE_BYTES + 128)
        throw std::runtime_error("Account metadata is too large");
    size_t offset = 0;
    auto object = msgpack::unpack(packed.data(), packed.size(), offset);
    if (offset != packed.size())
        throw std::runtime_error("Trailing account metadata data");
    auto field = [](const msgpack::object& map, std::string_view key) -> const msgpack::object& {
        if (map.type == msgpack::type::MAP)
            for (uint32_t i = 0; i < map.via.map.size; ++i) {
                const auto& item = map.via.map.ptr[i];
                if (item.key.type == msgpack::type::STR
                    && std::string_view(item.key.via.str.ptr, item.key.via.str.size) == key)
                    return item.val;
            }
        throw std::runtime_error("Missing account metadata field");
    };
    field(object.get(), "clock").as<uint64_t>();
    const auto& entries = field(object.get(), "entries");
    if (entries.type != msgpack::type::MAP || entries.via.map.size > MAX_ENTRIES)
        throw std::runtime_error("Invalid account metadata entries");
    for (uint32_t i = 0; i < entries.via.map.size; ++i) {
        const auto& entry = entries.via.map.ptr[i].val;
        field(entry, "revision").as<uint64_t>();
        field(entry, "writer").as<std::string>();
        field(entry, "value").as<std::string>();
    }
    AccountMetadata state;
    object.get().convert(state);
    validate(state);
    return state;
}

void
AccountMetadataStore::validate(const AccountMetadata& state)
{
    if (state.clock == std::numeric_limits<uint64_t>::max())
        throw std::invalid_argument("Account metadata clock cannot advance");
    if (state.entries.size() > MAX_ENTRIES)
        throw std::invalid_argument("Too many account metadata keys");
    size_t bytes = 0;
    for (const auto& [key, entry] : state.entries) {
        validateValue(key, entry.value);
        if (!entry.revision || entry.revision > state.clock || !validWriter(entry.writer))
            throw std::invalid_argument("Invalid account metadata stamp");
        bytes += key.size() + entry.value.size() + entry.writer.size() + 64;
        if (bytes > MAX_STATE_BYTES)
            throw std::invalid_argument("Account metadata is too large");
    }
}

void
AccountMetadataStore::load() const
{
    if (loaded_)
        return;
    if (!std::filesystem::exists(path_)) {
        loaded_ = true;
        return;
    }
    const auto size = std::filesystem::file_size(path_);
    if (!size || size > MAX_STATE_BYTES + 128)
        throw std::runtime_error("Invalid account metadata file size");
    std::ifstream file(path_, std::ios::binary);
    file.exceptions(std::ios::failbit | std::ios::badbit);
    std::string data(size, '\0');
    file.read(data.data(), data.size());
    state_ = decode(data);
    loaded_ = true;
}

AccountMetadata
AccountMetadataStore::state() const
{
    std::lock_guard lock(mutex_);
    load();
    return state_;
}

std::map<std::string, std::string>
AccountMetadataStore::values() const
{
    return snapshot(state());
}

AccountMetadataStore::Change
AccountMetadataStore::update(const std::string& writer,
                             const std::map<std::string, std::string>& updates,
                             bool onlyIfAbsent)
{
    if (!validWriter(writer) || updates.size() > MAX_ENTRIES)
        throw std::invalid_argument("Invalid account metadata update");
    for (const auto& [key, value] : updates)
        validateValue(key, value);
    std::lock_guard lock(mutex_);
    load();
    auto next = state_;
    for (const auto& [key, value] : updates) {
        auto it = next.entries.find(key);
        if (it != next.entries.end() && (onlyIfAbsent || it->second.value == value))
            continue;
        if (next.clock == std::numeric_limits<uint64_t>::max())
            throw std::overflow_error("Account metadata logical clock exhausted");
        next.entries[key] = {++next.clock, writer, value};
    }
    return commit(std::move(next));
}

AccountMetadataStore::Change
AccountMetadataStore::merge(const AccountMetadata& remote)
{
    validate(remote);
    std::lock_guard lock(mutex_);
    load();
    if (remote.clock > state_.clock && remote.clock - state_.clock > MAX_REMOTE_CLOCK_ADVANCE)
        throw std::invalid_argument("Account metadata clock advance is too large");
    auto next = state_;
    next.clock = std::max(next.clock, remote.clock);
    for (const auto& [key, entry] : remote.entries) {
        auto it = next.entries.find(key);
        if (it == next.entries.end() || it->second.stamp() < entry.stamp())
            next.entries[key] = entry;
        else if (it->second.stamp() == entry.stamp() && it->second.value != entry.value)
            throw std::invalid_argument("Conflicting account metadata values with identical stamp");
    }
    return commit(std::move(next));
}

AccountMetadataStore::Change
AccountMetadataStore::commit(AccountMetadata&& next)
{
    validate(next);
    Change result;
    result.changed = next.entries != state_.entries;
    result.snapshot = snapshot(next);
    result.valuesChanged = result.snapshot != snapshot(state_);
    if (next != state_) {
        save(next);
        state_ = std::move(next);
    }
    return result;
}

void
AccountMetadataStore::save(const AccountMetadata& next) const
{
    msgpack::sbuffer buffer;
    msgpack::pack(buffer, next);
    // A sibling staging file keeps replacement atomic, including across a crash.
    auto staging = path_;
    staging += ".new";
    try {
#ifdef _WIN32
        auto handle
            = CreateFileW(staging.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            throw std::system_error(GetLastError(), std::system_category(), "Opening account metadata");
        DWORD written = 0;
        bool ok = WriteFile(handle, buffer.data(), static_cast<DWORD>(buffer.size()), &written, nullptr)
                  && written == buffer.size() && FlushFileBuffers(handle);
        auto error = GetLastError();
        CloseHandle(handle);
        if (!ok)
            throw std::system_error(error, std::system_category(), "Writing account metadata");
        if (!MoveFileExW(staging.c_str(), path_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::system_error(GetLastError(), std::system_category(), "Replacing account metadata");
#else
        int fd = open(staging.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW, 0600);
        if (fd < 0)
            throw std::system_error(errno, std::generic_category(), "Opening account metadata");
        size_t offset = 0;
        while (offset < buffer.size()) {
            auto n = write(fd, buffer.data() + offset, buffer.size() - offset);
            if (n < 0 && errno == EINTR)
                continue;
            if (n <= 0) {
                auto error = errno;
                close(fd);
                throw std::system_error(error, std::generic_category(), "Writing account metadata");
            }
            offset += n;
        }
        auto error = fsync(fd) == 0 ? 0 : errno;
        if (close(fd) != 0 && !error)
            error = errno;
        if (error)
            throw std::system_error(error, std::generic_category(), "Flushing account metadata");
        std::filesystem::rename(staging, path_);
        int dir = open(path_.parent_path().c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (dir < 0)
            throw std::system_error(errno, std::generic_category(), "Opening metadata directory");
        error = fsync(dir) == 0 ? 0 : errno;
        close(dir);
        if (error)
            throw std::system_error(error, std::generic_category(), "Flushing metadata directory");
#endif
    } catch (...) {
        // A rename may already have succeeded. Reload before any subsequent operation,
        // but never report success or emit a notification for a failed durable write.
        loaded_ = false;
        std::error_code ec;
        std::filesystem::remove(staging, ec);
        throw;
    }
}

} // namespace jami
