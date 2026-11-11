/*
 * Copyright (C) 2026 Savoir-faire Linux Inc.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "metadata_store.h"
#include "logger.h"

#include <fstream>
#include <system_error>

namespace jami {

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
    if (!size || size > Metadata::MAX_STATE_BYTES + 128) {
        JAMI_ERROR("Invalid metadata file size at {}: {}", path_, size);
        return false;
    }
    std::ifstream file(path_, std::ios::binary);
    std::string data(size, '\0');
    if (!file.read(data.data(), data.size())) {
        JAMI_ERROR("Cannot read metadata {}", path_);
        return false;
    }
    auto state = Metadata::decode(data);
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

std::optional<Metadata::Values>
MetadataStore::values() const
{
    std::lock_guard lock(mutex_);
    if (!load())
        return std::nullopt;
    return state_.values();
}

std::optional<Metadata::Change>
MetadataStore::update(const std::string& writer, const Metadata::Values& updates)
{
    std::lock_guard lock(mutex_);
    if (!load())
        return std::nullopt;
    auto next = state_;
    auto change = next.update(writer, updates);
    if (!change || (change->changed && !save(next)))
        return std::nullopt;
    state_ = std::move(next);
    return change;
}

std::optional<Metadata::Change>
MetadataStore::merge(const Metadata& remote)
{
    std::lock_guard lock(mutex_);
    if (!load())
        return std::nullopt;
    auto next = state_;
    auto change = next.merge(remote);
    if (!change || (change->changed && !save(next)))
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
