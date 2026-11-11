/*
 * Copyright (C) 2026 Savoir-faire Linux Inc.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "metadata.h"

#include <filesystem>
#include <mutex>
#include <utility>

namespace jami {

/** Thread-safe, file-backed metadata. Mutations are saved before becoming visible. */
class MetadataStore
{
public:
    explicit MetadataStore(std::filesystem::path path)
        : path_(std::move(path))
    {}

    // nullopt indicates failure, including corrupt or unreadable storage.
    [[nodiscard]] std::optional<Metadata> state() const;
    [[nodiscard]] std::optional<Metadata::Values> values() const;
    [[nodiscard]] std::optional<Metadata::Change> update(const std::string& writer, const Metadata::Values& updates);
    [[nodiscard]] std::optional<Metadata::Change> merge(const Metadata& remote);

private:
    bool load() const; // mutex_ held
    bool save(const Metadata& next) const;

    const std::filesystem::path path_;
    mutable std::mutex mutex_;
    mutable bool loaded_ {false};
    mutable Metadata state_;
};

} // namespace jami
