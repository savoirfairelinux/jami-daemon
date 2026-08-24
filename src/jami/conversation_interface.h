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

#ifndef LIBJAMI_CONVERSATIONI_H
#define LIBJAMI_CONVERSATIONI_H

#include "def.h"

#include <vector>
#include <map>
#include <string>
#include <cstdint>

namespace libjami {

struct SwarmMessage
{
    std::string id;
    std::string type;
    std::string linearizedParent;
    std::map<std::string, std::string> body;
    std::vector<std::map<std::string, std::string>> reactions;
    std::vector<std::map<std::string, std::string>> editions;
    std::map<std::string, int32_t> status;
    // Extra attributes written by plugins (e.g. {"bodyOverwrite": "..."}).
    // Never stored to git — local display only.
    std::map<std::string, std::string> pluginData;
    // Id of the edition commit that last set the current body (empty = no editions yet).
    // Used to correctly tag superseded bodies when new editions arrive.
    std::string latestEditionId;

    void fromMapStringString(const std::map<std::string, std::string>& commit)
    {
        id = commit.at("id");
        type = commit.at("type");
        body = commit; // TODO erase type/id?
    }
};

// Conversation management
LIBJAMI_PUBLIC std::string startConversation(const std::string& accountId);
LIBJAMI_PUBLIC void acceptConversationRequest(const std::string& accountId, const std::string& conversationId);
LIBJAMI_PUBLIC void declineConversationRequest(const std::string& accountId, const std::string& conversationId);
LIBJAMI_PUBLIC bool removeConversation(const std::string& accountId, const std::string& conversationId);
LIBJAMI_PUBLIC std::vector<std::string> getConversations(const std::string& accountId);
LIBJAMI_PUBLIC std::vector<std::map<std::string, std::string>> getConversationRequests(const std::string& accountId);

// Calls
LIBJAMI_PUBLIC std::vector<std::map<std::string, std::string>> getActiveCalls(const std::string& accountId,
                                                                              const std::string& conversationId);

// Conversation's infos management
LIBJAMI_PUBLIC void updateConversationInfos(const std::string& accountId,
                                            const std::string& conversationId,
                                            const std::map<std::string, std::string>& infos);
LIBJAMI_PUBLIC std::map<std::string, std::string> conversationInfos(const std::string& accountId,
                                                                    const std::string& conversationId);
LIBJAMI_PUBLIC void setConversationPreferences(const std::string& accountId,
                                               const std::string& conversationId,
                                               const std::map<std::string, std::string>& prefs);
LIBJAMI_PUBLIC std::map<std::string, std::string> getConversationPreferences(const std::string& accountId,
                                                                             const std::string& conversationId);

// Member management
LIBJAMI_PUBLIC void addConversationMember(const std::string& accountId,
                                          const std::string& conversationId,
                                          const std::string& contactUri);
LIBJAMI_PUBLIC void removeConversationMember(const std::string& accountId,
                                             const std::string& conversationId,
                                             const std::string& contactUri);
LIBJAMI_PUBLIC std::vector<std::map<std::string, std::string>> getConversationMembers(const std::string& accountId,
                                                                                      const std::string& conversationId);

// Message send/load
LIBJAMI_PUBLIC void sendMessage(const std::string& accountId,
                                const std::string& conversationId,
                                const std::string& message,
                                const std::string& replyTo,
                                const int32_t& flag = 0);
LIBJAMI_PUBLIC uint32_t loadConversation(const std::string& accountId,
                                         const std::string& conversationId,
                                         const std::string& fromMessage,
                                         size_t n);
LIBJAMI_PUBLIC uint32_t loadSwarmUntil(const std::string& accountId,
                                       const std::string& conversationId,
                                       const std::string& fromMessage,
                                       const std::string& toMessage,
                                       size_t n = 0);
LIBJAMI_PUBLIC uint32_t countInteractions(const std::string& accountId,
                                          const std::string& conversationId,
                                          const std::string& toId,
                                          const std::string& fromId,
                                          const std::string& authorUri);
LIBJAMI_PUBLIC void clearCache(const std::string& accountId, const std::string& conversationId);
LIBJAMI_PUBLIC uint32_t searchConversation(const std::string& accountId,
                                           const std::string& conversationId,
                                           const std::string& author,
                                           const std::string& lastId,
                                           const std::string& regexSearch,
                                           const std::string& type,
                                           const int64_t& after,
                                           const int64_t& before,
                                           const uint32_t& maxResult,
                                           const int32_t& flag);
LIBJAMI_PUBLIC void reloadConversationsAndRequests(const std::string& accountId);

/**
 * Real-time collaborative editing of a shared document inside a conversation.
 *
 * The daemon is a transport for Y-CRDT updates, not an editor: it moves opaque
 * updates between the clients and the conversation members and stores them, but
 * it never interprets what a document contains. A client keeps its own yrs
 * replica, produces updates from it and merges the ones it receives through
 * ConversationSignal::CollaborativeDocumentUpdate.
 *
 * Because nothing here is tied to a document type, a client is free to implement
 * an editor for any type yrs supports -- text, rich text, maps, arrays, XML
 * fragments -- without a change to this API. A document carries the media type
 * of what it holds, so a client can tell an unsupported document apart from one
 * it should open.
 *
 * Updates cross this API as the bytes the engine produced. They are binary and
 * they are handed over as such: encoding them would inflate every keystroke by a
 * third and cost a conversion at each end, for nothing the transports need.
 */
LIBJAMI_PUBLIC std::string createCollaborativeDocument(const std::string& accountId,
                                                       const std::string& conversationId,
                                                       const std::string& name,
                                                       const std::string& mimeType);
/**
 * Open a document and get its whole state as a single Y-CRDT update. Apply it to
 * a fresh replica to obtain the current document.
 *
 * @return the state, never empty: a document that holds nothing still encodes as
 *         a short, valid update, and applying it is a harmless no-op. Empty means
 *         the account is gone.
 */
LIBJAMI_PUBLIC std::vector<uint8_t> openCollaborativeDocument(const std::string& accountId,
                                                              const std::string& conversationId,
                                                              const std::string& documentId);
/**
 * Remove a document from the conversation. It is retired for every member and
 * erased from every device, so this is not a way to stop holding a document
 * locally.
 *
 * Only the member who created the document can: the removal is an edition of the
 * announcement, and the swarm accepts an edition only from the author of what it
 * edits.
 *
 * @return false if no announcement for that document is known here. True means
 *         the removal was committed, not that the members already applied it;
 *         ConversationSignal::CollaborativeDocumentRemoved reports that.
 */
LIBJAMI_PUBLIC bool removeCollaborativeDocument(const std::string& accountId,
                                                const std::string& conversationId,
                                                const std::string& documentId);
/**
 * Remove a document from this device only, leaving the other members untouched.
 *
 * Any member may, on any document: nothing is said to the conversation, this
 * only reclaims what this device chose to store. The document stays listed --
 * with "storedLocally" false -- and openCollaborativeDocument() fetches it back.
 *
 * This cancels the pending checkpoint and discards the in-memory replica. Edits
 * produced here since the last checkpoint therefore go with it, even when
 * another member received them live: receivers merge remote updates in memory
 * but do not checkpoint them. An update is durably held elsewhere only after a
 * member has fetched and merged the producer's checkpoint into its repository.
 *
 * @return false if the conversation never announced that document. True means it
 *         is gone from here; ConversationSignal::CollaborativeDocumentRemoved
 *         reports it with @c everywhere false.
 */
LIBJAMI_PUBLIC bool removeCollaborativeDocumentLocally(const std::string& accountId,
                                                       const std::string& conversationId,
                                                       const std::string& documentId);
LIBJAMI_PUBLIC void closeCollaborativeDocument(const std::string& accountId,
                                               const std::string& conversationId,
                                               const std::string& documentId);
/**
 * Hand the daemon an update produced by the client's own replica: it is merged,
 * broadcast to the members and persisted.
 *
 * It is not signalled back to the local clients, since the replica that produced
 * it already holds it.
 *
 * An update the engine cannot read, or one over 8 MiB once decoded, is dropped:
 * the call has no way to fail, so a client must not treat it as an acknowledgement.
 * Calling collaborativeDocumentState() tells the client what the daemon actually
 * holds.
 */
LIBJAMI_PUBLIC void applyCollaborativeUpdate(const std::string& accountId,
                                             const std::string& conversationId,
                                             const std::string& documentId,
                                             const std::vector<uint8_t>& update);
/// The document's whole current state as a Y-CRDT update.
LIBJAMI_PUBLIC std::vector<uint8_t> collaborativeDocumentState(const std::string& accountId,
                                                               const std::string& conversationId,
                                                               const std::string& documentId);
/**
 * Share ephemeral state with the other members while editing: presence, cursor,
 * selection. The payload is opaque, never merged and never stored, so its shape
 * is the clients' own agreement. Delivered as
 * ConversationSignal::CollaborativeAwarenessChanged.
 *
 * It is meant to stay small: a state over 8 KiB is dropped, in both directions.
 */
LIBJAMI_PUBLIC void setCollaborativeAwareness(const std::string& accountId,
                                              const std::string& conversationId,
                                              const std::string& documentId,
                                              const std::string& state);
LIBJAMI_PUBLIC void setCollaborativeDocumentName(const std::string& accountId,
                                                 const std::string& conversationId,
                                                 const std::string& documentId,
                                                 const std::string& name);
LIBJAMI_PUBLIC std::string collaborativeDocumentName(const std::string& accountId,
                                                     const std::string& conversationId,
                                                     const std::string& documentId);
/**
 * Every collaborative document announced in a conversation. Each entry carries
 * "id", the document's own id -- the one every other document call takes --
 * plus "displayName", "mimeType", "author" and "timestamp", read from the
 * commit that announced it, and "announcement", that commit's id, so a client
 * can tie the document to its timeline interaction. One more key is added by
 * the daemon rather than read from the commit: "storedLocally", "true" unless
 * this device removed the document from itself, in which case opening it
 * fetches it back.
 */
LIBJAMI_PUBLIC std::vector<std::map<std::string, std::string>> getCollaborativeDocuments(
    const std::string& accountId, const std::string& conversationId);

/**
 * Checkpoints of a collaborative document, newest first. Each entry describes one
 * batch of edits with the keys "id", "author", "device", "timestamp" and "deltas".
 * @param accountId       the local account id
 * @param conversationId  the conversation hosting the document
 * @param documentId      the document id
 * @param max             maximum number of entries, 0 for no limit
 */
LIBJAMI_PUBLIC std::vector<std::map<std::string, std::string>> getCollaborativeDocumentHistory(
    const std::string& accountId, const std::string& conversationId, const std::string& documentId, uint32_t max);

/**
 * The document's state as it was at checkpoint @c commitId, as a Y-CRDT update.
 * The live document is left untouched.
 *
 * Reviewing that state, or restoring the document to it, is the client's
 * business: both need to know what the document is, which is precisely what the
 * daemon does not.
 * @return empty if that checkpoint is unknown here
 */
LIBJAMI_PUBLIC std::vector<uint8_t> collaborativeDocumentStateAt(const std::string& accountId,
                                                                 const std::string& conversationId,
                                                                 const std::string& documentId,
                                                                 const std::string& commitId);

/**
 * Store a binary payload the document refers to -- an image, a sound, any blob --
 * and return the id to embed in the document.
 *
 * The content is opaque to the daemon, like an update: this does not know about
 * images any more than the rest of this API knows about text. It is kept out of
 * the CRDT deliberately, because a CRDT never forgets: a deleted payload would
 * still weigh on every replica for good. Stored here it is a plain git blob,
 * written once whatever the number of references to it, and carried to the other
 * members by the document's own repository.
 *
 * @return the attachment id, or empty when the payload is empty, over 16 MiB, or
 *         could not be stored.
 */
LIBJAMI_PUBLIC std::string addCollaborativeAttachment(const std::string& accountId,
                                                      const std::string& conversationId,
                                                      const std::string& documentId,
                                                      const std::vector<uint8_t>& data);
/**
 * Read back an attachment.
 *
 * @return empty when this replica does not hold it @b yet, which is the normal
 *         state right after a peer referenced it: the reference travels on the
 *         real-time path and the payload with the repository. A client should
 *         show a placeholder and wait for
 *         ConversationSignal::CollaborativeAttachmentAdded rather than treat
 *         this as an error.
 */
LIBJAMI_PUBLIC std::vector<uint8_t> collaborativeAttachment(const std::string& accountId,
                                                            const std::string& conversationId,
                                                            const std::string& documentId,
                                                            const std::string& attachmentId);

struct LIBJAMI_PUBLIC ConversationSignal
{
    /**
     * A Y-CRDT update to merge into the client's own replica of the document.
     *
     * The payload is opaque: the daemon neither produces nor reads the
     * document's content, so this one signal carries every change of every
     * document type. An update the replica already has is a no-op, so applying
     * it unconditionally is always correct.
     *
     * An empty payload is a notification, not an update: the document changed
     * -- a synchronization brought edits while no client here had it open --
     * but the content is withheld until the document is opened. There is
     * nothing to apply; it exists so a client can mark the document unread.
     */
    struct LIBJAMI_PUBLIC CollaborativeDocumentUpdate
    {
        constexpr static const char* name = "CollaborativeDocumentUpdate";
        using cb_type = void(const std::string& /*account_id*/,
                             const std::string& /*convId*/,
                             const std::string& /*documentId*/,
                             const std::vector<uint8_t>& /*update*/);
    };
    /**
     * Ephemeral state a peer shares while editing: presence, cursor, selection.
     * Never merged and never stored; its shape is agreed between clients, not
     * imposed by the daemon.
     *
     * A peer is identified by @c clientId, not by @c peerId: one account can
     * have several devices in the same document, and each of them has its own
     * cursor. @c peerId says which person that client belongs to.
     */
    struct LIBJAMI_PUBLIC CollaborativeAwarenessChanged
    {
        constexpr static const char* name = "CollaborativeAwarenessChanged";
        using cb_type = void(const std::string& /*account_id*/,
                             const std::string& /*convId*/,
                             const std::string& /*documentId*/,
                             const std::string& /*peerId*/,
                             uint64_t /*clientId*/,
                             const std::string& /*state*/);
    };
    /// A client withdrew its state, or stopped announcing it for long enough to
    /// be considered gone. Anything shown for @c clientId can be dropped.
    struct LIBJAMI_PUBLIC CollaborativeParticipantLeft
    {
        constexpr static const char* name = "CollaborativeParticipantLeft";
        using cb_type = void(const std::string& /*account_id*/,
                             const std::string& /*convId*/,
                             const std::string& /*documentId*/,
                             const std::string& /*peerId*/,
                             uint64_t /*clientId*/);
    };
    struct LIBJAMI_PUBLIC CollaborativeDocumentRenamed
    {
        constexpr static const char* name = "CollaborativeDocumentRenamed";
        using cb_type = void(const std::string& /*account_id*/,
                             const std::string& /*convId*/,
                             const std::string& /*documentId*/,
                             const std::string& /*name*/);
    };
    /// A document is no longer held by this device. @c everywhere tells the two
    /// apart: true when its author retired it and it is gone for every member,
    /// false when this device alone removed it and the others still have it.
    struct LIBJAMI_PUBLIC CollaborativeDocumentRemoved
    {
        constexpr static const char* name = "CollaborativeDocumentRemoved";
        using cb_type = void(const std::string& /*account_id*/,
                             const std::string& /*convId*/,
                             const std::string& /*documentId*/,
                             bool /*everywhere*/);
    };
    /// A synchronization brought in a binary payload the document refers to.
    /// Clients showing a placeholder for it can now read it with
    /// collaborativeAttachment().
    struct LIBJAMI_PUBLIC CollaborativeAttachmentAdded
    {
        constexpr static const char* name = "CollaborativeAttachmentAdded";
        using cb_type = void(const std::string& /*account_id*/,
                             const std::string& /*convId*/,
                             const std::string& /*documentId*/,
                             const std::string& /*attachmentId*/);
    };
    struct LIBJAMI_PUBLIC SwarmLoaded
    {
        constexpr static const char* name = "SwarmLoaded";
        using cb_type = void(uint32_t /* id */,
                             const std::string& /*accountId*/,
                             const std::string& /* conversationId */,
                             std::vector<SwarmMessage> /*messages*/);
    };
    struct LIBJAMI_PUBLIC MessagesFound
    {
        constexpr static const char* name = "MessagesFound";
        using cb_type = void(uint32_t /* id */,
                             const std::string& /*accountId*/,
                             const std::string& /* conversationId */,
                             std::vector<std::map<std::string, std::string>> /*messages*/);
    };
    struct LIBJAMI_PUBLIC SwarmMessageReceived
    {
        constexpr static const char* name = "SwarmMessageReceived";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& /* conversationId */,
                             const SwarmMessage& /*message*/);
    };
    struct LIBJAMI_PUBLIC SwarmMessageUpdated
    {
        constexpr static const char* name = "SwarmMessageUpdated";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& /* conversationId */,
                             const SwarmMessage& /*message*/);
    };
    struct LIBJAMI_PUBLIC ReactionAdded
    {
        constexpr static const char* name = "ReactionAdded";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& /* conversationId */,
                             const std::string& /* messageId */,
                             std::map<std::string, std::string> /*reaction*/);
    };
    struct LIBJAMI_PUBLIC ReactionRemoved
    {
        constexpr static const char* name = "ReactionRemoved";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& /* conversationId */,
                             const std::string& /* messageId */,
                             const std::string& /* reactionId */);
    };
    struct LIBJAMI_PUBLIC ConversationProfileUpdated
    {
        constexpr static const char* name = "ConversationProfileUpdated";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& /* conversationId */,
                             std::map<std::string, std::string> /*profile*/);
    };
    struct LIBJAMI_PUBLIC ConversationRequestReceived
    {
        constexpr static const char* name = "ConversationRequestReceived";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& /* conversationId */,
                             std::map<std::string, std::string> /*metadatas*/);
    };
    struct LIBJAMI_PUBLIC ConversationRequestDeclined
    {
        constexpr static const char* name = "ConversationRequestDeclined";
        using cb_type = void(const std::string& /*accountId*/, const std::string& /* conversationId */);
    };
    struct LIBJAMI_PUBLIC ConversationReady
    {
        constexpr static const char* name = "ConversationReady";
        using cb_type = void(const std::string& /*accountId*/, const std::string& /* conversationId */);
    };
    struct LIBJAMI_PUBLIC ConversationRemoved
    {
        constexpr static const char* name = "ConversationRemoved";
        using cb_type = void(const std::string& /*accountId*/, const std::string& /* conversationId */);
    };
    struct LIBJAMI_PUBLIC ConversationMemberEvent
    {
        constexpr static const char* name = "ConversationMemberEvent";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& /* conversationId */,
                             const std::string& /* memberUri */,
                             int /* event 0 = add, 1 = joins, 2 = leave, 3 = banned */);
    };

    struct LIBJAMI_PUBLIC ConversationSyncFinished
    {
        constexpr static const char* name = "ConversationSyncFinished";
        using cb_type = void(const std::string& /*accountId*/);
    };

    struct LIBJAMI_PUBLIC ConversationCloned
    {
        constexpr static const char* name = "ConversationCloned";
        using cb_type = void(const std::string& /*accountId*/);
    };

    struct LIBJAMI_PUBLIC CallConnectionRequest
    {
        constexpr static const char* name = "CallConnectionRequest";
        using cb_type = void(const std::string& /*accountId*/, const std::string& /*peerId*/, bool hasVideo);
    };

    struct LIBJAMI_PUBLIC OnConversationError
    {
        constexpr static const char* name = "OnConversationError";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& /* conversationId */,
                             int code,
                             const std::string& what);
    };

    // Preferences
    struct LIBJAMI_PUBLIC ConversationPreferencesUpdated
    {
        constexpr static const char* name = "ConversationPreferencesUpdated";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& /*conversationId*/,
                             std::map<std::string, std::string> /*preferences*/);
    };
};

} // namespace libjami

#endif // LIBJAMI_CONVERSATIONI_H
