/*
 * Copyright (C) 2026 Savoir-faire Linux Inc.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "conversation_module.h"

#include <dhtnet/channel_socket.h>
#include <msgpack/v2/null_visitor.hpp>
#include <msgpack/v2/parse.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <system_error>
#include <vector>

namespace jami {

struct SyncMsgReadLimits
{
    // Safety envelope for every SyncMsg, including legacy contact/preference payloads.
    // The stricter metadata limits below are checked independently inside this envelope.
    static constexpr size_t MAX_WIRE_BYTES = 8 * 1024 * 1024;
    static constexpr size_t MAX_DEPTH = 32;
    static constexpr size_t MAX_OBJECTS = 16 * AccountMetadataStore::MAX_ENTRIES;
    static constexpr size_t MAX_CONTAINER_ENTRIES = MAX_OBJECTS;
};

namespace sync_msg_detail {

// Preflight never constructs a msgpack object tree or retains references into
// the parser's buffer. Container declarations are charged before their children
// arrive, including children of otherwise ignored/unknown fields.
class Preflight : public msgpack::v2::null_visitor
{
public:
    void init()
    {
        depth_ = 0;
        objects_ = 1;
        metadataEntries_ = 0;
        metadataBytes_ = 0;
    }

    bool visit_nil() { return scalar(); }
    bool visit_boolean(bool) { return scalar(); }
    bool visit_positive_integer(uint64_t) { return scalar(); }
    bool visit_negative_integer(int64_t) { return scalar(); }
    bool visit_float32(float) { return scalar(); }
    bool visit_float64(double) { return scalar(); }
    bool visit_ext(const char*, uint32_t) { return scalar(); }
    bool visit_str(const char* data, uint32_t size) { return bytes(data, size, true); }
    // std::string's msgpack adaptor also accepts BIN.
    bool visit_bin(const char* data, uint32_t size) { return bytes(data, size, false); }

    bool start_array(uint32_t count) { return container(count, false); }
    bool start_map(uint32_t count) { return container(count, true); }
    bool end_array()
    {
        --depth_;
        return true;
    }
    bool end_map()
    {
        --depth_;
        return true;
    }
    bool end_array_item()
    {
        ++frames_[depth_ - 1].index;
        return true;
    }
    bool start_map_key()
    {
        auto& frame = frames_[depth_ - 1];
        frame.key = true;
        frame.token = Token::Other;
        return true;
    }
    bool start_map_value()
    {
        frames_[depth_ - 1].key = false;
        return true;
    }

    void parse_error(size_t, size_t) { throw msgpack::parse_error("Invalid sync MessagePack"); }

private:
    enum class Role { Root, Metadata, Entries, Entry, Other };
    enum class Token { Am, Entries, Value, Writer, Other };
    struct Frame
    {
        Role role;
        bool map;
        bool key {false};
        size_t index {0};
        Token token {Token::Other};
    };

    Role nextRole() const
    {
        if (!depth_)
            return Role::Root;
        const auto& frame = frames_[depth_ - 1];
        if (frame.role == Role::Root && (frame.map ? !frame.key && frame.token == Token::Am : frame.index == 6))
            return Role::Metadata;
        if (frame.role == Role::Metadata && !frame.key && frame.token == Token::Entries)
            return Role::Entries;
        if (frame.role == Role::Entries && !frame.key)
            return Role::Entry;
        return Role::Other;
    }

    bool schemaKey() const
    {
        if (!depth_)
            return false;
        const auto& frame = frames_[depth_ - 1];
        return frame.map && frame.key
               && (frame.role == Role::Root || frame.role == Role::Metadata || frame.role == Role::Entry);
    }

    bool metadataString() const
    {
        if (!depth_)
            return false;
        const auto& frame = frames_[depth_ - 1];
        return (frame.role == Role::Entries && frame.key)
               || (frame.role == Role::Entry && !frame.key
                   && (frame.token == Token::Value || frame.token == Token::Writer));
    }

    bool scalar() const
    {
        if (nextRole() != Role::Other || schemaKey() || metadataString())
            throw msgpack::type_error();
        return true;
    }

    void accountBytes(size_t size)
    {
        if (size > AccountMetadataStore::MAX_STATE_BYTES - metadataBytes_)
            throw msgpack::size_overflow("Sync account metadata is too large");
        metadataBytes_ += size;
    }

    bool bytes(const char* data, uint32_t size, bool string)
    {
        if (nextRole() != Role::Other)
            throw msgpack::type_error();
        auto& frame = frames_[depth_ - 1];
        if (schemaKey()) {
            if (!string)
                throw msgpack::type_error();
            const std::string_view key(data, size);
            frame.token = key == "am"        ? Token::Am
                          : key == "entries" ? Token::Entries
                          : key == "value"   ? Token::Value
                          : key == "writer"  ? Token::Writer
                                             : Token::Other;
        } else if (frame.role == Role::Entries && frame.key) {
            if (!size || size > AccountMetadataStore::MAX_KEY_BYTES)
                throw msgpack::size_overflow("Invalid sync account metadata key size");
            accountBytes(size);
        } else if (frame.role == Role::Entry && !frame.key) {
            if (frame.token == Token::Value) {
                if (size > AccountMetadataStore::MAX_VALUE_BYTES)
                    throw msgpack::size_overflow("Sync account metadata value is too large");
                accountBytes(size);
            } else if (frame.token == Token::Writer) {
                if (size != 64)
                    throw msgpack::size_overflow("Invalid sync account metadata writer size");
                accountBytes(size);
            }
        }
        return true;
    }

    bool container(uint32_t count, bool map)
    {
        const auto role = nextRole();
        if (schemaKey() || metadataString() || (!map && role != Role::Root && role != Role::Other))
            throw msgpack::type_error();
        if (depth_ == SyncMsgReadLimits::MAX_DEPTH)
            throw msgpack::depth_size_overflow("Sync MessagePack nesting is too deep");
        const uint64_t children = uint64_t(count) * (map ? 2 : 1);
        if (count > SyncMsgReadLimits::MAX_CONTAINER_ENTRIES || children > SyncMsgReadLimits::MAX_OBJECTS - objects_)
            throw msgpack::size_overflow("Too many sync MessagePack objects");
        objects_ += static_cast<size_t>(children);
        if (role == Role::Entries) {
            // Charge duplicates too, rather than letting duplicate am/entries
            // fields bypass the bounds before the map adaptor selects a value.
            if (count > AccountMetadataStore::MAX_ENTRIES - metadataEntries_)
                throw msgpack::size_overflow("Too many sync account metadata entries");
            metadataEntries_ += count;
            accountBytes(size_t(count) * 64);
        }
        frames_[depth_++] = Frame {role, map};
        return true;
    }

    std::array<Frame, SyncMsgReadLimits::MAX_DEPTH> frames_ {};
    size_t depth_ {0};
    size_t objects_ {1};
    size_t metadataEntries_ {0};
    size_t metadataBytes_ {0};
};

struct UnreferencedBuffer
{
    void operator()(char*) const {}
};

class Parser : private UnreferencedBuffer, public msgpack::v2::parser<Parser, UnreferencedBuffer>, public Preflight
{
public:
    Parser()
        : msgpack::v2::parser<Parser, UnreferencedBuffer>(static_cast<UnreferencedBuffer&>(*this), 16384)
    {}

    Preflight& visitor() { return *this; }
};

class Reader
{
public:
    explicit Reader(std::function<std::error_code(SyncMsg&&)> cb)
        : cb_(std::move(cb))
    {}

    ssize_t receive(const uint8_t* data, size_t len)
    {
        if (rejected_ || len > static_cast<size_t>(std::numeric_limits<ssize_t>::max()) || (!data && len)) {
            rejected_ = true;
            return -1;
        }
        // All failures, including exceptions from the application callback,
        // leave the reader terminal. Only a successful receive clears this.
        rejected_ = true;
        auto remaining = len;
        for (;;) {
            std::optional<SyncMsg> message;
            try {
                message = next(data, remaining);
            } catch (const std::exception&) {
                return -1;
            }
            if (!message) {
                rejected_ = false;
                return static_cast<ssize_t>(len);
            }
            // Deliberately outside the protocol-error catch: application
            // exceptions must not be mistaken for successfully consumed input.
            if (auto ec = cb_(std::move(*message)); ec)
                return ec.value() > 0 ? -static_cast<ssize_t>(ec.value()) : -1;
        }
    }

private:
    std::optional<SyncMsg> next(const uint8_t*& data, size_t& remaining)
    {
        for (;;) {
            if (parser_.next()) {
                const auto size = parser_.parsed_size();
                const msgpack::unpack_limit limits(SyncMsgReadLimits::MAX_CONTAINER_ENTRIES,
                                                   SyncMsgReadLimits::MAX_CONTAINER_ENTRIES,
                                                   SyncMsgReadLimits::MAX_WIRE_BYTES,
                                                   SyncMsgReadLimits::MAX_WIRE_BYTES,
                                                   SyncMsgReadLimits::MAX_WIRE_BYTES,
                                                   SyncMsgReadLimits::MAX_DEPTH);
                auto object = msgpack::unpack(wire_.data() + offset_, size, nullptr, nullptr, limits);
                auto message = object.get().as<SyncMsg>();
                offset_ += size;
                parser_.reset();
                return message;
            }
            // Compact only when another chunk is needed, not after each small
            // object in a coalesced receive.
            if (offset_) {
                wire_.erase(wire_.begin(), wire_.begin() + offset_);
                offset_ = 0;
            }
            const auto room = SyncMsgReadLimits::MAX_WIRE_BYTES - wire_.size();
            if (!room)
                throw msgpack::size_overflow("Sync MessagePack exceeds the wire limit");
            if (!remaining)
                return std::nullopt;
            const auto count = std::min({remaining, room, size_t {16384}});
            const auto size = wire_.size() + count;
            // Bound both reassembly buffers before reserve/copy. Feeding small
            // chunks lets the visitor reject hostile declarations promptly.
            if (size > wire_.capacity())
                wire_.reserve(std::min(SyncMsgReadLimits::MAX_WIRE_BYTES, std::max(size, wire_.capacity() * 2)));
            parser_.reserve_buffer(count);
            wire_.insert(wire_.end(), data, data + count);
            std::memcpy(parser_.buffer(), data, count);
            parser_.buffer_consumed(count);
            data += count;
            remaining -= count;
        }
    }

    std::function<std::error_code(SyncMsg&&)> cb_;
    Parser parser_;
    std::vector<char> wire_;
    size_t offset_ {0};
    bool rejected_ {false};
};

} // namespace sync_msg_detail

inline dhtnet::ChannelSocket::RecvCb
buildSyncMsgReader(std::function<std::error_code(SyncMsg&&)> cb)
{
    return [reader = std::make_shared<sync_msg_detail::Reader>(std::move(cb))](const uint8_t* data,
                                                                               size_t len) -> ssize_t {
        return reader->receive(data, len);
    };
}

} // namespace jami
