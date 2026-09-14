/*
 * Copyright (C) 2026 Savoir-faire Linux Inc.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <msgpack.hpp>

#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <tuple>

namespace jami {

struct AccountMetadataEntry
{
    uint64_t revision {0};
    std::string writer;
    std::string value;

    auto stamp() const { return std::tie(revision, writer); }
    bool operator==(const AccountMetadataEntry&) const = default;
    MSGPACK_DEFINE_MAP(revision, writer, value)
};

struct AccountMetadata
{
    uint64_t clock {0};
    std::map<std::string, AccountMetadataEntry> entries;
    bool operator==(const AccountMetadata&) const = default;
    MSGPACK_DEFINE_MAP(clock, entries)
};

/** Account-private LWW registers. Omitted keys are never removed. */
class AccountMetadataStore
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

    explicit AccountMetadataStore(std::filesystem::path path) : path_(std::move(path)) {}

    AccountMetadata state() const;
    std::map<std::string, std::string> values() const;
    Change update(const std::string& writer, const std::map<std::string, std::string>& updates,
                  bool onlyIfAbsent = false);
    Change merge(const AccountMetadata& remote);

    static void validate(const AccountMetadata& state);
    static AccountMetadata decode(std::string_view packed);

private:
    void load() const; // mutex_ held; corrupt/unreadable files are never treated as empty
    Change commit(AccountMetadata&& next);
    void save(const AccountMetadata& next) const;

    const std::filesystem::path path_;
    mutable std::mutex mutex_;
    mutable bool loaded_ {false};
    mutable AccountMetadata state_;
};

} // namespace jami
