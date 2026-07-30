/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#include "jamidht/y_protocol.h"

namespace jami {
namespace yprotocol {

namespace {
constexpr uint8_t PAYLOAD_BITS {0x7f};
constexpr uint8_t CONTINUATION_BIT {0x80};
/// The largest integer lib0 accepts, and therefore the largest one anything else
/// in the yjs ecosystem can be relied upon to read back: those values travel
/// through JSON, where integers above 2^53 stop being exact. Emitting a larger
/// one would produce a message only this implementation could read.
constexpr uint64_t MAX_SAFE_INTEGER {(uint64_t(1) << 53) - 1};
} // namespace

void
Encoder::writeVarUint(uint64_t value)
{
    while (value > PAYLOAD_BITS) {
        out_.push_back(static_cast<uint8_t>(CONTINUATION_BIT | (value & PAYLOAD_BITS)));
        value >>= 7;
    }
    out_.push_back(static_cast<uint8_t>(value & PAYLOAD_BITS));
}

void
Encoder::writeVarUint8Array(const uint8_t* data, size_t size)
{
    writeVarUint(size);
    out_.insert(out_.end(), data, data + size);
}

void
Encoder::writeVarString(std::string_view value)
{
    writeVarUint(value.size());
    out_.insert(out_.end(), value.begin(), value.end());
}

bool
Decoder::readVarUint(uint64_t& value)
{
    uint64_t result = 0;
    unsigned shift = 0;
    while (pos_ < size_) {
        const uint8_t byte = data_[pos_++];
        // Past 53 bits the value is no longer one lib0 would have written, so it
        // is refused rather than wrapped into something plausible.
        if (shift > 53)
            return false;
        result |= static_cast<uint64_t>(byte & PAYLOAD_BITS) << shift;
        if ((byte & CONTINUATION_BIT) == 0) {
            if (result > MAX_SAFE_INTEGER)
                return false;
            value = result;
            return true;
        }
        shift += 7;
    }
    return false; // truncated: the last byte never cleared the continuation bit
}

bool
Decoder::readVarUint8Array(const uint8_t*& out, size_t& size)
{
    uint64_t length = 0;
    if (!readVarUint(length))
        return false;
    if (length > size_ - pos_)
        return false;
    out = data_ + pos_;
    size = static_cast<size_t>(length);
    pos_ += size;
    return true;
}

bool
Decoder::readVarBytes(Bytes& out)
{
    const uint8_t* data = nullptr;
    size_t size = 0;
    if (!readVarUint8Array(data, size))
        return false;
    out.assign(data, data + size);
    return true;
}

bool
Decoder::readVarString(std::string& out)
{
    const uint8_t* data = nullptr;
    size_t size = 0;
    if (!readVarUint8Array(data, size))
        return false;
    out.assign(reinterpret_cast<const char*>(data), size);
    return true;
}

Bytes
syncStep1(const Bytes& stateVector)
{
    Encoder e;
    e.writeVarUint(static_cast<uint64_t>(Message::SYNC));
    e.writeVarUint(static_cast<uint64_t>(Sync::STEP1));
    e.writeVarUint8Array(stateVector);
    return e.take();
}

Bytes
syncStep2(const Bytes& update)
{
    Encoder e;
    e.writeVarUint(static_cast<uint64_t>(Message::SYNC));
    e.writeVarUint(static_cast<uint64_t>(Sync::STEP2));
    e.writeVarUint8Array(update);
    return e.take();
}

Bytes
syncUpdate(const Bytes& update)
{
    Encoder e;
    e.writeVarUint(static_cast<uint64_t>(Message::SYNC));
    e.writeVarUint(static_cast<uint64_t>(Sync::UPDATE));
    e.writeVarUint8Array(update);
    return e.take();
}

Bytes
encodeAwarenessUpdate(const std::vector<AwarenessEntry>& entries)
{
    Encoder e;
    e.writeVarUint(entries.size());
    for (const auto& entry : entries) {
        e.writeVarUint(entry.clientId);
        e.writeVarUint(entry.clock);
        e.writeVarString(entry.state);
    }
    return e.take();
}

Bytes
awarenessMessage(const std::vector<AwarenessEntry>& entries)
{
    const auto body = encodeAwarenessUpdate(entries);
    Encoder e;
    e.writeVarUint(static_cast<uint64_t>(Message::AWARENESS));
    e.writeVarUint8Array(body);
    return e.take();
}

bool
decodeAwarenessUpdate(const uint8_t* data, size_t size, std::vector<AwarenessEntry>& out)
{
    Decoder d(data, size);
    uint64_t count = 0;
    if (!d.readVarUint(count))
        return false;
    // An entry is three fields, so it cannot be shorter than three bytes. A
    // count larger than that allows is a truncated or hostile message, and
    // reserving for it would allocate on a peer's say-so.
    if (count > size / 3)
        return false;
    out.clear();
    out.reserve(static_cast<size_t>(count));
    for (uint64_t i = 0; i < count; ++i) {
        AwarenessEntry entry;
        if (!d.readVarUint(entry.clientId) || !d.readVarUint(entry.clock) || !d.readVarString(entry.state))
            return false;
        out.push_back(std::move(entry));
    }
    // Bytes left over mean the sender framed the message with something other
    // than the count it declared. Reading it as a well-formed update would let
    // that go unnoticed.
    return d.atEnd();
}

} // namespace yprotocol
} // namespace jami
