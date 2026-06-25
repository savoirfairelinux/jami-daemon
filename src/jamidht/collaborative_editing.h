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
#include <memory>
#include <mutex>
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
 * This handles the live, in-memory side. Durable storage in the conversation
 * (for offline peers and late joiners) is layered on top separately.
 */
class CollaborativeEditing : public std::enable_shared_from_this<CollaborativeEditing>
{
public:
    explicit CollaborativeEditing(const std::shared_ptr<JamiAccount>& account);
    ~CollaborativeEditing();

    /// Create a new document in @c conversationId, returning its generated id.
    /// @c kind is "text" (plain) or "rich" (WYSIWYG/HTML).
    std::string createDocument(const std::string& conversationId,
                               const std::string& name,
                               const std::string& kind);
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
    /// Current name of a document (CRDT field), or empty if unknown.
    std::string documentName(const std::string& conversationId, const std::string& documentId);
    /// Rename a document; the new name syncs to all members and persists.
    void setName(const std::string& conversationId,
                 const std::string& documentId,
                 const std::string& name);

    /// Apply a local rich-text edit expressed as a Quill-style delta (JSON array of
    /// retain/insert/delete ops with formatting attributes). Syncs and persists.
    void applyDelta(const std::string& conversationId,
                    const std::string& documentId,
                    const std::string& deltaJson);
    /// Whole current content of a document as a Quill delta JSON, or empty if unknown.
    std::string documentContentDelta(const std::string& conversationId,
                                     const std::string& documentId);

    /// Broadcast this device's cursor position (UTF-16 code units) to other members.
    void setCursor(const std::string& conversationId,
                   const std::string& documentId,
                   int position,
                   int anchor);

    /// Handle a collaborative-editing payload received from a peer (real-time).
    void onRemotePayload(const std::string& from, const std::string& jsonPayload);

    /// Apply a persisted CRDT snapshot read from a conversation commit (load or sync).
    void applyPersistedUpdate(const std::string& conversationId,
                              const std::string& documentId,
                              const std::string& base64Update,
                              bool notifyClient = false);
    /// Seed a document's session from its COLLAB_DOC creation commit.
    void onDocumentCommit(const std::string& conversationId,
                          const std::string& documentId,
                          const std::string& base64State);

private:
    struct Session;

    static std::string key(const std::string& conversationId, const std::string& documentId);
    uint64_t replicaId();
    std::shared_ptr<Session> ensureSession(const std::string& conversationId,
                                           const std::string& documentId);
    std::shared_ptr<Session> findSession(const std::string& conversationId,
                                         const std::string& documentId);

    void onLocalUpdate(const std::shared_ptr<Session>& session, const YrsDocument::Bytes& update);
    /// Seed a freshly created session from the conversation's persisted COLLAB
    /// commits (replayed once), so opening a document is independent of whether the
    /// commits happened to pass through the message-history load path.
    void loadPersistedState(const std::string& conversationId,
                            const std::string& documentId,
                            const std::shared_ptr<Session>& session);
    void broadcastLeave(const std::string& conversationId, const std::string& documentId);
    void schedulePersist(const std::shared_ptr<Session>& session);
    void persistNow(const std::shared_ptr<Session>& session);
    void emitRemoteChanges(const std::string& conversationId,
                           const std::string& documentId,
                           const std::vector<YrsDocument::TextChange>& changes);
    void emitRename(const std::string& conversationId,
                    const std::string& documentId,
                    const std::string& name);
    void emitRichDelta(const std::string& conversationId,
                       const std::string& documentId,
                       const std::vector<YrsDocument::RichOp>& ops);

    std::weak_ptr<JamiAccount> account_;
    std::string accountId_;
    uint64_t clientId_ {0};
    std::shared_ptr<asio::io_context> ioContext_;

    std::mutex mutex_;
    std::map<std::string, std::shared_ptr<Session>> sessions_;
};

} // namespace jami
