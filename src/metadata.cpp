/*
 * Copyright (C) 2026 Savoir-faire Linux Inc.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "metadata.h"
#include "connectivity/utf8_utils.h"
#include "jamidht/timestamp.h"
#include "logger.h"

#include <algorithm>
#include <fstream>
#include <limits>
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

bool
validValue(const std::string& key, const std::string& value)
{
    return !key.empty() && key.size() <= MetadataStore::MAX_KEY_BYTES && value.size() <= MetadataStore::MAX_VALUE_BYTES
           && utf8_validate(key) && utf8_validate(value) && key.find('\0') == std::string::npos
           && value.find('\0') == std::string::npos;
}

MetadataStore::Values
snapshot(const Metadata& state)
{
    MetadataStore::Values result;
    for (const auto& [key, entry] : state.entries)
        result.emplace_hint(result.end(), key, entry.value);
    return result;
}

} // namespace

std::optional<Metadata>
MetadataStore::decode(std::string_view packed)
{
    if (packed.size() > MAX_STATE_BYTES + 128) {
        JAMI_ERROR("Metadata is too large");
        return std::nullopt;
    }
    auto field = [](const msgpack::object& map, std::string_view key) -> const msgpack::object* {
        if (map.type == msgpack::type::MAP)
            for (uint32_t i = 0; i < map.via.map.size; ++i) {
                const auto& item = map.via.map.ptr[i];
                if (item.key.type == msgpack::type::STR
                    && std::string_view(item.key.via.str.ptr, item.key.via.str.size) == key)
                    return &item.val;
            }
        return nullptr;
    };
    auto isString = [](const msgpack::object& value) {
        return value.type == msgpack::type::STR || value.type == msgpack::type::BIN;
    };
    try {
        size_t offset = 0;
        auto object = msgpack::unpack(packed.data(), packed.size(), offset);
        if (offset != packed.size()) {
            JAMI_ERROR("Trailing metadata data");
            return std::nullopt;
        }
        const auto* entries = field(object.get(), "entries");
        if (!entries || entries->type != msgpack::type::MAP || entries->via.map.size > MAX_ENTRIES) {
            JAMI_ERROR("Invalid metadata entries");
            return std::nullopt;
        }
        for (uint32_t i = 0; i < entries->via.map.size; ++i) {
            const auto& entry = entries->via.map.ptr[i].val;
            const auto* timestamp = field(entry, "timestampMs");
            const auto* writer = field(entry, "writer");
            const auto* value = field(entry, "value");
            if (!timestamp || !writer || !value) {
                JAMI_ERROR("Missing metadata field");
                return std::nullopt;
            }
            timestamp->as<int64_t>();
            if (!isString(*writer) || !isString(*value)) {
                JAMI_ERROR("Invalid metadata field type");
                return std::nullopt;
            }
        }
        Metadata state;
        object.get().convert(state);
        if (!validate(state))
            return std::nullopt;
        return state;
    } catch (const msgpack::unpack_error& e) {
        JAMI_ERROR("Cannot decode metadata: {}", e.what());
    } catch (const msgpack::type_error& e) {
        JAMI_ERROR("Invalid metadata field type: {}", e.what());
    }
    return std::nullopt;
}

bool
MetadataStore::validate(const Metadata& state)
{
    if (state.entries.size() > MAX_ENTRIES) {
        JAMI_ERROR("Too many metadata keys");
        return false;
    }
    size_t bytes = 0;
    for (const auto& [key, entry] : state.entries) {
        if (!validValue(key, entry.value) || entry.timestampMs <= 0 || !validWriter(entry.writer)) {
            JAMI_ERROR("Invalid metadata key, value, or stamp");
            return false;
        }
        bytes += key.size() + entry.value.size() + entry.writer.size() + 64;
        if (bytes > MAX_STATE_BYTES) {
            JAMI_ERROR("Metadata is too large");
            return false;
        }
    }
    return true;
}

bool
MetadataStore::load() const
{
    if (loaded_)
        return true;
    std::error_code ec;
    const auto exists = std::filesystem::exists(path_, ec);
    if (ec) {
        JAMI_ERROR("Cannot inspect metadata {}: {}", path_, ec.message());
        return false;
    }
    if (!exists) {
        loaded_ = true;
        return true;
    }
    const auto size = std::filesystem::file_size(path_, ec);
    if (ec) {
        JAMI_ERROR("Cannot read metadata file size at {}: {}", path_, ec.message());
        return false;
    }
    if (!size || size > MAX_STATE_BYTES + 128) {
        JAMI_ERROR("Invalid metadata file size at {}: {}", path_, size);
        return false;
    }
    std::ifstream file(path_, std::ios::binary);
    std::string data(size, '\0');
    if (!file.read(data.data(), data.size())) {
        JAMI_ERROR("Cannot read metadata {}", path_);
        return false;
    }
    auto state = decode(data);
    if (!state)
        return false;
    state_ = std::move(*state);
    loaded_ = true;
    return true;
}

std::optional<Metadata>
MetadataStore::state() const
{
    std::lock_guard lock(mutex_);
    if (!load())
        return std::nullopt;
    return state_;
}

std::optional<MetadataStore::Values>
MetadataStore::values() const
{
    std::lock_guard lock(mutex_);
    if (!load())
        return std::nullopt;
    return snapshot(state_);
}

std::optional<MetadataStore::Change>
MetadataStore::update(const std::string& writer, const Values& updates)
{
    if (!validWriter(writer) || updates.size() > MAX_ENTRIES) {
        JAMI_ERROR("Invalid metadata update");
        return std::nullopt;
    }
    for (const auto& [key, value] : updates)
        if (!validValue(key, value)) {
            JAMI_ERROR("Invalid metadata key or value");
            return std::nullopt;
        }
    std::lock_guard lock(mutex_);
    if (!load())
        return std::nullopt;
    auto next = state_;
    bool changed = false;
    const auto now = toMillisecondsSinceEpoch(nowMs());
    for (const auto& [key, value] : updates) {
        auto it = next.entries.find(key);
        if (it != next.entries.end() && it->second.value == value)
            continue;
        const auto previous = it != next.entries.end() ? it->second.timestampMs : 0;
        if (previous == std::numeric_limits<int64_t>::max()) {
            JAMI_ERROR("Metadata timestamp exhausted");
            return std::nullopt;
        }
        next.entries[key] = {std::max(now, previous + 1), writer, value};
        changed = true;
    }
    if (!changed)
        return Change {};
    return commit(std::move(next), true);
}

std::optional<MetadataStore::Change>
MetadataStore::merge(const Metadata& remote)
{
    if (!validate(remote))
        return std::nullopt;
    std::lock_guard lock(mutex_);
    if (!load())
        return std::nullopt;
    auto next = state_;
    bool changed = false;
    bool valuesChanged = false;
    for (const auto& [key, entry] : remote.entries) {
        auto it = next.entries.find(key);
        if (it == next.entries.end() || it->second.stamp() < entry.stamp()) {
            valuesChanged = valuesChanged || it == next.entries.end() || it->second.value != entry.value;
            next.entries[key] = entry;
            changed = true;
        } else if (it->second.stamp() == entry.stamp() && it->second.value != entry.value) {
            JAMI_ERROR("Conflicting metadata values with identical stamp");
            return std::nullopt;
        }
    }
    if (!changed)
        return Change {};
    return commit(std::move(next), valuesChanged);
}

std::optional<MetadataStore::Change>
MetadataStore::commit(Metadata&& next, bool valuesChanged)
{
    if (!validate(next))
        return std::nullopt;
    Change change;
    change.changed = true;
    if (valuesChanged)
        change.snapshot = snapshot(next);
    if (!save(next))
        return std::nullopt;
    state_ = std::move(next);
    return change;
}

bool
MetadataStore::save(const Metadata& next) const
{
    msgpack::sbuffer buffer;
    msgpack::pack(buffer, next);
    auto staging = path_;
    staging += ".new";
    auto cleanup = [&] {
        std::error_code ec;
        std::filesystem::remove(staging, ec);
        if (ec)
            JAMI_WARNING("Cannot remove metadata staging file {}: {}", staging, ec.message());
    };
    std::ofstream file(staging, std::ios::binary | std::ios::trunc);
    if (!file) {
        JAMI_ERROR("Cannot open metadata staging file {}", staging);
        cleanup();
        return false;
    }
    std::error_code ec;
    std::filesystem::permissions(staging, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write, ec);
    if (ec) {
        JAMI_ERROR("Cannot set metadata permissions at {}: {}", staging, ec.message());
        file.close();
        cleanup();
        return false;
    }
    file.write(buffer.data(), buffer.size());
    file.close();
    if (!file) {
        JAMI_ERROR("Cannot write metadata staging file {}", staging);
        cleanup();
        return false;
    }
    std::filesystem::rename(staging, path_, ec);
    if (ec) {
        JAMI_ERROR("Cannot replace metadata {}: {}", path_, ec.message());
        cleanup();
        return false;
    }
    return true;
}

} // namespace jami
