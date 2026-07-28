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
#include <optional>
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
 * Per-account manager for real-time collaborative documents shared inside swarm
 * conversations.
 *
 * The daemon is a @b transport, not an editor. It moves opaque Y-CRDT updates
 * between the local clients and the conversation members, and it stores them; it
 * never interprets what a document contains. A client holds its own yrs replica,
 * produces updates from it and applies the ones it receives, which is what lets
 * a client implement an editor for @e any document type yrs supports -- text,
 * rich text, maps, arrays, XML fragments -- without a single change here.
 *
 * Real-time path: updates are broadcast to the members as ephemeral instant
 * messages and merged on reception.
 *
 * Durable path: a CollabRepository per document appends batches of updates as
 * checkpoints to a dedicated git repository, so offline peers and late joiners
 * converge and the history stays browsable, without adding anything to the
 * conversation history itself.
 */
class CollaborativeEditing : public std::enable_shared_from_this<CollaborativeEditing>
{
public:
    explicit CollaborativeEditing(const std::shared_ptr<JamiAccount>& account);
    ~CollaborativeEditing();

    /// Create a new document in @c conversationId, returning its generated id.
    /// @c kind is a free-form hint telling clients which editor to open; the
    /// daemon only stores it.
    std::string createDocument(const std::string& conversationId, const std::string& name, const std::string& kind);
    /**
     * Open (or create the local session for) a document.
     * @return its whole state as a single base64 Y-CRDT update, which the caller
     *         applies to its own replica. Empty for a document nothing is known
     *         about yet.
     */
    std::string openDocument(const std::string& conversationId, const std::string& documentId);
    /// Drop the local session for a document.
    void closeDocument(const std::string& conversationId, const std::string& documentId);
    /**
     * Merge a Y-CRDT update produced by a local client and hand it on: broadcast
     * to the members and queued for the next checkpoint.
     *
     * The update is opaque here. It is @b not signalled back to the local
     * clients: the one that produced it already has it in its own replica.
     */
    void applyUpdate(const std::string& conversationId, const std::string& documentId, const std::string& base64Update);
    /// Whole current state as a single base64 Y-CRDT update, or empty if unknown.
    std::string documentState(const std::string& conversationId, const std::string& documentId);
    /// Same, as of checkpoint @c commitId, without touching the live document.
    /// Empty if that checkpoint is unknown to this replica.
    std::string documentStateAt(const std::string& conversationId,
                                const std::string& documentId,
                                const std::string& commitId);
    /// Current name of a document, or empty if unknown.
    std::string documentName(const std::string& conversationId, const std::string& documentId);
    /// Rename a document; the new name syncs to all members and persists.
    void setName(const std::string& conversationId, const std::string& documentId, const std::string& name);

    /// List all collaborative documents that exist in @c conversationId, read from the
    /// conversation history (as COLLAB_DOC commit maps). Lets a client show the document
    /// list without paging in the announcing messages.
    std::vector<std::map<std::string, std::string>> documents(const std::string& conversationId);

    /**
     * Broadcast this device's awareness state to the other members: who is here,
     * where their cursor and selection are, anything else the client wants to
     * share while editing.
     *
     * The payload is opaque and ephemeral -- never merged, never stored -- so
     * its shape is entirely the clients' agreement, not the daemon's.
     */
    void setAwareness(const std::string& conversationId, const std::string& documentId, const std::string& state);

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

    /// Broadcast an update to the members and queue it for the next checkpoint.
    void onLocalUpdate(const std::shared_ptr<Session>& session, const YrsDocument::Bytes& update);
    /// Whether a COLLAB_DOC commit in @c conversationId announced this document.
    bool isAnnouncedDocument(const std::string& conversationId, const std::string& documentId);
    /// Whether room remains to hold a session for a document the conversation
    /// never announced. Live updates arrive before the announcement is merged,
    /// so such sessions have to be tolerated -- but an authorized member can name
    /// any id it likes, and each one costs a replica in memory that nothing would
    /// ever evict. Returns false past the cap, and true for one already held.
    bool admitUnannounced(const std::string& conversationId, const std::string& documentId);
    /// Announced document ids per conversation, so the check above stays O(1).
    std::mutex announcedMtx_;
    std::map<std::string, std::set<std::string>> announced_;
    /// Display names already read from disk. Clients ask for a name far more
    /// often than one changes -- once per message delegate built while scrolling
    /// -- and answering from the repository means git lookups on their UI thread.
    /// Refreshed on rename and dropped on synchronization. Guarded by mutex_.
    std::map<std::string, std::string> nameCache_;
    /// Bumped whenever the cache above is written or dropped, so a reader that
    /// went to disk can tell its answer was overtaken while it was reading and
    /// drop it rather than reinstate a stale name nothing would ever correct.
    uint64_t nameEpoch_ {0};
    /// Replay the document repository's stored updates into a freshly created
    /// session, so opening a document always yields its full state, even after a
    /// daemon restart.
    void loadPersistedState(const std::shared_ptr<Session>& session);
    /// Apply every update stored in the repository, skipping unreadable ones.
    void replayStoredUpdates(const std::shared_ptr<Session>& session);
    void broadcastLeave(const std::string& conversationId, const std::string& documentId);
    /// Queue a local update and arm the checkpoint timer, bringing the deadline
    /// forward when enough updates have piled up.
    void queueUpdate(const std::shared_ptr<Session>& session, const YrsDocument::Bytes& update);
    void scheduleCheckpoint(const std::shared_ptr<Session>& session, std::chrono::seconds delay);
    /// Drain the pending updates into a checkpoint commit now.
    void checkpointNow(const std::shared_ptr<Session>& session);
    /// Hand an update to the local clients, so they merge it into their replica.
    void emitUpdate(const std::string& conversationId, const std::string& documentId, const YrsDocument::Bytes& update);
    void emitRename(const std::string& conversationId, const std::string& documentId, const std::string& name);

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
