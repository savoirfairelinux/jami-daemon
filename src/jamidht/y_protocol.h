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
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace jami {
namespace yprotocol {

using Bytes = std::vector<uint8_t>;

/**
 * The message types of the yjs protocols, as the rest of that ecosystem numbers
 * them. Keeping the numbering means a document edited here and a document edited
 * by anything else built on yjs speak the same thing.
 */
enum class Message : uint64_t {
    SYNC = 0,
    AWARENESS = 1,
};

/// Sub-types of a SYNC message.
enum class Sync : uint64_t {
    /// "here is what I already have": carries a state vector, expects STEP2.
    STEP1 = 0,
    /// "here is what you are missing": carries an update built against a state vector.
    STEP2 = 1,
    /// An update to merge, sent as it is produced.
    UPDATE = 2,
};

/**
 * Writer for the lib0 encoding the yjs protocols are framed with.
 *
 * Integers are variable-length: seven bits of payload per byte, least
 * significant group first, the high bit marking that another byte follows. A
 * small number therefore costs one byte, which is what makes the framing itself
 * almost free.
 */
class Encoder
{
public:
    void writeVarUint(uint64_t value);
    void writeVarUint8Array(const uint8_t* data, size_t size);
    void writeVarUint8Array(const Bytes& data) { writeVarUint8Array(data.data(), data.size()); }
    /// A length-prefixed UTF-8 string. The bytes are written as they are given.
    void writeVarString(std::string_view value);

    const Bytes& bytes() const { return out_; }
    Bytes take() { return std::move(out_); }

private:
    Bytes out_;
};

/**
 * Reader for the same encoding.
 *
 * Every read is fallible and every one of them is checked, because what is being
 * read comes from a peer: a truncated length or an integer that never terminates
 * has to end the parse, not the process.
 */
class Decoder
{
public:
    Decoder(const uint8_t* data, size_t size)
        : data_(data)
        , size_(size)
    {}
    explicit Decoder(const Bytes& data)
        : Decoder(data.data(), data.size())
    {}

    bool readVarUint(uint64_t& value);
    /// Reads a length-prefixed byte array. @p out points into the buffer being
    /// decoded, which must outlive it.
    bool readVarUint8Array(const uint8_t*& out, size_t& size);
    bool readVarBytes(Bytes& out);
    bool readVarString(std::string& out);

    bool atEnd() const { return pos_ >= size_; }

private:
    const uint8_t* data_;
    size_t size_;
    size_t pos_ {0};
};

/// One client's slice of an awareness update.
struct AwarenessEntry
{
    uint64_t clientId {0};
    /// Bumped by its owner on every change. An entry whose clock is not greater
    /// than the one already held is ignored, which is what keeps a message that
    /// took a longer route from resurrecting a state everyone has moved past.
    uint64_t clock {0};
    /// The client's state as a JSON document, or "null" for a client that is
    /// gone. Opaque here: its shape is the editors' agreement.
    std::string state;
};

/// Frame a message that carries what this replica already holds.
Bytes syncStep1(const Bytes& stateVector);
/// Frame a message that carries what a peer is missing.
Bytes syncStep2(const Bytes& update);
/// Frame a single update, for the real-time path.
Bytes syncUpdate(const Bytes& update);
/// Frame an awareness update.
Bytes awarenessMessage(const std::vector<AwarenessEntry>& entries);

/// The body of an awareness message, without the outer framing.
Bytes encodeAwarenessUpdate(const std::vector<AwarenessEntry>& entries);
/// @return false on anything malformed, leaving @p out unusable.
bool decodeAwarenessUpdate(const uint8_t* data, size_t size, std::vector<AwarenessEntry>& out);

} // namespace yprotocol
} // namespace jami
