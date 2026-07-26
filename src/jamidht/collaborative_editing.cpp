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

#include "jamidht/collab_repository.h"
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

// Checkpoint policy. Writing a commit for every burst of keystrokes multiplies
// the number of git objects for no benefit: on a 6000-keystroke session, the
// commit cadence is by far the dominant factor of the on-disk footprint, ahead
// of what is actually stored in each commit. Updates are therefore accumulated
// and flushed either after a pause in typing or once enough of them have piled
// up, which bounds both the footprint and the amount of work lost on a crash.
static constexpr std::chrono::seconds CHECKPOINT_IDLE {10};
static constexpr size_t CHECKPOINT_MAX_PENDING {200};
/// Ceiling for a session that has no repository to drain into.
static constexpr size_t PENDING_HARD_CAP {CHECKPOINT_MAX_PENDING * 50};

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
    std::shared_ptr<CollabRepository> repo;
    std::unique_ptr<asio::steady_timer> checkpointTimer;
    // Set once the repository's stored updates have been replayed into this session.
    bool persistedLoaded {false};

    // Local updates produced since the last checkpoint, base64-encoded. Guarded by
    // its own mutex: it is filled from a YrsDocument callback, which already holds
    // the document's lock, so it must not reach for the manager-wide mutex.
    std::mutex pendingMutex;
    std::vector<std::string> pending;

    // The timer is rearmed both from the thread producing the edits and from the
    // io thread retrying a failed checkpoint; asio timers are not thread-safe.
    std::mutex timerMutex;

    // Set when the pending batch reached its cap, so that continued typing stops
    // pushing the debounce timer further away and the checkpoint actually runs.
    std::atomic_bool checkpointDue {false};
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
CollaborativeEditing::ensureSession(const std::string& conversationId,
                                    const std::string& documentId,
                                    bool allowRepoCreation)
{
    std::lock_guard<std::mutex> lk(mutex_);
    auto k = key(conversationId, documentId);
    if (auto it = sessions_.find(k); it != sessions_.end()) {
        // A session created from live updates has no repository yet; give it one
        // as soon as a local call proves the document is legitimate.
        if (allowRepoCreation && !it->second->repo)
            if (auto account = account_.lock())
                it->second->repo = CollabRepository::openOrInit(account,
                                                                conversationId,
                                                                documentId);
        return it->second;
    }

    auto session = std::make_shared<Session>();
    session->conversationId = conversationId;
    session->documentId = documentId;
    session->doc = std::make_unique<YrsDocument>(replicaId());
    session->checkpointTimer = std::make_unique<asio::steady_timer>(*ioContext_);
    // Only a document the conversation announced may allocate a repository on
    // disk. Live updates that arrive before the announcement is merged are kept
    // in memory and persisted once it lands.
    if (allowRepoCreation)
        if (auto account = account_.lock())
            session->repo = CollabRepository::openOrInit(account, conversationId, documentId);

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

    auto account = account_.lock();
    if (!account)
        return {};
    // The document gets its own repository, holding its content and history. It
    // must exist before the session opens it.
    if (!CollabRepository::create(account, conversationId, documentId, name, kind)) {
        JAMI_ERROR("[Account {}] Unable to create repository for document {}",
                   accountId_,
                   documentId);
        return {};
    }

    auto session = ensureSession(conversationId, documentId);
    {
        std::lock_guard<std::mutex> lk(mutex_);
        session->name = name;
    }
    // Store the name as a CRDT field too, so renames sync and persist.
    session->doc->setName(name);

    // Announce the document in the conversation so that members discover it and
    // can replicate its repository. The announcement carries no content.
    if (auto* cm = account->convModule())
        cm->createCommit(conversationId, CommitMessage::collabDocCreated(documentId, name, kind));
    return documentId;
}

void
CollaborativeEditing::setName(const std::string& conversationId,
                              const std::string& documentId,
                              const std::string& name)
{
    auto session = findSession(conversationId, documentId);
    if (!session)
        return;
    session->doc->setName(name);
    if (session->repo)
        session->repo->setDisplayName(name);
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

std::vector<std::map<std::string, std::string>>
CollaborativeEditing::documents(const std::string& conversationId)
{
    auto account = account_.lock();
    if (!account)
        return {};
    auto* cm = account->convModule();
    if (!cm)
        return {};
    auto conversation = cm->getConversation(conversationId);
    if (!conversation)
        return {};
    return conversation->collaborativeDocuments();
}

std::string
CollaborativeEditing::openDocument(const std::string& conversationId, const std::string& documentId)
{
    auto session = ensureSession(conversationId, documentId);
    // Rebuild the CRDT state from persisted commits if this session was just created,
    // so a document opens with its full content even when the daemon restarted or the
    // commits were never replayed through the message-history load path.
    loadPersistedState(session);
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
    if (session->checkpointTimer) {
        std::lock_guard<std::mutex> lk(session->timerMutex);
        session->checkpointTimer->cancel();
    }
    checkpointNow(session);
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
    // Durable path: accumulate the update for the next checkpoint.
    queueUpdate(session, update);
}

void
CollaborativeEditing::replayStoredUpdates(const std::shared_ptr<Session>& session, bool silent)
{
    if (!session->repo)
        return;
    // The blobs come from peers' subtrees, so their content is not ours to
    // trust: a malformed line must cost that one update, not the whole replay.
    for (const auto& encoded : session->repo->updates()) {
        try {
            auto update = base64::decode(encoded);
            if (!update.empty())
                session->doc->applyUpdate(update, silent);
        } catch (const std::exception& e) {
            JAMI_WARNING("[Account {}] [Document {}] Skipping unreadable stored update: {}",
                         accountId_,
                         session->documentId,
                         e.what());
        }
    }
}

void
CollaborativeEditing::loadPersistedState(const std::shared_ptr<Session>& session)
{
    {
        std::lock_guard<std::mutex> lk(mutex_);
        if (session->persistedLoaded)
            return;
    }
    // Replay the updates stored in the document's repository. Seeding is silent:
    // the caller reads the converged state directly, and live editors are updated
    // through the real-time path.
    replayStoredUpdates(session, /*silent=*/true);
    // Only now: a session flagged as loaded is never replayed again, so flagging
    // it before the replay would freeze a partially rebuilt document.
    std::lock_guard<std::mutex> lk(mutex_);
    session->persistedLoaded = true;
}

void
CollaborativeEditing::queueUpdate(const std::shared_ptr<Session>& session,
                                  const YrsDocument::Bytes& update)
{
    bool capReached = false;
    {
        std::lock_guard<std::mutex> lk(session->pendingMutex);
        // Without a repository nothing will ever drain this: keep the last batch
        // so a repository appearing later still saves recent work, and drop the
        // rest rather than growing without bound.
        if (!session->repo && session->pending.size() >= PENDING_HARD_CAP)
            session->pending.erase(session->pending.begin(),
                                   session->pending.begin() + CHECKPOINT_MAX_PENDING);
        session->pending.emplace_back(base64::encode(update));
        capReached = session->pending.size() >= CHECKPOINT_MAX_PENDING;
    }
    // This runs inside the CRDT's update observer, hence inside the still-open
    // write transaction. Checkpointing reads the document back, which would try
    // to open a read transaction and abort the process, so it must never happen
    // inline here: scheduleCheckpoint() only arms a timer.
    if (capReached)
        session->checkpointDue = true;
    scheduleCheckpoint(session, capReached ? std::chrono::seconds(0) : CHECKPOINT_IDLE);
}

void
CollaborativeEditing::scheduleCheckpoint(const std::shared_ptr<Session>& session,
                                         std::chrono::seconds delay)
{
    if (!session->checkpointTimer)
        return;
    // Once the batch has reached its cap, stop letting new edits push the
    // deadline further away: sustained typing would otherwise never checkpoint.
    if (delay.count() > 0 && session->checkpointDue)
        return;
    std::weak_ptr<CollaborativeEditing> wthis = weak_from_this();
    std::weak_ptr<Session> wsession = session;
    std::lock_guard<std::mutex> lk(session->timerMutex);
    session->checkpointTimer->expires_after(delay);
    session->checkpointTimer->async_wait([wthis, wsession](const asio::error_code& ec) {
        if (ec) // cancelled by a newer edit (debounce) or by shutdown
            return;
        auto sthis = wthis.lock();
        auto session = wsession.lock();
        if (sthis && session)
            sthis->checkpointNow(session);
    });
}

void
CollaborativeEditing::checkpointNow(const std::shared_ptr<Session>& session)
{
    session->checkpointDue = false;
    // Before draining anything: a drained batch with nowhere to go is lost, and
    // the session's repository is never reopened once it failed to open.
    if (!session->repo)
        return;
    std::vector<std::string> batch;
    {
        std::lock_guard<std::mutex> lk(session->pendingMutex);
        if (session->pending.empty())
            return;
        batch.swap(session->pending);
    }

    // Store the batch together with a readable projection of the document, so
    // that "git log -p" on the repository shows how the text evolved.
    if (session->repo->appendCheckpoint(batch, session->doc->text()).empty()) {
        // Keep the updates queued so the next checkpoint retries them rather
        // than silently losing the edits they carry, and make sure a retry is
        // actually scheduled even if the user has stopped typing.
        {
            std::lock_guard<std::mutex> lk(session->pendingMutex);
            session->pending.insert(session->pending.begin(),
                                    std::make_move_iterator(batch.begin()),
                                    std::make_move_iterator(batch.end()));
        }
        scheduleCheckpoint(session, CHECKPOINT_IDLE);
        return;
    }
}

std::vector<CollabRepository::HistoryEntry>
CollaborativeEditing::history(const std::string& conversationId,
                              const std::string& documentId,
                              size_t max)
{
    auto session = findSession(conversationId, documentId);
    auto repo = session ? session->repo : nullptr;
    if (!repo) {
        if (auto account = account_.lock())
            repo = CollabRepository::open(account, conversationId, documentId);
    }
    return repo ? repo->history(max) : std::vector<CollabRepository::HistoryEntry> {};
}

void
CollaborativeEditing::onDocumentAnnounced(const std::string& conversationId,
                                          const std::string& documentId)
{
    auto account = account_.lock();
    if (!account)
        return;
    // Create the local repository if needed so the document can be replicated,
    // then ask for its content. Nothing is loaded in memory until it is opened.
    CollabRepository::openOrInit(account, conversationId, documentId);
}

void
CollaborativeEditing::onRepositoryUpdated(const std::string& conversationId,
                                          const std::string& documentId)
{
    auto session = findSession(conversationId, documentId);
    if (!session || !session->repo)
        return; // not being edited here; the repository is up to date on disk
    {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!session->persistedLoaded)
            return; // never opened: it will be replayed on open
    }
    // Applying an update the replica already knows is a no-op for a CRDT, so
    // replaying the whole set is correct, just more work than strictly needed.
    for (const auto& encoded : session->repo->updates()) {
        auto update = base64::decode(encoded);
        if (!update.empty())
            session->doc->applyUpdate(update);
    }
}

void
CollaborativeEditing::flush()
{
    std::vector<std::shared_ptr<Session>> sessions;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        sessions.reserve(sessions_.size());
        for (const auto& [_, session] : sessions_)
            sessions.emplace_back(session);
    }
    for (const auto& session : sessions) {
        if (session->checkpointTimer)
            session->checkpointTimer->cancel();
        checkpointNow(session);
    }
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
