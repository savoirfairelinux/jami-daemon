/*
 * Copyright (C) 2026 Savoir-faire Linux Inc.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <msgpack.hpp>

#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>

namespace jami {

struct MetadataEntry
{
    int64_t timestampMs {0};
    std::string writer; // stable replica identifier (64 lowercase hexadecimal characters)
    std::string value;

    auto stamp() const { return std::tie(timestampMs, writer); }
    bool operator==(const MetadataEntry&) const = default;
    MSGPACK_DEFINE_MAP(timestampMs, writer, value)
};

struct Metadata
{
    std::map<std::string, MetadataEntry> entries;
    bool operator==(const Metadata&) const = default;
    MSGPACK_DEFINE_MAP(entries)
};

/** Persistent, path-scoped LWW registers. Omitted keys are never removed. */
class MetadataStore
{
public:
    using Values = std::map<std::string, std::string>;

    static constexpr size_t MAX_KEY_BYTES = 1024;
    static constexpr size_t MAX_VALUE_BYTES = 65536;
    static constexpr size_t MAX_ENTRIES = 16384;
    static constexpr size_t MAX_STATE_BYTES = 4 * 1024 * 1024;

    struct Change
    {
        bool changed {false}; // includes stamp-only changes, which must be forwarded
        std::optional<Values> snapshot; // present only when values changed
    };

    explicit MetadataStore(std::filesystem::path path)
        : path_(std::move(path))
    {}

    // nullopt indicates failure; successful mutations may report no change.
    [[nodiscard]] std::optional<Metadata> state() const;
    [[nodiscard]] std::optional<Values> values() const;
    [[nodiscard]] std::optional<Change> update(const std::string& writer, const Values& updates);
    [[nodiscard]] std::optional<Change> merge(const Metadata& remote);

    [[nodiscard]] static bool validate(const Metadata& state);
    [[nodiscard]] static std::optional<Metadata> decode(std::string_view packed);

private:
    bool load() const; // mutex_ held; corrupt/unreadable files are never treated as empty
    std::optional<Change> commit(Metadata&& next, bool valuesChanged);
    bool save(const Metadata& next) const;

    const std::filesystem::path path_;
    mutable std::mutex mutex_;
    mutable bool loaded_ {false};
    mutable Metadata state_;
};

} // namespace jami
