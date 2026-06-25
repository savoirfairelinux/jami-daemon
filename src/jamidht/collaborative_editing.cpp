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
#include "collaborative_editing.h"

#include "jamidht/jamiaccount.h"
#include "jamidht/conversation_module.h"
#include "jamidht/conversation.h"
#include "jamidht/commit_message.h"
#include "manager.h"
#include "client/jami_signal.h"
#include "base64.h"
#include "json_utils.h"

#include <chrono>
#include <cstdio>
#include <functional>
#include <random>

namespace jami {

// Coalesce bursts of local edits into a single persisted snapshot commit.
static constexpr std::chrono::seconds PERSIST_DEBOUNCE {2};

namespace {

// Serialize a rich-text op list to a Quill-style delta JSON string (the wire
// format exchanged with the editor): an array of {retain|insert|delete} ops, each
// optionally carrying an "attributes" object.
std::string
richOpsToDeltaJson(const std::vector<YrsDocument::RichOp>& ops)
{
    Json::Value arr(Json::arrayValue);
    for (const auto& op : ops) {
        Json::Value o(Json::objectValue);
        switch (op.kind) {
        case YrsDocument::RichOp::Kind::Retain:
            o["retain"] = op.len;
            break;
        case YrsDocument::RichOp::Kind::Delete:
            o["delete"] = op.len;
            break;
        case YrsDocument::RichOp::Kind::Insert:
            o["insert"] = op.text;
            break;
        }
        if (!op.attrs.empty()) {
            Json::Value attrs;
            if (json::parse(op.attrs, attrs) && attrs.isObject())
                o["attributes"] = attrs;
        }
        arr.append(o);
    }
    Json::StreamWriterBuilder b;
    b["indentation"] = "";
    return Json::writeString(b, arr);
}

// Parse a Quill-style delta JSON string into a rich-text op list. Unknown or
// malformed ops are skipped.
std::vector<YrsDocument::RichOp>
deltaJsonToRichOps(const std::string& deltaJson)
{
    std::vector<YrsDocument::RichOp> ops;
    Json::Value arr;
    if (!json::parse(deltaJson, arr) || !arr.isArray())
        return ops;
    Json::StreamWriterBuilder b;
    b["indentation"] = "";
    for (const auto& o : arr) {
        YrsDocument::RichOp op;
        if (o.isMember("insert") && o["insert"].isString()) {
            op.kind = YrsDocument::RichOp::Kind::Insert;
            op.text = o["insert"].asString();
        } else if (o.isMember("retain") && o["retain"].isIntegral()) {
            op.kind = YrsDocument::RichOp::Kind::Retain;
            op.len = o["retain"].asUInt();
        } else if (o.isMember("delete") && o["delete"].isIntegral()) {
            op.kind = YrsDocument::RichOp::Kind::Delete;
            op.len = o["delete"].asUInt();
        } else {
            continue;
        }
        if (o.isMember("attributes") && o["attributes"].isObject())
            op.attrs = Json::writeString(b, o["attributes"]);
        ops.push_back(std::move(op));
    }
    return ops;
}

} // namespace

struct CollaborativeEditing::Session
{
    std::string conversationId;
    std::string documentId;
    std::string name;
    std::unique_ptr<YrsDocument> doc;
    std::unique_ptr<asio::steady_timer> persistTimer;
    // Set once the persisted COLLAB commits have been replayed into this session.
    bool persistedLoaded {false};
};

CollaborativeEditing::CollaborativeEditing(const std::shared_ptr<JamiAccount>& account)
    : account_(account)
    , accountId_(account->getAccountID())
    , ioContext_(Manager::instance().ioContext())
{}

CollaborativeEditing::~CollaborativeEditing() = default;

uint64_t
CollaborativeEditing::replicaId()
{
    // The Y-CRDT client id must be unique and stable per device. Derive it lazily
    // from the device id, which may not be available yet when this manager is
    // first created (e.g. very early during conversation loading).
    if (clientId_ != 0)
        return clientId_;
    if (auto acc = account_.lock()) {
        auto deviceId = std::string(acc->currentDeviceId());
        if (!deviceId.empty()) {
            clientId_ = static_cast<uint64_t>(std::hash<std::string> {}(deviceId))
                        & ((uint64_t(1) << 53) - 1);
            if (clientId_ == 0)
                clientId_ = 1;
        }
    }
    return clientId_;
}

std::string
CollaborativeEditing::key(const std::string& conversationId, const std::string& documentId)
{
    return conversationId + '/' + documentId;
}

std::shared_ptr<CollaborativeEditing::Session>
CollaborativeEditing::findSession(const std::string& conversationId, const std::string& documentId)
{
    std::lock_guard<std::mutex> lk(mutex_);
    auto it = sessions_.find(key(conversationId, documentId));
    return it != sessions_.end() ? it->second : nullptr;
}

std::shared_ptr<CollaborativeEditing::Session>
CollaborativeEditing::ensureSession(const std::string& conversationId, const std::string& documentId)
{
    std::lock_guard<std::mutex> lk(mutex_);
    auto k = key(conversationId, documentId);
    if (auto it = sessions_.find(k); it != sessions_.end())
        return it->second;

    auto session = std::make_shared<Session>();
    session->conversationId = conversationId;
    session->documentId = documentId;
    session->doc = std::make_unique<YrsDocument>(replicaId());
    session->persistTimer = std::make_unique<asio::steady_timer>(*ioContext_);

    // Capture weak references only: the document owns these callbacks, so capturing
    // the session (which owns the document) would create a reference cycle.
    std::weak_ptr<CollaborativeEditing> wthis = weak_from_this();
    std::weak_ptr<Session> wsession = session;
    session->doc->setUpdateCallback(
        [wthis, wsession](const YrsDocument::Bytes& update, bool isLocal) {
            if (!isLocal)
                return; // remote updates already came from a peer; don't echo them
            auto sthis = wthis.lock();
            auto session = wsession.lock();
            if (sthis && session)
                sthis->onLocalUpdate(session, update);
        });
    session->doc->setChangeCallback(
        [wthis, wsession](const std::vector<YrsDocument::TextChange>& changes, bool isLocal) {
            if (isLocal)
                return; // local edits already live in the editor that produced them
            auto sthis = wthis.lock();
            auto session = wsession.lock();
            if (sthis && session)
                sthis->emitRemoteChanges(session->conversationId, session->documentId, changes);
        });
    session->doc->setNameCallback(
        [wthis, wsession](const std::string& name, bool /*isLocal*/) {
            // Emit for both local and remote: the bubble and other editors mirror
            // the new name. The CRDT update itself is broadcast/persisted via the
            // update callback.
            auto sthis = wthis.lock();
            auto session = wsession.lock();
            if (sthis && session)
                sthis->emitRename(session->conversationId, session->documentId, name);
        });
    session->doc->setRichChangeCallback(
        [wthis, wsession](const std::vector<YrsDocument::RichOp>& ops, bool isLocal) {
            if (isLocal)
                return; // local edits already live in the editor that produced them
            auto sthis = wthis.lock();
            auto session = wsession.lock();
            if (sthis && session)
                sthis->emitRichDelta(session->conversationId, session->documentId, ops);
        });

    sessions_.emplace(k, session);
    return session;
}

std::string
CollaborativeEditing::createDocument(const std::string& conversationId,
                                     const std::string& name,
                                     const std::string& kind)
{
    std::random_device rd;
    std::uniform_int_distribution<uint64_t> dist;
    std::mt19937_64 gen(rd());
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(dist(gen)));
    std::string documentId(buf);

    auto session = ensureSession(conversationId, documentId);
    {
        std::lock_guard<std::mutex> lk(mutex_);
        session->name = name;
    }
    // Store the name as a CRDT field too, so renames sync and persist.
    session->doc->setName(name);

    // Announce the document to the conversation (durable + shown like a file).
    if (auto account = account_.lock()) {
        if (auto* cm = account->convModule()) {
            auto state = base64::encode(session->doc->encodeStateAsUpdate());
            cm->createCommit(conversationId,
                             CommitMessage::collabDocCreated(documentId, name, kind, state));
        }
    }
    return documentId;
}

void
CollaborativeEditing::setName(const std::string& conversationId,
                              const std::string& documentId,
                              const std::string& name)
{
    auto session = findSession(conversationId, documentId);
    if (session)
        session->doc->setName(name);
}

std::string
CollaborativeEditing::documentName(const std::string& conversationId, const std::string& documentId)
{
    auto session = findSession(conversationId, documentId);
    return session ? session->doc->name() : std::string {};
}

void
CollaborativeEditing::applyDelta(const std::string& conversationId,
                                 const std::string& documentId,
                                 const std::string& deltaJson)
{
    auto session = ensureSession(conversationId, documentId);
    if (session)
        session->doc->applyDelta(deltaJsonToRichOps(deltaJson));
}

std::string
CollaborativeEditing::documentContentDelta(const std::string& conversationId,
                                           const std::string& documentId)
{
    auto session = ensureSession(conversationId, documentId);
    return session ? session->doc->contentDelta() : std::string {};
}

std::string
CollaborativeEditing::openDocument(const std::string& conversationId, const std::string& documentId)
{
    auto session = ensureSession(conversationId, documentId);
    // Rebuild the CRDT state from persisted commits if this session was just created,
    // so a document opens with its full content even when the daemon restarted or the
    // commits were never replayed through the message-history load path.
    loadPersistedState(conversationId, documentId, session);
    return session->doc->text();
}

void
CollaborativeEditing::closeDocument(const std::string& conversationId, const std::string& documentId)
{
    auto session = findSession(conversationId, documentId);
    if (!session)
        return;
    // Flush pending edits, but keep the in-memory CRDT replica so that reopening
    // the document shows its current content. The session stays consistent via
    // persisted commits (replayed on load) and live updates from other members.
    if (session->persistTimer)
        session->persistTimer->cancel();
    persistNow(session);
    // Tell other members this device is no longer editing (clears its cursor).
    broadcastLeave(conversationId, documentId);
}

void
CollaborativeEditing::edit(const std::string& conversationId,
                           const std::string& documentId,
                           uint32_t index,
                           uint32_t deleteLen,
                           const std::string& insert)
{
    auto session = findSession(conversationId, documentId);
    if (!session)
        return;
    // A replace is expressed as a delete then an insert at the same index.
    if (deleteLen > 0)
        session->doc->remove(index, deleteLen);
    if (!insert.empty())
        session->doc->insert(index, insert);
}

std::string
CollaborativeEditing::documentText(const std::string& conversationId, const std::string& documentId)
{
    auto session = findSession(conversationId, documentId);
    return session ? session->doc->text() : std::string {};
}

void
CollaborativeEditing::onRemotePayload(const std::string& from, const std::string& jsonPayload)
{
    Json::Value root;
    if (!json::parse(jsonPayload, root))
        return;
    auto conversationId = root["cid"].asString();
    auto documentId = root["did"].asString();
    if (conversationId.empty() || documentId.empty())
        return;

    // "k" (kind) discriminates ephemeral awareness messages from CRDT ops.
    // Absent/"op" = a CRDT update; "cur" = a cursor position; "leave" = the
    // peer closed the document. Awareness is keyed on the authenticated sender.
    auto kind = root.get("k", "op").asString();
    if (kind == "cur") {
        emitSignal<libjami::ConfigurationSignal::CollaborativeCursorChanged>(accountId_,
                                                                             conversationId,
                                                                             documentId,
                                                                             from,
                                                                             root.get("p", 0).asInt(),
                                                                             root.get("a", 0).asInt());
        return;
    }
    if (kind == "leave") {
        emitSignal<libjami::ConfigurationSignal::CollaborativeParticipantLeft>(accountId_,
                                                                               conversationId,
                                                                               documentId,
                                                                               from);
        return;
    }
    auto update = base64::decode(root["u"].asString());
    if (update.empty())
        return;
    auto session = ensureSession(conversationId, documentId);
    session->doc->applyUpdate(update);
}

void
CollaborativeEditing::setCursor(const std::string& conversationId,
                                const std::string& documentId,
                                int position,
                                int anchor)
{
    auto account = account_.lock();
    if (!account)
        return;
    Json::Value root;
    root["cid"] = conversationId;
    root["did"] = documentId;
    root["k"] = "cur";
    root["p"] = position;
    root["a"] = anchor;
    account->sendInstantMessage(conversationId, {{MIME_TYPE_COLLAB, json::toString(root)}});
}

void
CollaborativeEditing::broadcastLeave(const std::string& conversationId, const std::string& documentId)
{
    auto account = account_.lock();
    if (!account)
        return;
    Json::Value root;
    root["cid"] = conversationId;
    root["did"] = documentId;
    root["k"] = "leave";
    account->sendInstantMessage(conversationId, {{MIME_TYPE_COLLAB, json::toString(root)}});
}

void
CollaborativeEditing::applyPersistedUpdate(const std::string& conversationId,
                                           const std::string& documentId,
                                           const std::string& base64Update,
                                           bool notifyClient)
{
    auto update = base64::decode(base64Update);
    if (update.empty())
        return;
    auto session = ensureSession(conversationId, documentId);
    session->doc->applyUpdate(update, /*silent=*/!notifyClient);
}

void
CollaborativeEditing::onDocumentCommit(const std::string& conversationId,
                                       const std::string& documentId,
                                       const std::string& base64State)
{
    auto session = ensureSession(conversationId, documentId);
    if (base64State.empty())
        return;
    auto state = base64::decode(base64State);
    if (!state.empty())
        session->doc->applyUpdate(state);
}

void
CollaborativeEditing::onLocalUpdate(const std::shared_ptr<Session>& session,
                                    const YrsDocument::Bytes& update)
{
    // Real-time path: broadcast the incremental update to connected members.
    if (auto account = account_.lock()) {
        Json::Value root;
        root["cid"] = session->conversationId;
        root["did"] = session->documentId;
        root["u"] = base64::encode(update);
        account->sendInstantMessage(session->conversationId,
                                    {{MIME_TYPE_COLLAB, json::toString(root)}});
    }
    // Durable path: schedule a debounced snapshot commit.
    schedulePersist(session);
}

void
CollaborativeEditing::loadPersistedState(const std::string& conversationId,
                                         const std::string& documentId,
                                         const std::shared_ptr<Session>& session)
{
    {
        std::lock_guard<std::mutex> lk(mutex_);
        if (session->persistedLoaded)
            return;
        session->persistedLoaded = true;
    }
    auto account = account_.lock();
    if (!account)
        return;
    auto* cm = account->convModule();
    if (!cm)
        return;
    auto conversation = cm->getConversation(conversationId);
    if (!conversation)
        return;

    // Replay the document's persisted CRDT snapshots read straight from git. Seeding is
    // silent: the caller reads the converged state directly, and live editors are updated
    // through the normal real-time/commit paths.
    for (const auto& commit : conversation->collaborativeCommits(documentId)) {
        auto bodyIt = commit.find(CommitKey::BODY);
        if (bodyIt == commit.end() || bodyIt->second.empty())
            continue;
        auto state = base64::decode(bodyIt->second);
        if (!state.empty())
            session->doc->applyUpdate(state, /*silent=*/true);
    }
}

void
CollaborativeEditing::schedulePersist(const std::shared_ptr<Session>& session)
{
    if (!session->persistTimer)
        return;
    std::weak_ptr<CollaborativeEditing> wthis = weak_from_this();
    std::weak_ptr<Session> wsession = session;
    session->persistTimer->expires_after(PERSIST_DEBOUNCE);
    session->persistTimer->async_wait([wthis, wsession](const asio::error_code& ec) {
        if (ec) // cancelled by a newer edit (debounce) or by shutdown
            return;
        auto sthis = wthis.lock();
        auto session = wsession.lock();
        if (sthis && session)
            sthis->persistNow(session);
    });
}

void
CollaborativeEditing::persistNow(const std::shared_ptr<Session>& session)
{
    auto account = account_.lock();
    if (!account)
        return;
    auto* cm = account->convModule();
    if (!cm)
        return;
    auto state = base64::encode(session->doc->encodeStateAsUpdate());
    cm->createCommit(session->conversationId,
                     CommitMessage::collabUpdate(session->documentId, state));
}

void
CollaborativeEditing::emitRemoteChanges(const std::string& conversationId,
                                        const std::string& documentId,
                                        const std::vector<YrsDocument::TextChange>& changes)
{
    for (const auto& c : changes) {
        emitSignal<libjami::ConfigurationSignal::CollaborativeDocumentChanged>(accountId_,
                                                                               conversationId,
                                                                               documentId,
                                                                               static_cast<int>(
                                                                                   c.index),
                                                                               static_cast<int>(
                                                                                   c.deleteLen),
                                                                               c.inserted);
    }
}

void
CollaborativeEditing::emitRename(const std::string& conversationId,
                                 const std::string& documentId,
                                 const std::string& name)
{
    emitSignal<libjami::ConfigurationSignal::CollaborativeDocumentRenamed>(accountId_,
                                                                           conversationId,
                                                                           documentId,
                                                                           name);
}

void
CollaborativeEditing::emitRichDelta(const std::string& conversationId,
                                    const std::string& documentId,
                                    const std::vector<YrsDocument::RichOp>& ops)
{
    emitSignal<libjami::ConfigurationSignal::CollaborativeDocumentDelta>(accountId_,
                                                                         conversationId,
                                                                         documentId,
                                                                         richOpsToDeltaJson(ops));
}

} // namespace jami
