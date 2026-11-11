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
    static constexpr size_t MAX_KEY_BYTES = 1024;
    static constexpr size_t MAX_VALUE_BYTES = 65536;
    static constexpr size_t MAX_ENTRIES = 16384;
    static constexpr size_t MAX_STATE_BYTES = 4 * 1024 * 1024;

    struct Change
    {
        bool changed {false}; // includes stamp-only changes, which must be forwarded
        bool valuesChanged {false};
        std::map<std::string, std::string> snapshot;
    };

    explicit MetadataStore(std::filesystem::path path)
        : path_(std::move(path))
    {}

    Metadata state() const;
    std::map<std::string, std::string> values() const;
    Change update(const std::string& writer, const std::map<std::string, std::string>& updates);
    Change merge(const Metadata& remote);

    static void validate(const Metadata& state);
    static Metadata decode(std::string_view packed);

private:
    void load() const; // mutex_ held; corrupt/unreadable files are never treated as empty
    Change commit(Metadata&& next);
    void save(const Metadata& next) const;

    const std::filesystem::path path_;
    mutable std::mutex mutex_;
    mutable bool loaded_ {false};
    mutable Metadata state_;
};

} // namespace jami
