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

#include <cstdint>
#include <memory>
#include <vector>

namespace jami {

/**
 * RAII C++ wrapper around the yrs (Y-CRDT) C FFI, holding one replica of a
 * shared document.
 *
 * The daemon deliberately knows nothing about what the document contains. It
 * never reads a branch, never names a shared type and never interprets an
 * update: it merges updates and re-encodes the merged state. What the document
 * @e is -- text, a rich-text tree, a map, an array -- is entirely the client's
 * business, which is what lets a client implement an editor for any type yrs
 * supports without changing anything here.
 *
 * The replica exists for two reasons only:
 *  - to hand a newly opened client the whole document as a @b single update,
 *    instead of the thousands of updates its history is made of;
 *  - to do the same for an arbitrary past checkpoint.
 *
 * Thread-safety: every public method that touches the yrs document is serialized
 * by an internal mutex. takeChanged() is the exception: it only reads and clears
 * an atomic flag, so it never blocks and may be called while another thread is
 * inside the document.
 */
class YrsDocument
{
public:
    using Bytes = std::vector<uint8_t>;

    /**
     * @param clientId Unique replica identifier (per device). Two replicas
     *                 sharing a clientId while editing corrupts the document.
     */
    explicit YrsDocument(uint64_t clientId);
    ~YrsDocument();

    YrsDocument(const YrsDocument&) = delete;
    YrsDocument& operator=(const YrsDocument&) = delete;

    /// Merge an update. Idempotent and order-independent, so replaying a whole
    /// history in any order converges. Returns false when the update is
    /// malformed, which is expected: updates come from peers.
    bool applyUpdate(const Bytes& update);

    /// The entire state as a single update, for a client that has seen nothing.
    Bytes encodeStateAsUpdate() const;

    /// What this replica knows, in the compact form updates are diffed against.
    Bytes encodeStateVector() const;

    /**
     * Everything this replica holds that @p stateVector does not account for.
     *
     * @warning Its size is @b not a way to tell whether anything is new. yrs
     * appends the whole deletion set, undiffed, so a diff that carries nothing
     * new still measures a few bytes on any document where a character was ever
     * erased -- which is to say, on every real document. Use takeChanged().
     */
    Bytes encodeDiff(const Bytes& stateVector) const;

    /// Whether the document moved since this was last called, and clears the
    /// flag. Answered by yrs itself, which only reports an update that brought
    /// something the replica did not already hold: applying a known update, or
    /// replaying a whole history a second time, leaves this false.
    bool takeChanged();

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace jami
