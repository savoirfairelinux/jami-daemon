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

#include "collab_repository.h"
#include "yrs_document.h"

#include <asio.hpp>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace jami {

class JamiAccount;

// MIME type carrying a real-time collaborative-editing payload between members.
static constexpr const char MIME_TYPE_COLLAB[] {"application/x-jami-collab+json"};

/**
 * Per-account manager for real-time collaborative text documents shared inside
 * swarm conversations. Each document is backed by a YrsDocument (Y-CRDT) whose
 * updates are broadcast to the conversation members as ephemeral instant
 * messages and merged on reception, so every participant converges.
 *
 * Durability is provided by a CollabRepository per document: batches of CRDT
 * updates are appended as checkpoints to a dedicated git repository, so offline
 * peers and late joiners converge and the document's history stays browsable,
 * without adding anything to the conversation history itself.
 */
class CollaborativeEditing : public std::enable_shared_from_this<CollaborativeEditing>
{
public:
    explicit CollaborativeEditing(const std::shared_ptr<JamiAccount>& account);
    ~CollaborativeEditing();

    /// Create a new document in @c conversationId, returning its generated id.
    /// @c kind is "text" (plain) or "rich" (WYSIWYG/HTML).
    std::string createDocument(const std::string& conversationId, const std::string& name, const std::string& kind);
    /// Open (or create the local session for) a document; returns its current text.
    std::string openDocument(const std::string& conversationId, const std::string& documentId);
    /// Drop the local session for a document.
    void closeDocument(const std::string& conversationId, const std::string& documentId);
    /**
     * Apply a local edit: at @c index (UTF-16 code units), remove @c deleteLen
     * code units then insert @c insert.
     */
    void edit(const std::string& conversationId,
              const std::string& documentId,
              uint32_t index,
              uint32_t deleteLen,
              const std::string& insert);
    /// Current full text of a document, or an empty string if unknown.
    std::string documentText(const std::string& conversationId, const std::string& documentId);
    /// The document's content as of checkpoint @c commitId, without altering the
    /// live document. Empty if that checkpoint is unknown to this replica.
    std::string documentTextAt(const std::string& conversationId,
                               const std::string& documentId,
                               const std::string& commitId);
    /// Bring the open document back to its content at @c commitId, as a regular
    /// edit so peers converge on it. False if the document is not open or the
    /// checkpoint is unknown.
    bool restoreDocument(const std::string& conversationId, const std::string& documentId, const std::string& commitId);
    /// Current name of a document (CRDT field), or empty if unknown.
    std::string documentName(const std::string& conversationId, const std::string& documentId);
    /// Rename a document; the new name syncs to all members and persists.
    void setName(const std::string& conversationId, const std::string& documentId, const std::string& name);

    /// Apply a local rich-text edit expressed as a Quill-style delta (JSON array of
    /// retain/insert/delete ops with formatting attributes). Syncs and persists.
    void applyDelta(const std::string& conversationId, const std::string& documentId, const std::string& deltaJson);
    /// Whole current content of a document as a Quill delta JSON, or empty if unknown.
    std::string documentContentDelta(const std::string& conversationId, const std::string& documentId);

    /// List all collaborative documents that exist in @c conversationId, read from the
    /// conversation history (as COLLAB_DOC commit maps). Lets a client show the document
    /// list without paging in the announcing messages.
    std::vector<std::map<std::string, std::string>> documents(const std::string& conversationId);

    /// Broadcast this device's cursor position (UTF-16 code units) to other members.
    void setCursor(const std::string& conversationId, const std::string& documentId, int position, int anchor);

    /// Handle a collaborative-editing payload received from a peer (real-time).
    /// @param from        the sender's account URI
    /// @param fromDevice  the sending device, needed to fetch what it announces
    void onRemotePayload(const std::string& from, const std::string& fromDevice, const std::string& jsonPayload);

    /// Checkpoints of a document, newest first (@c max == 0 means no limit).
    std::vector<CollabRepository::HistoryEntry> history(const std::string& conversationId,
                                                        const std::string& documentId,
                                                        size_t max = 0);

    /// A peer announced a document in @c conversationId: make sure a local
    /// repository exists so it can be replicated.
    void onDocumentAnnounced(const std::string& conversationId, const std::string& documentId);
    /// The document's repository changed after a sync: replay the new updates
    /// into the live session and notify the client.
    void onRepositoryUpdated(const std::string& conversationId, const std::string& documentId);
    /// Flush every pending checkpoint (called before the account goes away).
    void flush();

private:
    struct Session;

    static std::string key(const std::string& conversationId, const std::string& documentId);
    uint64_t replicaId();
    std::shared_ptr<Session> ensureSession(const std::string& conversationId,
                                           const std::string& documentId,
                                           bool allowRepoCreation = true);
    std::shared_ptr<Session> findSession(const std::string& conversationId, const std::string& documentId);

    void onLocalUpdate(const std::shared_ptr<Session>& session, const YrsDocument::Bytes& update);
    /// Replay the document repository's stored updates into a freshly created
    /// session, so opening a document always shows its full content, even after a
    /// daemon restart.
    /// Whether a COLLAB_DOC commit in @c conversationId announced this document.
    bool isAnnouncedDocument(const std::string& conversationId, const std::string& documentId);
    /// Announced document ids per conversation, so the check above stays O(1).
    std::mutex announcedMtx_;
    std::map<std::string, std::set<std::string>> announced_;
    void loadPersistedState(const std::shared_ptr<Session>& session);
    /// Apply every update stored in the repository, skipping unreadable ones.
    void replayStoredUpdates(const std::shared_ptr<Session>& session, bool silent);
    void broadcastLeave(const std::string& conversationId, const std::string& documentId);
    /// Queue a local update and arm the checkpoint timer, bringing the deadline
    /// forward when enough updates have piled up.
    ///
    /// Called from the CRDT update observer, so it must not read the document
    /// back: doing so from inside the engine's write transaction aborts the
    /// process.
    void queueUpdate(const std::shared_ptr<Session>& session, const YrsDocument::Bytes& update);
    void scheduleCheckpoint(const std::shared_ptr<Session>& session, std::chrono::seconds delay);
    /**
     * Write a checkpoint now.
     *
     * @param finalizing  the session is being closed or flushed, so the readable
     *                    projection is refreshed even if the user was still
     *                    typing. See the projection policy in the .cpp.
     */
    void checkpointNow(const std::shared_ptr<Session>& session, bool finalizing = false);
    /**
     * Whether the readable projection may be rewritten now. Rate-limits the one
     * write that is proportional to the size of the document rather than to the
     * size of the edit.
     *
     * @param force  bypass the interval (closing, flushing).
     */
    bool projectionDue(const std::shared_ptr<Session>& session, bool force) const;
    /// Record that a projection was actually written. Kept separate from
    /// projectionDue() so that a failed write does not consume the interval.
    void markProjectionWritten(const std::shared_ptr<Session>& session);
    void emitRemoteChanges(const std::string& conversationId,
                           const std::string& documentId,
                           const std::vector<YrsDocument::TextChange>& changes);
    void emitRename(const std::string& conversationId, const std::string& documentId, const std::string& name);
    void emitRichDelta(const std::string& conversationId,
                       const std::string& documentId,
                       const std::vector<YrsDocument::RichOp>& ops);

    std::weak_ptr<JamiAccount> account_;
    std::string accountId_;
    uint64_t clientId_ {0};
    std::shared_ptr<asio::io_context> ioContext_;

    std::mutex mutex_;
    std::map<std::string, std::shared_ptr<Session>> sessions_;
    /// Documents already asked for since the account registered, so that paging
    /// through history does not re-request every document over and over.
    std::set<std::string> syncedDocuments_;
};

} // namespace jami
