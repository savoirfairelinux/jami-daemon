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

namespace dhtnet {
class ChannelSocket;
}

namespace jami {

class JamiAccount;
class Conversation;

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
 * Real-time path: devices that have a document open hold a dedicated binary
 * channel ("ydoc://") per peer device, over which updates travel raw as they
 * are produced, alongside awareness states -- who is editing and where. There
 * is no handshake and no per-peer protocol state: a CRDT update commutes and
 * repeats harmlessly, so convergence needs nothing more than every update
 * eventually reaching every replica -- live over these channels, or with the
 * next checkpoint for whatever a device missed while its channels were down.
 *
 * Durable path: each document is a swarm repository of its own (a conversation
 * in mode DOCUMENT, held by ConversationModule), where batches of updates are
 * appended as checkpoint commits. It replicates through the very pipeline the
 * conversations use -- same membership, same validation, same git transport --
 * so offline peers and late joiners converge and the history stays browsable,
 * without adding anything to the conversation history itself.
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
    /// Retire a document from the conversation: it is removed on every device.
    /// Only its author can, which the swarm enforces on the edition itself.
    /// @return false if no announcement for it was found here
    bool removeDocument(const std::string& conversationId, const std::string& documentId);
    /**
     * Remove a document from @b this device only, without touching what the
     * other members hold. Anyone may, on any document: this says nothing to the
     * conversation, it only reclaims what this device chose to store.
     *
     * The document stays announced and stays listed, marked as no longer held
     * here; openDocument() brings it back. Until then nothing replicates it:
     * neither an announcement paged back in, nor a peer's checkpoint, nor a live
     * update.
     *
     * The pending checkpoint and the in-memory replica are discarded. Updates
     * produced here since the last checkpoint therefore go with the repository,
     * even if another member received them live: a receiver merges remote
     * updates in memory but does not checkpoint them. An update is durably held
     * elsewhere only once a member has fetched and merged its producer's
     * checkpoint.
     *
     * @return false if the conversation never announced this document
     */
    bool removeDocumentLocally(const std::string& conversationId, const std::string& documentId);
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
     * @return the attachment id, or empty when the payload is empty, over the
     *         attachment size limit, or could not be stored.
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

    /// Whether a real-time channel for @p documentId from this peer is welcome:
    /// only when the document is currently open here and the peer is one of its
    /// members per the local replica.
    bool acceptsRealtimeChannel(const std::string& documentId, const std::string& peer, const std::string& deviceId);
    /// A real-time channel came up, whichever side asked for it: wire it into
    /// the document's session, which owns it from here on.
    void onRealtimeChannel(const std::string& documentId,
                           const std::string& peer,
                           const std::string& deviceId,
                           std::shared_ptr<dhtnet::ChannelSocket> socket);

    /// Checkpoints of a document, newest first (@c max == 0 means no limit).
    /// One map per checkpoint commit, with keys "id", "author", "device",
    /// "timestamp" and "deltas" (how many updates the checkpoint carries).
    std::vector<std::map<std::string, std::string>> history(const std::string& conversationId,
                                                            const std::string& documentId,
                                                            size_t max = 0);

    /// A peer announced a document in @c conversationId: record it, so its id
    /// is recognized when a client asks to open it. Nothing is replicated:
    /// holding a replica is a per-device choice, made by opening the document.
    void onDocumentAnnounced(const std::string& conversationId, const std::string& documentId);
    /// Whether @p documentId names a document some conversation announced, in
    /// whichever conversation. Lets the sync pipeline tell a document this
    /// device chose not to hold from a conversation it was never invited to.
    bool knowsDocument(const std::string& documentId);
    /// The author of a document retired its announcement: stop replicating it and
    /// drop what this device holds of it.
    void onDocumentRemoved(const std::string& conversationId, const std::string& documentId);
    /// The document's repository changed after a sync: replay the new updates
    /// into the live session and notify the client.
    void onRepositoryUpdated(const std::string& conversationId, const std::string& documentId);
    /// Flush every pending checkpoint (called before the account goes away).
    void flush();

private:
    struct Session;

    static std::string key(const std::string& conversationId, const std::string& documentId);
    uint64_t replicaId();
    /// The swarm holding a document's replica on this device, or nullptr when
    /// the device does not hold it (never opened, or removed from here).
    std::shared_ptr<Conversation> documentConversation(const std::string& documentId);
    std::shared_ptr<Session> ensureSession(const std::string& conversationId, const std::string& documentId);
    std::shared_ptr<Session> findSession(const std::string& conversationId, const std::string& documentId);
    /// The session holding @p documentId, whichever conversation announced it.
    /// A channel request names only the document: its repository is its own.
    std::shared_ptr<Session> findSessionByDocument(const std::string& documentId);

    /// Broadcast an update to the members and queue it for the next checkpoint.
    void onLocalUpdate(const std::shared_ptr<Session>& session, const YrsDocument::Bytes& update);
    /// Whether a COLLAB_DOC commit in @c conversationId announced this document.
    bool isAnnouncedDocument(const std::string& conversationId, const std::string& documentId);
    /// Whether the author of this document has retired its announcement.
    ///
    /// Answered from what the history walk recorded, never by asking the
    /// conversation again: this is consulted while the caller holds the
    /// conversation lock.
    bool isRemovedDocument(const std::string& conversationId, const std::string& documentId);
    /// Drop the live replica of a document, whichever kind of removal asked for
    /// it; the repository itself is torn down by the module. Cancels the
    /// pending checkpoint rather than writing it: it would land in a repository
    /// about to be erased.
    void dropLocalReplica(const std::string& conversationId, const std::string& documentId);
    /// Announced document ids per conversation, so the check above stays O(1).
    std::mutex announcedMtx_;
    std::map<std::string, std::set<std::string>> announced_;
    /// Removed document ids per conversation, guarded by @c announcedMtx_ too:
    /// the two sets are always read together, and one lock keeps them consistent.
    /// A conversation absent from this map has not been scanned yet.
    std::map<std::string, std::set<std::string>> removed_;
    /// Display names already read from the repository. Clients ask for a name far more
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

    /// Open real-time channels towards the document's member devices that have
    /// none yet. Called on open and again after every synchronization: a member
    /// whose join was merged just now is a device to reach out to, which is also
    /// what heals the refusal its own early request may have met with.
    void connectRealtimeChannels(const std::shared_ptr<Session>& session);
    /// Send one frame to every real-time channel of the session.
    void broadcastFrame(const std::shared_ptr<Session>& session, uint8_t tag, const std::vector<uint8_t>& payload);
    /// A frame arrived on one of the session's channels; @p from sent it.
    void onFrame(const std::shared_ptr<Session>& session,
                 const std::string& from,
                 uint8_t tag,
                 const uint8_t* payload,
                 size_t size);
    /// Merge an update a peer produced and hand it to the local clients.
    void onRemoteUpdate(const std::shared_ptr<Session>& session, const YrsDocument::Bytes& update);
    /// Handle a peer's awareness message; @p from owns the client ids it announces.
    void onAwarenessPayload(const std::shared_ptr<Session>& session,
                            const std::string& from,
                            const uint8_t* data,
                            size_t size);
    /// Shut every real-time channel of the session down.
    void closeRealtimeChannels(const std::shared_ptr<Session>& session);
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
};

} // namespace jami
