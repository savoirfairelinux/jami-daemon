/*
 * Copyright (C) 2026 Savoir-faire Linux Inc.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <msgpack.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace jami {

struct MetadataEntry
{
    int64_t timestampMs {0};
    std::string writer; // stable replica identifier (64 lowercase hexadecimal characters)
    // MessagePack writes byte vectors as BIN and also accepts legacy STR values.
    std::vector<uint8_t> value;

    auto stamp() const { return std::tie(timestampMs, writer); }
    bool operator==(const MetadataEntry&) const = default;
    MSGPACK_DEFINE_MAP(timestampMs, writer, value)
};

/** In-memory LWW registers. Omitted keys are never removed.
 * Owners provide synchronization and persistence.
 */
struct Metadata
{
    using Values = std::map<std::string, std::vector<uint8_t>>;

    static constexpr size_t MAX_KEY_BYTES = 1024;
    static constexpr size_t MAX_VALUE_BYTES = 65536;
    // Keys are defined statically by clients, never derived from user data, so a small cap is enough.
    // It bounds the state without a total size limit, which two valid states could exceed when merged.
    static constexpr size_t MAX_ENTRIES = 64;
    // Upper bound on the MessagePack encoding of a valid state, to reject oversized input before decoding.
    static constexpr size_t MAX_ENCODED_BYTES = 128 + MAX_ENTRIES * (MAX_KEY_BYTES + MAX_VALUE_BYTES + 256);

    struct Change
    {
        bool changed {false};           // includes stamp-only changes, which must be forwarded
        std::optional<Values> snapshot; // present only when values changed
    };

    std::map<std::string, MetadataEntry> entries;
    bool operator==(const Metadata&) const = default;
    MSGPACK_DEFINE_MAP(entries)

    Values values() const;
    // Mutations are atomic on validation failure. Successful no-ops retain stamps.
    [[nodiscard]] std::optional<Change> update(const std::string& writer, const Values& updates);
    [[nodiscard]] std::optional<Change> merge(const Metadata& remote);
    [[nodiscard]] bool validate() const;
    [[nodiscard]] static std::optional<Metadata> decode(std::string_view packed);

private:
    std::optional<Change> apply(Metadata&& next, bool valuesChanged);
};

} // namespace jami
