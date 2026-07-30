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
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace jami {

class JamiAccount;

/**
 * Durable storage for one collaborative document, backed by its own git
 * repository — separate from the conversation's repository.
 *
 * Rationale
 * ---------
 * Collaborative editing produces a high volume of synchronization data that
 * has no display value. Writing it into the conversation's message DAG
 * pollutes the history, breaks clients that do not know the feature, and makes
 * the data impossible to compact or delete independently. Giving each document
 * its own repository keeps the conversation clean, makes the document's history
 * directly browsable with regular git tooling, and allows per-document
 * retention policies.
 *
 * Layout (branch @c main, bare repository)
 * ----------------------------------------
 * @verbatim
 *   meta.json                          document id, conversation id, media type, name
 *   attachments/<blobId>               binary payloads referenced by the document
 * @endverbatim
 *
 * The tree holds descriptive data only. The CRDT updates themselves live in the
 * @b commit @b messages, base64-encoded, one per line below the subject:
 *
 * @verbatim
 *   checkpoint: 3 update(s)
 *
 *   AQFzk4WSAQAEAQdjb250ZW50AQdIZWxsbyA=
 *   AQFzk4WSAgAEAQdjb250ZW50CAF3b3JsZA==
 * @endverbatim
 *
 * Keeping them out of the tree is what makes the repository small. A delta
 * stored as a file forces git to rewrite the enclosing subtrees at every
 * checkpoint, and those tree objects cost more than the deltas themselves;
 * measured on a 300 kB document written by eight people, the same history is
 * 856 kB with deltas in the messages against 1780 kB with deltas in files.
 *
 * Attachments are the one thing that does live in the tree, and for the exact
 * reason deltas do not: what that measurement condemned is rewriting the tree
 * once per @b checkpoint, not holding an immutable blob. An attachment is
 * written once, never modified and never rewritten, so it costs one blob and
 * one tree rewrite for its whole lifetime. Storing it in a commit message
 * instead would mean base64, i.e. a third more bytes forever, and would put it
 * in the CRDT's path where nothing can ever reclaim it.
 *
 * The repository is @b bare: the content is reachable through the object
 * database and the history is readable with @c git @c log / @c git @c show,
 * but no working copy is checked out, which halves the disk footprint.
 *
 * Why this converges without conflicts
 * ------------------------------------
 * The set of updates of a commit is the set of updates of every commit it
 * reaches, so merging two replicas is a plain union performed by the commit
 * graph itself: a merge commit reaches both sides. That union is deterministic,
 * commutative, associative and idempotent, and it needs no tree surgery. Y-CRDT
 * updates being commutative in turn, replaying them in any order rebuilds the
 * same document. No merge conflict can occur by construction, so a single
 * @c main branch is enough.
 *
 * Thread-safety: every public method is serialized by an internal mutex.
 */
class CollabRepository
{
public:
    /// What a document holds when nothing says otherwise.
    static constexpr const char* DEFAULT_MIME_TYPE {"text/plain"};

    /// Descriptive fields of a document, stored in @c meta.json.
    struct Meta
    {
        std::string documentId;
        std::string conversationId;
        std::string displayName;
        std::string mimeType {DEFAULT_MIME_TYPE}; ///< media type of what the document holds
        std::string createdBy;                    ///< author URI
        int64_t createdAt {0};                    ///< seconds since epoch
    };

    /// One entry of the document's history, as shown to a client.
    struct HistoryEntry
    {
        std::string commitId;
        std::string author;   ///< signature name (member URI when known)
        std::string deviceId; ///< signature email field
        int64_t timestamp {0};
        unsigned deltaCount {0}; ///< number of CRDT updates in this checkpoint
    };

    /// Whether @c id is safe to build a filesystem path or a git refspec from.
    /// Document, conversation, commit and device ids are all hexadecimal on the
    /// wire, so anything else is either a bug or an attempt at escaping the
    /// account's directory.
    static bool isValidId(std::string_view id);

    /// Longest document name kept, in bytes. Names are merged in from other
    /// members and shown by every client; a name is a label, not a payload.
    static constexpr size_t MAX_DOCUMENT_NAME_SIZE {256};

    /// Largest attachment accepted, in bytes. Unlike a CRDT update an attachment
    /// is stored raw, so this is what it actually costs on every replica; it is
    /// deliberately larger than MAX_UPDATE_SIZE because that one pays for base64
    /// twice over.
    static constexpr size_t MAX_ATTACHMENT_SIZE {16 * 1024 * 1024};

    /// @c name cut down to MAX_DOCUMENT_NAME_SIZE bytes, on a UTF-8 boundary.
    static std::string truncatedName(std::string name);

    /// Directory holding every document repository of a conversation.
    /// Empty if @c conversationId is not a valid id.
    static std::filesystem::path conversationPath(const std::string& accountId, const std::string& conversationId);
    /// Directory of one document's repository.
    /// Empty if either id is not a valid id.
    static std::filesystem::path documentPath(const std::string& accountId,
                                              const std::string& conversationId,
                                              const std::string& documentId);
    /// Document ids having a local repository in @c conversationId.
    static std::vector<std::string> listDocuments(const std::string& accountId, const std::string& conversationId);

    /**
     * Create the repository of a new document and write its initial commit.
     * Fails if the repository already exists.
     */
    static std::shared_ptr<CollabRepository> create(const std::shared_ptr<JamiAccount>& account,
                                                    const std::string& conversationId,
                                                    const std::string& documentId,
                                                    const std::string& displayName,
                                                    const std::string& mimeType);
    /// Open an existing document repository, or return nullptr.
    static std::shared_ptr<CollabRepository> open(const std::shared_ptr<JamiAccount>& account,
                                                  const std::string& conversationId,
                                                  const std::string& documentId);
    /**
     * Open the document repository, creating an empty one (no initial commit)
     * if it does not exist yet. Used on the receiving side, before fetching a
     * document announced by a peer.
     */
    static std::shared_ptr<CollabRepository> openOrInit(const std::shared_ptr<JamiAccount>& account,
                                                        const std::string& conversationId,
                                                        const std::string& documentId);

    ~CollabRepository();

    const std::string& documentId() const { return documentId_; }
    const std::string& conversationId() const { return conversationId_; }
    /// Absolute path of the git repository (bare).
    const std::string& path() const { return path_; }

    /// Commit id of @c refs/heads/main, empty when the repository has no commit yet.
    std::string head() const;
    /// Parsed @c meta.json of the current tip.
    Meta meta() const;

    /**
     * Append a checkpoint: a batch of CRDT updates, recorded in the message of
     * a commit that leaves the tree untouched.
     *
     * @param updates  CRDT updates accumulated since the previous checkpoint,
     *                 each base64-encoded; written one per line below the
     *                 subject, so a burst of typing costs a single commit and
     *                 no tree object at all.
     * @return the new commit id, or an empty string on failure.
     */
    std::string appendCheckpoint(const std::vector<std::string>& updates);

    /// Rename the document (updates @c meta.json). Returns the new commit id.
    std::string setDisplayName(const std::string& displayName);

    /**
     * Store a binary payload the document refers to -- an image, a sound, any
     * blob a client wants to embed -- and make it part of the document's
     * history.
     *
     * The daemon does not interpret the content, exactly as it does not
     * interpret a CRDT update: what an attachment @e is remains the clients'
     * agreement.
     *
     * The returned id is the git object id of the content itself, which is what
     * a client embeds in the document. Naming the entry after its own content
     * buys three properties at no cost: identical payloads are stored once,
     * concurrent additions on two replicas can never collide, and a peer cannot
     * serve different bytes under a known id -- attachment() checks the entry
     * against the id it was asked for.
     *
     * @return the attachment id, or an empty string if @c data is empty, larger
     *         than MAX_ATTACHMENT_SIZE, or could not be written.
     */
    std::string addAttachment(const std::vector<uint8_t>& data);

    /**
     * Read back an attachment.
     *
     * @return its content, or an empty vector when this replica does not hold
     *         it (yet): a peer's attachment only lands here once the document
     *         has been fetched and merged, which is normally later than the
     *         real-time update that referenced it.
     */
    std::vector<uint8_t> attachment(const std::string& attachmentId) const;

    /// Ids of every attachment held by the current tip.
    std::vector<std::string> attachmentIds() const;

    /**
     * Every CRDT update stored in the repository, oldest checkpoint first.
     * Replaying them rebuilds the document state.
     */
    std::vector<std::string> updates() const;
    /**
     * Same, but as of @c commitId instead of the current head, so a client can
     * rebuild what the document looked like at that checkpoint.
     *
     * @return std::nullopt when @c commitId names nothing in this document's
     *         history, as opposed to an empty vector for a checkpoint that is
     *         real but holds nothing -- the first one, say. A caller that
     *         conflates the two either refuses to show a legitimately empty
     *         version or, worse, empties the document on an unknown id.
     */
    std::optional<std::vector<std::string>> updatesAt(const std::string& commitId) const;
    /// Whether the repository has no commit yet, i.e. nothing was ever stored
    /// or fetched for this document.
    bool isEmpty() const;
    /// Most recent history entries, newest first (@c max == 0 means no limit).
    std::vector<HistoryEntry> history(size_t max = 0) const;

    /**
     * Fetch @c main from a peer device into @c refs/remotes/<deviceId>/main.
     *
     * Travels over the peer-to-peer git transport, like a conversation does; the
     * channel to that device must already be open.
     *
     * @return true on success.
     */
    bool fetch(const std::string& remoteDeviceId);

    /**
     * Merge a reference fetched from @c remoteDeviceId into @c main.
     *
     * The merge is the union of the @c deltas/ trees (see class documentation),
     * never a textual merge. Fast-forwards when possible.
     *
     * @return true when @c main changed.
     */
    bool mergeRemote(const std::string& remoteDeviceId);

    /**
     * Pack the loose objects of the repository and drop the packed copies.
     *
     * Nothing else ever compacts a document repository: every checkpoint writes
     * a loose object, compressed on its own and taking a whole filesystem
     * block, and successive commits are never delta-compressed against each
     * other. Left alone, a 300 kB document written by eight people costs about
     * 7.5 MB on disk; packed, about 0.9 MB.
     *
     * Only objects reachable from @c main are packed and pruned, so an
     * interrupted write can never lose data. Concurrent readers are safe:
     * libgit2 refreshes its object database when a loose object it expected has
     * moved into a pack, and unlinking a file another handle is reading is
     * harmless on POSIX.
     *
     * Fetching from a peer also leaves a pack behind, and nothing merged them
     * either, so a heavily synchronized document accumulates one pack per fetch.
     * Compaction consolidates those too.
     *
     * @param force  compact even if little has accumulated.
     * @return true when a pack was written.
     */
    bool compact(bool force = false);

    /// Whether enough loose objects or packs have accumulated to be worth packing.
    bool needsCompaction() const;

private:
    CollabRepository(const std::shared_ptr<JamiAccount>& account,
                     std::string conversationId,
                     std::string documentId,
                     std::string path);

    /// The one instance for this repository, creating it on disk when @c create
    /// and it does not exist yet. Every public entry point goes through here, so
    /// that a single object -- and therefore a single mutex -- serializes every
    /// operation on a given repository.
    static std::shared_ptr<CollabRepository> instance(const std::shared_ptr<JamiAccount>& account,
                                                      const std::string& conversationId,
                                                      const std::string& documentId,
                                                      bool create);

    class Impl;
    std::unique_ptr<Impl> pimpl_;

    std::string conversationId_;
    std::string documentId_;
    std::string path_;
};

} // namespace jami
