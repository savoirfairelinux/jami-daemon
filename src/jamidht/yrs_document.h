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
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace jami {

/**
 * RAII C++ wrapper around the yrs (Y-CRDT) C FFI for a single shared text
 * document. It encapsulates a YDoc holding one YText branch named "content"
 * and isolates every libyrs call.
 *
 * Text offsets and lengths are expressed in UTF-16 code units, matching the
 * string indexing used by the Qt, Android and iOS text editors.
 *
 * Thread-safety: every public method is serialized by an internal mutex.
 * Callbacks are invoked synchronously from within the method that caused the
 * mutation, while the lock is held, so they must not call back into the same
 * YrsDocument instance.
 */
class YrsDocument
{
public:
    using Bytes = std::vector<uint8_t>;

    /**
     * A single text change, expressed so it can be replayed verbatim on a UI
     * text buffer: at @c index, remove @c deleteLen UTF-16 code units then
     * insert @c inserted. A change set is applied in order.
     */
    struct TextChange
    {
        uint32_t index;
        uint32_t deleteLen;
        std::string inserted;
    };

    /**
     * Invoked after every committed mutation with the binary Y-CRDT update
     * (lib0 v1 encoding). @c isLocal is true when the mutation originates from
     * a local insert()/remove(), false when it comes from applyUpdate().
     */
    using UpdateCallback = std::function<void(const Bytes& update, bool isLocal)>;

    /**
     * Invoked after every committed mutation with the resulting text changes.
     * @c isLocal mirrors UpdateCallback: clients should apply only remote
     * (isLocal == false) changes, since local edits already live in their UI.
     */
    using ChangeCallback = std::function<void(const std::vector<TextChange>& changes, bool isLocal)>;

    /**
     * Invoked after the document's name changes. @c isLocal mirrors the other
     * callbacks. The name is a CRDT field too, so it syncs and persists like the
     * content.
     */
    using NameCallback = std::function<void(const std::string& name, bool isLocal)>;

    /**
     * A rich-text operation in Quill-delta form. Used to report committed changes
     * to the UI (retain/insert/delete carrying formatting attributes) and, in
     * reverse, to apply UI edits through applyDelta(). Lengths and offsets are in
     * UTF-16 code units, matching the clients' text editors.
     *
     * @c attrs is a JSON object string of formatting attributes (e.g.
     * {"b":true,"header":2}); an empty string means "no attributes". For a Retain
     * op, a non-empty @c attrs formats the retained range (a null attribute value
     * removes that attribute, following the Quill convention).
     */
    struct RichOp
    {
        enum class Kind { Retain, Insert, Delete };
        Kind kind {Kind::Retain};
        uint32_t len {0};  ///< retained/deleted length, or UTF-16 length of @c text
        std::string text;  ///< inserted text (Insert only)
        std::string attrs; ///< JSON object of formatting attributes (may be empty)
    };

    /**
     * Invoked after every committed mutation with the resulting rich-text delta
     * (a sequence of retain/insert/delete ops carrying formatting attributes).
     * @c isLocal mirrors the other callbacks. Clients replay remote
     * (isLocal == false) ops onto their editor to converge formatting too.
     */
    using RichChangeCallback
        = std::function<void(const std::vector<RichOp>& ops, bool isLocal)>;

    /**
     * @param clientId Unique replica identifier (per device). Two replicas
     *                 sharing a clientId while editing corrupts the document.
     */
    explicit YrsDocument(uint64_t clientId);
    ~YrsDocument();

    YrsDocument(const YrsDocument&) = delete;
    YrsDocument& operator=(const YrsDocument&) = delete;

    /// Callbacks should be set once, before any mutation.
    void setUpdateCallback(UpdateCallback cb);
    void setChangeCallback(ChangeCallback cb);
    void setNameCallback(NameCallback cb);
    void setRichChangeCallback(RichChangeCallback cb);

    /// Insert @c utf8Text at @c index (UTF-16 code unit offset).
    void insert(uint32_t index, const std::string& utf8Text);
    /// Remove @c length UTF-16 code units starting at @c index.
    void remove(uint32_t index, uint32_t length);

    /// Apply a local rich-text edit expressed as a Quill-style delta (an ordered
    /// sequence of retain/insert/delete ops carrying formatting attributes). The
    /// whole delta is applied in a single transaction, so it broadcasts and
    /// persists as one atomic change.
    void applyDelta(const std::vector<RichOp>& ops);

    /// Whole current content as a Quill delta: a JSON array of {insert, attributes}
    /// runs. Used to render the initial document state in a rich editor.
    std::string contentDelta() const;

    /// Replace the document's name (a CRDT field shared with all members).
    void setName(const std::string& utf8Name);
    /// Current document name (UTF-8).
    std::string name() const;

    /// Merge a remote Y-CRDT update. Idempotent and order-independent. When @p silent
    /// is true, observers are suppressed (used to seed a document from persisted
    /// commits, where the caller reads the converged state directly afterwards).
    void applyUpdate(const Bytes& update, bool silent = false);

    /// Whole current text content (UTF-8).
    std::string text() const;

    /// Encode the entire document state as a single update for persistence or
    /// catch-up of a peer that has seen nothing yet.
    Bytes encodeStateAsUpdate() const;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace jami
