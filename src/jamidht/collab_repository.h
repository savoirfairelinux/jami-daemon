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
 *   meta.json                          document id, conversation id, kind, name
 *   document.md                        human-readable projection of the content
 *   deltas/<deviceId>/<NNNNNN>-<oid>   batched CRDT updates, one blob per checkpoint
 * @endverbatim
 *
 * The repository is @b bare: the content is reachable through the object
 * database and the history is readable with @c git @c log / @c git @c show,
 * but no working copy is checked out, which halves the disk footprint.
 *
 * Why this converges without conflicts
 * ------------------------------------
 * Every device only ever adds entries below its own @c deltas/<deviceId>/
 * subtree, and entry names embed the blob hash, so an entry name never denotes
 * two different contents. The set of delta blobs is therefore a @b grow-only
 * @b set: merging two replicas is the union of their trees, which is
 * deterministic, commutative, associative and idempotent. No merge conflict can
 * occur by construction, so a single @c main branch is enough.
 *
 * @c document.md is a derived artifact, recomputed from the CRDT state after a
 * merge; it is never merged textually.
 *
 * Thread-safety: every public method is serialized by an internal mutex.
 */
class CollabRepository
{
public:
    /// Descriptive fields of a document, stored in @c meta.json.
    struct Meta
    {
        std::string documentId;
        std::string conversationId;
        std::string displayName;
        std::string kind {"text"}; ///< "text" (plain) or "rich" (formatted)
        std::string createdBy;     ///< author URI
        int64_t createdAt {0};     ///< seconds since epoch
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

    /// Whether @c id is safe to build a filesystem path from. Document and
    /// conversation ids are hexadecimal on the wire, so anything else is either
    /// a bug or an attempt at escaping the account's directory.
    static bool isValidId(std::string_view id);

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
                                                    const std::string& kind);
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
     * Append a checkpoint: a batch of CRDT updates plus the regenerated
     * human-readable projection of the document.
     *
     * @param updates  CRDT updates accumulated since the previous checkpoint,
     *                 each base64-encoded; stored as one newline-separated blob
     *                 so a burst of typing costs a single object.
     * @param projection  current document content, stored as @c document.md so
     *                 that @c git @c log @c -p and @c git @c show are readable.
     * @return the new commit id, or an empty string on failure.
     */
    std::string appendCheckpoint(const std::vector<std::string>& updates, const std::string& projection);

    /**
     * Rewrite @c document.md from the merged state, committing only when it
     * actually changed.
     *
     * A merge unions the deltas but cannot recompute the text, since the CRDT
     * lives in the editing engine. Calling this after replaying a merge keeps
     * the readable projection in step with the content. Being a no-op when
     * nothing changed is what stops two replicas from notifying each other back
     * and forth.
     *
     * @return the new commit id, or an empty string if nothing was committed.
     */
    std::string refreshProjection(const std::string& projection);

    /// Rename the document (updates @c meta.json). Returns the new commit id.
    std::string setDisplayName(const std::string& displayName);

    /**
     * Every CRDT update stored in the repository, oldest checkpoint first.
     * Replaying them rebuilds the document state.
     */
    std::vector<std::string> updates() const;
    /// Whether the repository has no commit yet, i.e. nothing was ever stored
    /// or fetched for this document.
    bool isEmpty() const;
    /// Current @c document.md content.
    std::string projection() const;
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

    /// Number of CRDT updates currently stored (cheap growth indicator).
    size_t updateCount() const;

private:
    CollabRepository(const std::shared_ptr<JamiAccount>& account,
                     std::string conversationId,
                     std::string documentId,
                     std::string path);

    class Impl;
    std::unique_ptr<Impl> pimpl_;

    std::string conversationId_;
    std::string documentId_;
    std::string path_;
};

} // namespace jami
