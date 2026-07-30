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
#include "y_protocol.h"

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
 * Real-time path: members exchange the yjs protocols -- @c sync for the document
 * itself, @c awareness for who is editing and where -- over ephemeral instant
 * messages. Speaking those rather than something of our own means a replica here
 * and a replica anywhere else in the yjs ecosystem converge without a
 * translation layer, and it is what brings the state vector: a device joining a
 * document is told only what it is actually missing.
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
    /// @c mimeType names the media type of what the document will hold, so a
    /// client can tell whether it is able to open it; the daemon only stores it.
    std::string createDocument(const std::string& conversationId, const std::string& name, const std::string& mimeType);
    /**
     * Open (or create the local session for) a document.
     * @return its whole state as a single Y-CRDT update, which the caller applies
     *         to its own replica. Empty for a document nothing is known about yet.
     */
    YrsDocument::Bytes openDocument(const std::string& conversationId, const std::string& documentId);
    /// Drop the local session for a document.
    void closeDocument(const std::string& conversationId, const std::string& documentId);
    /**
     * Merge a Y-CRDT update produced by a local client and hand it on: broadcast
     * to the members and queued for the next checkpoint.
     *
     * The update is opaque here. It is @b not signalled back to the local
     * clients: the one that produced it already has it in its own replica.
     */
    void applyUpdate(const std::string& conversationId, const std::string& documentId, const YrsDocument::Bytes& update);
    /// Whole current state as a single Y-CRDT update, or empty if unknown.
    YrsDocument::Bytes documentState(const std::string& conversationId, const std::string& documentId);
    /// Same, as of checkpoint @c commitId, without touching the live document.
    /// Empty if that checkpoint is unknown to this replica.
    YrsDocument::Bytes documentStateAt(const std::string& conversationId,
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
     * @p state is a JSON document whose shape is the clients' agreement, not the
     * daemon's, exactly as in the yjs awareness protocol. It is never merged and
     * never stored. Passing an empty string withdraws this device's state.
     *
     * Sharing a state also enrols the session in the protocol's upkeep: the
     * state is re-announced periodically so that peers can tell a silent editor
     * from one whose device dropped off, and a peer that stops re-announcing is
     * dropped once its state has gone stale.
     */
    void setAwareness(const std::string& conversationId, const std::string& documentId, const std::string& state);

    /**
     * Store a binary payload the document refers to -- an image, a sound, any
     * blob -- and return the id a client embeds in the document.
     *
     * Like an update, the content is opaque: what an attachment @e is remains
     * the clients' agreement. It is kept out of the CRDT on purpose. Inlining it
     * would put megabytes into a structure that never forgets anything, since a
     * CRDT keeps a tombstone for what is deleted; stored here it is a plain git
     * blob, written once, shared by every reference to it and reclaimed by the
     * usual retention of the repository.
     *
     * @return the attachment id, or empty when the payload is empty, over
     *         CollabRepository::MAX_ATTACHMENT_SIZE, or could not be stored.
     */
    std::string addAttachment(const std::string& conversationId,
                              const std::string& documentId,
                              const std::vector<uint8_t>& data);
    /**
     * Read back an attachment.
     *
     * @return empty when this replica does not hold it @e yet: the real-time
     *         path carries the reference, the payload travels with the
     *         repository, so a peer normally learns the id before it has the
     *         bytes. ConfigurationSignal::CollaborativeAttachmentAdded then
     *         tells it when they arrive.
     */
    std::vector<uint8_t> attachment(const std::string& conversationId,
                                    const std::string& documentId,
                                    const std::string& attachmentId);

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

    /// Wrap a y-protocol frame in the envelope that carries it to the members.
    /// @param peer    the single member to reach, or empty to reach them all.
    /// @param device  which of that member's devices.
    void sendFrame(const std::string& conversationId,
                   const std::string& documentId,
                   const yprotocol::Bytes& frame,
                   const std::string& peer = {},
                   const std::string& device = {});
    /// Handle the SYNC half of the protocol; @p from and @p fromDevice name who
    /// to answer.
    void onSyncMessage(const std::string& conversationId,
                       const std::string& documentId,
                       const std::string& from,
                       const std::string& fromDevice,
                       yprotocol::Decoder& decoder);
    /// Handle the AWARENESS half; @p from owns the client ids it announces.
    void onAwarenessMessage(const std::shared_ptr<Session>& session,
                            const std::string& from,
                            yprotocol::Decoder& decoder);
    /// The session a message from a peer may build state in, or nullptr when the
    /// document is neither announced nor within the tolerance for one that is
    /// about to be.
    std::shared_ptr<Session> admitSession(const std::string& conversationId, const std::string& documentId);
    /// Announce what this device is sharing, bumping its clock. Withdraws the
    /// state when @p state is empty, which is how a peer learns we are gone.
    void publishAwareness(const std::shared_ptr<Session>& session, const std::string& state);
    /// Re-announce our own state and drop the peers that stopped announcing
    /// theirs, then rearm as long as anything is still being shared.
    void awarenessUpkeep(const std::shared_ptr<Session>& session);
    void scheduleAwarenessUpkeep(const std::shared_ptr<Session>& session);
    /// Queue a local update and arm the checkpoint timer, bringing the deadline
    /// forward when enough updates have piled up.
    void queueUpdate(const std::shared_ptr<Session>& session, const YrsDocument::Bytes& update);
    void scheduleCheckpoint(const std::shared_ptr<Session>& session, std::chrono::seconds delay);
    /// Drain the pending updates into a checkpoint commit now.
    void checkpointNow(const std::shared_ptr<Session>& session);
    /// Hand an update to the local clients, so they merge it into their replica.
    void emitUpdate(const std::string& conversationId, const std::string& documentId, const YrsDocument::Bytes& update);
    void emitRename(const std::string& conversationId, const std::string& documentId, const std::string& name);
    /// Tell the local clients which attachments a synchronization brought in, so
    /// an editor showing a placeholder for one can finally draw it.
    void emitNewAttachments(const std::shared_ptr<Session>& session);

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
