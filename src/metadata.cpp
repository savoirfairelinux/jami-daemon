/*
 * Copyright (C) 2026 Savoir-faire Linux Inc.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "metadata.h"
#include "connectivity/utf8_utils.h"
#include "jamidht/timestamp.h"

#include <algorithm>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <system_error>

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
    if (key.empty() || key.size() > MetadataStore::MAX_KEY_BYTES || value.size() > MetadataStore::MAX_VALUE_BYTES
        || !utf8_validate(key) || !utf8_validate(value) || key.find('\0') != std::string::npos
        || value.find('\0') != std::string::npos)
        throw std::invalid_argument("Invalid metadata key or value");
}

std::map<std::string, std::string>
snapshot(const Metadata& state)
{
    std::map<std::string, std::string> result;
    for (const auto& [key, entry] : state.entries)
        result.emplace(key, entry.value);
    return result;
}

} // namespace

Metadata
MetadataStore::decode(std::string_view packed)
{
    if (packed.size() > MAX_STATE_BYTES + 128)
        throw std::runtime_error("Metadata is too large");
    size_t offset = 0;
    auto object = msgpack::unpack(packed.data(), packed.size(), offset);
    if (offset != packed.size())
        throw std::runtime_error("Trailing metadata data");
    auto field = [](const msgpack::object& map, std::string_view key) -> const msgpack::object& {
        if (map.type == msgpack::type::MAP)
            for (uint32_t i = 0; i < map.via.map.size; ++i) {
                const auto& item = map.via.map.ptr[i];
                if (item.key.type == msgpack::type::STR
                    && std::string_view(item.key.via.str.ptr, item.key.via.str.size) == key)
                    return item.val;
            }
        throw std::runtime_error("Missing metadata field");
    };
    const auto& entries = field(object.get(), "entries");
    if (entries.type != msgpack::type::MAP || entries.via.map.size > MAX_ENTRIES)
        throw std::runtime_error("Invalid metadata entries");
    for (uint32_t i = 0; i < entries.via.map.size; ++i) {
        const auto& entry = entries.via.map.ptr[i].val;
        field(entry, "timestampMs").as<int64_t>();
        field(entry, "writer").as<std::string>();
        field(entry, "value").as<std::string>();
    }
    Metadata state;
    object.get().convert(state);
    validate(state);
    return state;
}

void
MetadataStore::validate(const Metadata& state)
{
    if (state.entries.size() > MAX_ENTRIES)
        throw std::invalid_argument("Too many metadata keys");
    size_t bytes = 0;
    for (const auto& [key, entry] : state.entries) {
        validateValue(key, entry.value);
        if (entry.timestampMs <= 0 || !validWriter(entry.writer))
            throw std::invalid_argument("Invalid metadata stamp");
        bytes += key.size() + entry.value.size() + entry.writer.size() + 64;
        if (bytes > MAX_STATE_BYTES)
            throw std::invalid_argument("Metadata is too large");
    }
}

void
MetadataStore::load() const
{
    if (loaded_)
        return;
    if (!std::filesystem::exists(path_)) {
        loaded_ = true;
        return;
    }
    const auto size = std::filesystem::file_size(path_);
    if (!size || size > MAX_STATE_BYTES + 128)
        throw std::runtime_error("Invalid metadata file size");
    std::ifstream file(path_, std::ios::binary);
    file.exceptions(std::ios::failbit | std::ios::badbit);
    std::string data(size, '\0');
    file.read(data.data(), data.size());
    state_ = decode(data);
    loaded_ = true;
}

Metadata
MetadataStore::state() const
{
    std::lock_guard lock(mutex_);
    load();
    return state_;
}

std::map<std::string, std::string>
MetadataStore::values() const
{
    return snapshot(state());
}

MetadataStore::Change
MetadataStore::update(const std::string& writer, const std::map<std::string, std::string>& updates)
{
    if (!validWriter(writer) || updates.size() > MAX_ENTRIES)
        throw std::invalid_argument("Invalid metadata update");
    for (const auto& [key, value] : updates)
        validateValue(key, value);
    std::lock_guard lock(mutex_);
    load();
    auto next = state_;
    const auto now = toMillisecondsSinceEpoch(nowMs());
    for (const auto& [key, value] : updates) {
        auto it = next.entries.find(key);
        if (it != next.entries.end() && it->second.value == value)
            continue;
        const auto previous = it != next.entries.end() ? it->second.timestampMs : 0;
        if (previous == std::numeric_limits<int64_t>::max())
            throw std::overflow_error("Metadata timestamp exhausted");
        next.entries[key] = {std::max(now, previous + 1), writer, value};
    }
    return commit(std::move(next));
}

MetadataStore::Change
MetadataStore::merge(const Metadata& remote)
{
    validate(remote);
    std::lock_guard lock(mutex_);
    load();
    auto next = state_;
    for (const auto& [key, entry] : remote.entries) {
        auto it = next.entries.find(key);
        if (it == next.entries.end() || it->second.stamp() < entry.stamp())
            next.entries[key] = entry;
        else if (it->second.stamp() == entry.stamp() && it->second.value != entry.value)
            throw std::invalid_argument("Conflicting metadata values with identical stamp");
    }
    return commit(std::move(next));
}

MetadataStore::Change
MetadataStore::commit(Metadata&& next)
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
MetadataStore::save(const Metadata& next) const
{
    msgpack::sbuffer buffer;
    msgpack::pack(buffer, next);
    auto staging = path_;
    staging += ".new";
    try {
        std::ofstream file(staging, std::ios::binary | std::ios::trunc);
        file.exceptions(std::ios::failbit | std::ios::badbit);
        std::filesystem::permissions(staging, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
        file.write(buffer.data(), buffer.size());
        file.close();
        std::filesystem::rename(staging, path_);
    } catch (...) {
        std::error_code ec;
        std::filesystem::remove(staging, ec);
        throw;
    }
}

} // namespace jami
