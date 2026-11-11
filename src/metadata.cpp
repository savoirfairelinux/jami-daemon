/*
 * Copyright (C) 2026 Savoir-faire Linux Inc.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "metadata.h"
#include "jamidht/timestamp.h"
#include "logger.h"

#include <algorithm>
#include <limits>
#include <utility>

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
    return !key.empty() && key.size() <= Metadata::MAX_KEY_BYTES && value.size() <= Metadata::MAX_VALUE_BYTES;
}

} // namespace

Metadata::Values
Metadata::values() const
{
    Values result;
    for (const auto& [key, entry] : entries)
        result.emplace_hint(result.end(), key, entry.value);
    return result;
}

std::optional<Metadata>
Metadata::decode(std::string_view packed)
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
        if (!state.validate())
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
Metadata::validate() const
{
    if (entries.size() > MAX_ENTRIES) {
        JAMI_ERROR("Too many metadata keys");
        return false;
    }
    size_t bytes = 0;
    for (const auto& [key, entry] : entries) {
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

std::optional<Metadata::Change>
Metadata::update(const std::string& writer, const Values& updates)
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
    if (!validate())
        return std::nullopt;
    auto next = *this;
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
    return apply(std::move(next), true);
}

std::optional<Metadata::Change>
Metadata::merge(const Metadata& remote)
{
    if (!remote.validate() || !validate())
        return std::nullopt;
    auto next = *this;
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
    return apply(std::move(next), valuesChanged);
}

std::optional<Metadata::Change>
Metadata::apply(Metadata&& next, bool valuesChanged)
{
    if (!next.validate())
        return std::nullopt;
    Change change;
    change.changed = true;
    if (valuesChanged)
        change.snapshot = next.values();
    *this = std::move(next);
    return change;
}

} // namespace jami
