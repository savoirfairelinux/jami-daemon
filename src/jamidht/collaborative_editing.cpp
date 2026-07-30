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

#include <opendht/thread_pool.h>

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

// How many checkpoints between two looks at whether the repository is worth
// packing. Checking means listing the object directories, so it is not done on
// every checkpoint; packing itself only happens once the threshold inside
// CollabRepository::compact() is actually crossed.
static constexpr size_t COMPACT_CHECK_EVERY {64};

// Ceilings on what a single message may carry. Both the client API and the swarm
// hand us opaque blobs, and both decode into memory before the engine gets a say.
// The point is not to guess a "correct" size but to keep one message from being
// able to allocate without bound: a CRDT update stays well under a megabyte even
// when it carries the whole state of a large document, and an awareness state is
// a cursor plus a display name.
static constexpr size_t MAX_UPDATE_SIZE {8 * 1024 * 1024};
static constexpr size_t MAX_AWARENESS_SIZE {8 * 1024};
/// base64 costs 4 bytes per 3, plus padding and any line breaks a client adds.
static constexpr size_t MAX_ENCODED_UPDATE_SIZE {MAX_UPDATE_SIZE / 3 * 4 + 1024};
static constexpr size_t MAX_ENCODED_AWARENESS_SIZE {MAX_AWARENESS_SIZE / 3 * 4 + 1024};

struct CollaborativeEditing::Session
{
    std::string conversationId;
    std::string documentId;
    std::unique_ptr<YrsDocument> doc;
    std::shared_ptr<CollabRepository> repo;
    // The last name handed to the clients. A remote rename cannot be spotted by
    // reading the repository before and after a synchronization -- the merge is
    // already done by the time we hear about it -- so this is what tells a name
    // that changed from one the clients have seen. Empty until the document is
    // opened: announcing a rename to a client that was never told the first name
    // would be a phantom event. Guarded by mutex_.
    std::optional<std::string> announcedName;
    std::unique_ptr<asio::steady_timer> checkpointTimer;
    // Set once the repository's stored updates have been replayed into this session.
    bool persistedLoaded {false};
    // Attachment ids the local clients already know about, so a synchronization
    // only announces what it actually brought. Seeded on open with what the
    // repository already holds: a client reads those itself, and re-announcing
    // them would make every editor redraw its images on every sync. Guarded by
    // mutex_.
    std::set<std::string> knownAttachments;

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
    // Checkpoints written since the repository was last considered for packing.
    std::atomic_size_t sinceCompactCheck {0};
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
    // Drawn at random, never derived from the device id. The client id is only
    // half of an item id: the other half is a per-replica counter that starts at
    // zero and is not persisted on its own. A replica that reuses an id after
    // producing items its peers already hold would restart that counter, and the
    // peers would silently drop the new items as ones they had already seen,
    // diverging for good.
    //
    // The daemon's replica never produces an item of its own -- it only applies
    // updates coming from the clients and from the peers -- so its id never
    // reaches any document. Drawing it costs nothing and keeps that property from
    // depending on the fact that nothing writes here today.
    if (clientId_ != 0)
        return clientId_;
    std::random_device rd;
    // 53 bits: the yjs ecosystem carries these ids through JSON, where integers
    // above 2^53 are no longer exact.
    std::uniform_int_distribution<uint64_t> dist(1, (uint64_t(1) << 53) - 1);
    std::mt19937_64 gen(rd());
    clientId_ = dist(gen);
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
                it->second->repo = CollabRepository::openOrInit(account, conversationId, documentId);
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
    sessions_.emplace(k, session);
    return session;
}

std::string
CollaborativeEditing::createDocument(const std::string& conversationId,
                                     const std::string& name,
                                     const std::string& mimeType)
{
    std::random_device rd;
    std::uniform_int_distribution<uint64_t> dist;
    std::mt19937_64 gen(rd());
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(dist(gen)));
    std::string documentId(buf);

    // Settle the media type here rather than in each of the two places that
    // record it: the repository defaults an empty one on its own, the
    // announcement commit omits the field entirely, and a document would then be
    // listed as having no type while its metadata claimed one.
    const std::string type = mimeType.empty() ? CollabRepository::DEFAULT_MIME_TYPE : mimeType;

    auto account = account_.lock();
    if (!account)
        return {};
    // The document gets its own repository, holding its content and history. It
    // must exist before the session opens it.
    if (!CollabRepository::create(account, conversationId, documentId, name, type)) {
        JAMI_ERROR("[Account {}] Unable to create repository for document {}", accountId_, documentId);
        return {};
    }

    auto session = ensureSession(conversationId, documentId);
    {
        std::lock_guard<std::mutex> lk(mutex_);
        session->announcedName = name;
        nameCache_[key(conversationId, documentId)] = name;
        ++nameEpoch_;
    }
    {
        // Record it as announced right away. The commit below is asynchronous,
        // and a client that creates a document then opens it must not lose that
        // race against its own announcement.
        std::lock_guard<std::mutex> lk(announcedMtx_);
        announced_[conversationId].emplace(documentId);
    }
    // Announce the document in the conversation so that members discover it and
    // can replicate its repository. The announcement carries no content.
    //
    // Without it the document exists here and nowhere else: the other members
    // never learn about it, and the authorization that lets them replicate it is
    // derived from that very announcement. Report the failure rather than hand
    // back the id of a document nobody else can reach.
    auto* cm = account->convModule();
    if (!cm) {
        JAMI_ERROR("[Account {}] Unable to announce document {} in conversation {}",
                   accountId_,
                   documentId,
                   conversationId);
        closeDocument(conversationId, documentId);
        return {};
    }
    cm->createCommit(conversationId,
                     CommitMessage::collabDocCreated(documentId, name, type),
                     true,
                     {},
                     [w = weak_from_this(), conversationId, documentId, accountId = accountId_](bool ok,
                                                                                                const std::string&) {
                         if (ok)
                             return;
                         JAMI_ERROR("[Account {}] Document {} was not announced in conversation {}: "
                                    "the other members cannot reach it",
                                    accountId,
                                    documentId,
                                    conversationId);
                         if (auto sthis = w.lock()) {
                             {
                                 std::lock_guard<std::mutex> lk(sthis->announcedMtx_);
                                 if (auto it = sthis->announced_.find(conversationId); it != sthis->announced_.end())
                                     it->second.erase(documentId);
                             }
                             sthis->closeDocument(conversationId, documentId);
                         }
                     });
    return documentId;
}

void
CollaborativeEditing::setName(const std::string& conversationId, const std::string& documentId, const std::string& name)
{
    auto session = ensureSession(conversationId, documentId);
    if (!session || !session->repo)
        return;
    // The name describes the document, it is not part of its content: keeping it
    // in the repository rather than inside the CRDT is what lets the daemon stay
    // blind to what the document holds. It reaches the other members through the
    // ordinary repository synchronization.
    if (session->repo->setDisplayName(name).empty()) {
        JAMI_WARNING("[Account {}] [Document {}] Unable to rename: the repository has no commit "
                     "yet, so it has not been synchronized from the conversation",
                     accountId_,
                     documentId);
        return;
    }
    // The repository caps what it stores, so remember and announce what it really
    // holds, or the cache would answer a name no other member will ever see.
    const auto stored = CollabRepository::truncatedName(name);
    {
        std::lock_guard<std::mutex> lk(mutex_);
        session->announcedName = stored;
        nameCache_[key(conversationId, documentId)] = stored;
        ++nameEpoch_;
    }
    emitRename(conversationId, documentId, stored);
    if (auto account = account_.lock())
        account->syncCollabDocument(conversationId, documentId);
}

std::string
CollaborativeEditing::documentName(const std::string& conversationId, const std::string& documentId)
{
    // Reading a name must stay cheap. Clients ask for it constantly: once per
    // document to list a conversation's documents, and once per message delegate
    // built while scrolling a conversation. Opening a session would build a CRDT
    // replica and a checkpoint timer for every document; even reading meta.json
    // is a commit, tree and blob lookup plus a JSON parse, on the caller's thread
    // -- the client's UI thread -- behind the same lock compact() holds for the
    // length of a repack. So the answer is cached, and the cache is refreshed
    // wherever the name can change: here, in setName and on synchronization.
    const auto k = key(conversationId, documentId);
    uint64_t epoch = 0;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        if (auto it = nameCache_.find(k); it != nameCache_.end())
            return it->second;
        epoch = nameEpoch_;
    }
    std::string name;
    if (auto session = findSession(conversationId, documentId); session && session->repo) {
        name = session->repo->meta().displayName;
    } else if (auto account = account_.lock()) {
        if (auto repo = CollabRepository::open(account, conversationId, documentId))
            name = repo->meta().displayName;
        else
            return {}; // no repository yet: nothing worth remembering
    } else {
        return {};
    }
    std::lock_guard<std::mutex> lk(mutex_);
    // Only remember it if nothing invalidated the cache while we were reading.
    // Opening a conversation lists its documents and synchronizes them at the
    // same time, so a synchronization landing during the read above -- and it is
    // a cold git_repository_open, so the window is wide -- would otherwise be
    // overwritten by the name we read just before it, and stay wrong for good.
    if (epoch == nameEpoch_)
        nameCache_[k] = name;
    return name;
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

bool
CollaborativeEditing::isAnnouncedDocument(const std::string& conversationId, const std::string& documentId)
{
    // A document only exists once a COLLAB_DOC commit announced it in the
    // conversation. Without this check, a member could name arbitrary ids in
    // instant messages and have us create a bare repository on disk for each.
    {
        std::lock_guard<std::mutex> lk(announcedMtx_);
        if (auto it = announced_.find(conversationId); it != announced_.end())
            return it->second.count(documentId) != 0;
    }
    // First question asked about this conversation: walking its whole history is
    // expensive, so do it once and let onDocumentAnnounced() keep the set fresh.
    std::set<std::string> ids;
    for (const auto& doc : documents(conversationId)) {
        if (auto it = doc.find(CommitKey::URI); it != doc.end())
            ids.emplace(it->second);
    }
    std::lock_guard<std::mutex> lk(announcedMtx_);
    auto& set = announced_[conversationId];
    set.merge(ids);
    return set.count(documentId) != 0;
}

bool
CollaborativeEditing::admitUnannounced(const std::string& conversationId, const std::string& documentId)
{
    // Live updates travel over the real-time channel and routinely beat the
    // announcement they belong to, so a session has to be allowed before the
    // document is known. What must not be allowed is an unbounded number of
    // them: an authorized member can name any id it likes, and each one costs a
    // replica in memory that no later announcement would ever come and evict.
    static constexpr size_t MAX_UNANNOUNCED = 16;
    const auto k = key(conversationId, documentId);
    std::lock_guard<std::mutex> lk(mutex_);
    if (sessions_.count(k) != 0)
        return true; // already held: this is not a new allocation
    size_t unannounced = 0;
    for (const auto& [_, session] : sessions_) {
        if (session && !session->repo)
            ++unannounced;
    }
    return unannounced < MAX_UNANNOUNCED;
}

std::string
CollaborativeEditing::openDocument(const std::string& conversationId, const std::string& documentId)
{
    // A document only exists once the conversation announced it. Opening one
    // that was never announced would create a bare repository on disk for any id
    // a caller cares to name, and bypass the very gate that decides which
    // devices may replicate it.
    if (!isAnnouncedDocument(conversationId, documentId)) {
        JAMI_WARNING("[Account {}] Refusing to open document {}: it was not announced in conversation {}",
                     accountId_,
                     documentId,
                     conversationId);
        return {};
    }
    auto session = ensureSession(conversationId, documentId);
    // Rebuild the CRDT state from persisted commits if this session was just created,
    // so a document opens with its full content even when the daemon restarted or the
    // commits were never replayed through the message-history load path.
    loadPersistedState(session);
    // Remember the name the client is about to see, so a later rename can be
    // told from it. Read here rather than when the session is built: this takes
    // the repository's lock, which compact() holds for the length of a repack,
    // and holding the manager's lock across that would stall every other caller.
    if (session->repo) {
        const auto name = session->repo->meta().displayName;
        std::lock_guard<std::mutex> lk(mutex_);
        if (!session->announcedName)
            session->announcedName = name;
    }
    auto state = base64::encode(session->doc->encodeStateAsUpdate());
    // Catch up on repositories left uncompacted by earlier versions: a document
    // that is only ever synchronized accumulates one pack per fetch, and no
    // other trigger would ever tidy it up. Scheduled only once the state has
    // been read, because packing holds the repository lock for its whole
    // duration and this call is what the client waits on to show the document.
    dht::ThreadPool::io().run([repo = session->repo] {
        if (repo)
            repo->compact();
    });
    return state;
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
CollaborativeEditing::applyUpdate(const std::string& conversationId,
                                  const std::string& documentId,
                                  const std::string& base64Update)
{
    auto session = findSession(conversationId, documentId);
    if (!session)
        return;
    if (base64Update.size() > MAX_ENCODED_UPDATE_SIZE) {
        JAMI_WARNING("[Account {}] [Document {}] Discarding a {} byte update from the client: "
                     "over the {} byte limit",
                     accountId_,
                     documentId,
                     base64Update.size(),
                     MAX_ENCODED_UPDATE_SIZE);
        return;
    }
    YrsDocument::Bytes update;
    try {
        update = base64::decode(base64Update);
    } catch (const std::exception& e) {
        JAMI_WARNING("[Account {}] [Document {}] Discarding unreadable update from the client: {}",
                     accountId_,
                     documentId,
                     e.what());
        return;
    }
    // Merge before forwarding: an update the engine rejects must not be sent to
    // the members nor written to the repository.
    if (!session->doc->applyUpdate(update))
        return;
    onLocalUpdate(session, update);
}

std::string
CollaborativeEditing::documentState(const std::string& conversationId, const std::string& documentId)
{
    auto session = findSession(conversationId, documentId);
    return session ? base64::encode(session->doc->encodeStateAsUpdate()) : std::string {};
}

void
CollaborativeEditing::onRemotePayload(const std::string& from,
                                      const std::string& fromDevice,
                                      const std::string& jsonPayload)
{
    Json::Value root;
    if (!json::parse(jsonPayload, root))
        return;
    auto conversationId = root["cid"].asString();
    auto documentId = root["did"].asString();
    if (!CollabRepository::isValidId(conversationId) || !CollabRepository::isValidId(documentId))
        return;

    // Being able to send us a message is not the same as being allowed to edit
    // this document: a plain contact could otherwise inject text into a swarm
    // it does not belong to, and a legitimate editor would then checkpoint it.
    auto account = account_.lock();
    if (!account)
        return;
    auto* convModule = account->convModule(true);
    if (!convModule || !convModule->isPeerAuthorized(conversationId, from, fromDevice, true)) {
        JAMI_WARNING("[Account {}] [Document {}] Ignoring collaborative payload from "
                     "unauthorized peer {}",
                     accountId_,
                     documentId,
                     from);
        return;
    }

    // "k" (kind) discriminates ephemeral awareness messages from CRDT ops.
    // Absent/"op" = a CRDT update; "aw" = an opaque awareness state; "leave" =
    // the peer closed the document; "ckpt"/"req" drive repository
    // synchronization.
    auto kind = root.get("k", "op").asString();
    // These two touch the disk and the network, so they must name a document the
    // conversation actually announced. Live updates ("op") deliberately are not
    // gated: they routinely overtake the announcement commit, and they only ever
    // build in-memory state.
    if ((kind == "ckpt" || kind == "req") && !isAnnouncedDocument(conversationId, documentId)) {
        JAMI_WARNING("[Account {}] Ignoring collaborative {} for unannounced document {}", accountId_, kind, documentId);
        return;
    }
    if (kind == "aw") {
        // Opaque: whatever the peers agreed to put in it. The daemon only checks
        // that it comes from someone allowed to edit this document, and that it
        // is small enough to be a presence state rather than a payload.
        auto state = root.get("s", "").asString();
        if (state.size() > MAX_ENCODED_AWARENESS_SIZE) {
            JAMI_WARNING("[Account {}] [Document {}] Dropping an oversized awareness state from {}",
                         accountId_,
                         documentId,
                         from);
            return;
        }
        emitSignal<libjami::ConfigurationSignal::CollaborativeAwarenessChanged>(accountId_,
                                                                                conversationId,
                                                                                documentId,
                                                                                from,
                                                                                state);
        return;
    }
    if (kind == "ckpt") {
        // A peer checkpointed the document: pull its repository. Content is not
        // carried here, only the fact that there is something new to fetch.
        account->fetchCollabDocument(fromDevice, conversationId, documentId);
        return;
    }
    if (kind == "req") {
        // A peer wants this document and cannot pull from us on its own: answer
        // with a checkpoint notification so it fetches from us. Staying silent
        // when we have nothing avoids a needless round of fetches.
        auto session = findSession(conversationId, documentId);
        auto repo = session ? session->repo : nullptr;
        if (!repo) {
            if (auto acc = account_.lock())
                // open(), not openOrInit(): this is a peer asking, and creating a
                // repository on its say-so would let any member make us hold a
                // directory per document id it names. Having nothing to answer
                // with is exactly the case the test below already covers.
                repo = CollabRepository::open(acc, conversationId, documentId);
        }
        if (repo && !repo->isEmpty())
            account->syncCollabDocument(conversationId, documentId);
        return;
    }
    if (kind == "leave") {
        emitSignal<libjami::ConfigurationSignal::CollaborativeParticipantLeft>(accountId_,
                                                                               conversationId,
                                                                               documentId,
                                                                               from);
        return;
    }
    YrsDocument::Bytes update;
    try {
        auto encoded = root["u"].asString();
        if (encoded.size() > MAX_ENCODED_UPDATE_SIZE) {
            JAMI_WARNING("[Account {}] [Document {}] Dropping a {} byte update from {}: over the limit",
                         accountId_,
                         documentId,
                         encoded.size(),
                         from);
            return;
        }
        update = base64::decode(encoded);
    } catch (const std::exception&) {
        return; // a peer sent something that is not base64
    }
    if (update.empty())
        return;
    // No repository yet if the announcement has not been merged: the update
    // stays in memory and is persisted once the document is opened locally.
    const bool announced = isAnnouncedDocument(conversationId, documentId);
    if (!announced && !admitUnannounced(conversationId, documentId)) {
        // An authorized member can name any id it likes here. Sessions for ids
        // the conversation never announced are therefore capped: past the cap
        // they are dropped rather than allowed to accumulate a YrsDocument each.
        JAMI_WARNING("[Account {}] Dropping an update for unannounced document {} in conversation {}: "
                     "too many unannounced documents already pending",
                     accountId_,
                     documentId,
                     conversationId);
        return;
    }
    auto session = ensureSession(conversationId, documentId, announced);
    if (!session->doc->applyUpdate(update))
        return; // malformed: don't hand it to the clients
    // Not persisted here: the device that produced it checkpoints it into its own
    // repository and it reaches ours through synchronization. Storing it again
    // would keep one copy per member of every single edit.
    emitUpdate(conversationId, documentId, update);
}

void
CollaborativeEditing::setAwareness(const std::string& conversationId,
                                   const std::string& documentId,
                                   const std::string& state)
{
    auto account = account_.lock();
    if (!account)
        return;
    if (state.size() > MAX_ENCODED_AWARENESS_SIZE) {
        JAMI_WARNING("[Account {}] [Document {}] Refusing to broadcast an oversized awareness state",
                     accountId_,
                     documentId);
        return;
    }
    Json::Value root;
    root["cid"] = conversationId;
    root["did"] = documentId;
    root["k"] = "aw";
    root["s"] = state;
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
CollaborativeEditing::onLocalUpdate(const std::shared_ptr<Session>& session, const YrsDocument::Bytes& update)
{
    // Real-time path: broadcast the incremental update to connected members.
    if (auto account = account_.lock()) {
        Json::Value root;
        root["cid"] = session->conversationId;
        root["did"] = session->documentId;
        root["u"] = base64::encode(update);
        account->sendInstantMessage(session->conversationId, {{MIME_TYPE_COLLAB, json::toString(root)}});
    }
    // Durable path: accumulate the update for the next checkpoint.
    queueUpdate(session, update);
}

void
CollaborativeEditing::replayStoredUpdates(const std::shared_ptr<Session>& session)
{
    if (!session->repo)
        return;
    // The updates come from peers' commits, so their content is not ours to
    // trust: a malformed one must cost that one update, not the whole replay.
    for (const auto& encoded : session->repo->updates()) {
        try {
            session->doc->applyUpdate(base64::decode(encoded));
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
    // Replay the updates stored in the document's repository. Nothing is
    // signalled: the caller encodes the converged state and hands it to the
    // client that asked to open the document.
    replayStoredUpdates(session);
    // Only now: a session flagged as loaded is never replayed again, so flagging
    // it before the replay would freeze a partially rebuilt document.
    std::lock_guard<std::mutex> lk(mutex_);
    session->persistedLoaded = true;
    // What is already on disk is not news: the client resolves the attachments
    // of the state it is being handed. Only what arrives afterwards is signalled.
    // The asymmetry is deliberate. For a document being opened the client pulls,
    // asking for each reference it meets while rendering; announcing every stored
    // attachment here would push bytes nobody has asked for yet, on a document the
    // user may never scroll through. The signal exists for the opposite case: an
    // attachment landing in an already open document, which the client has no
    // reason to look for.
    if (session->repo)
        for (auto& id : session->repo->attachmentIds())
            session->knownAttachments.insert(std::move(id));
}

void
CollaborativeEditing::queueUpdate(const std::shared_ptr<Session>& session, const YrsDocument::Bytes& update)
{
    bool capReached = false;
    {
        std::lock_guard<std::mutex> lk(session->pendingMutex);
        // Without a repository nothing will ever drain this: keep the last batch
        // so a repository appearing later still saves recent work, and drop the
        // rest rather than growing without bound.
        if (!session->repo && session->pending.size() >= PENDING_HARD_CAP)
            session->pending.erase(session->pending.begin(), session->pending.begin() + CHECKPOINT_MAX_PENDING);
        session->pending.emplace_back(base64::encode(update));
        capReached = session->pending.size() >= CHECKPOINT_MAX_PENDING;
    }
    // Checkpointing reads the document back, so it must never run inline on the
    // path that just wrote to it: scheduleCheckpoint() only arms a timer.
    if (capReached)
        session->checkpointDue = true;
    scheduleCheckpoint(session, capReached ? std::chrono::seconds(0) : CHECKPOINT_IDLE);
}

void
CollaborativeEditing::scheduleCheckpoint(const std::shared_ptr<Session>& session, std::chrono::seconds delay)
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
        batch.swap(session->pending);
    }
    if (batch.empty())
        return;

    if (session->repo->appendCheckpoint(batch).empty()) {
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

    if (auto account = account_.lock())
        account->syncCollabDocument(session->conversationId, session->documentId);

    // Nothing else ever packs a document repository, and every checkpoint leaves
    // a loose object behind. Off the io thread: packing walks the
    // whole history and would otherwise hold up the checkpoint timers.
    if (++session->sinceCompactCheck >= COMPACT_CHECK_EVERY) {
        session->sinceCompactCheck = 0;
        dht::ThreadPool::io().run([repo = session->repo] {
            if (repo)
                repo->compact();
        });
    }
}

std::vector<CollabRepository::HistoryEntry>
CollaborativeEditing::history(const std::string& conversationId, const std::string& documentId, size_t max)
{
    auto session = findSession(conversationId, documentId);
    auto repo = session ? session->repo : nullptr;
    if (!repo) {
        if (auto account = account_.lock())
            repo = CollabRepository::open(account, conversationId, documentId);
    }
    return repo ? repo->history(max) : std::vector<CollabRepository::HistoryEntry> {};
}

std::string
CollaborativeEditing::documentStateAt(const std::string& conversationId,
                                      const std::string& documentId,
                                      const std::string& commitId)
{
    auto session = findSession(conversationId, documentId);
    auto repo = session ? session->repo : nullptr;
    if (!repo) {
        if (auto account = account_.lock())
            repo = CollabRepository::open(account, conversationId, documentId);
    }
    if (!repo)
        return {};

    // Nothing at all when the checkpoint is unknown, which is what the public
    // contract promises. It has to be told apart from a checkpoint that exists
    // and holds nothing: the two would otherwise be the same answer, and a
    // client restoring an early, legitimately empty version could not tell
    // whether it was allowed to.
    const auto stored = repo->updatesAt(commitId);
    if (!stored)
        return {};

    // Replay into a throwaway replica: the live document must not be touched.
    // What the client does with that state -- show it, restore it, diff it -- is
    // its own business, and depends on a document type the daemon ignores.
    YrsDocument snapshot {replicaId()};
    for (const auto& encoded : *stored) {
        try {
            snapshot.applyUpdate(base64::decode(encoded));
        } catch (const std::exception& e) {
            JAMI_WARNING("[Account {}] [Document {}] Skipping unreadable stored update: {}",
                         accountId_,
                         documentId,
                         e.what());
        }
    }
    return base64::encode(snapshot.encodeStateAsUpdate());
}

void
CollaborativeEditing::onDocumentAnnounced(const std::string& conversationId, const std::string& documentId)
{
    auto account = account_.lock();
    if (!account)
        return;
    {
        std::lock_guard<std::mutex> lk(announcedMtx_);
        announced_[conversationId].emplace(documentId);
    }
    // Create the local repository if needed so the document can be replicated.
    // Nothing is loaded in memory until the document is actually opened.
    auto repo = CollabRepository::openOrInit(account, conversationId, documentId);
    if (!repo)
        return;
    // Live updates may have opened a session before the announcement landed; it
    // has no repository, so give it this one and persist what it accumulated.
    if (auto session = findSession(conversationId, documentId)) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!session->repo)
            session->repo = repo;
    }
    // This also runs while paging older messages back in, and a conversation may
    // announce many documents: without a guard, scrolling would cost one
    // broadcast and one fetch per document, for every member, every time.
    //
    // Gating on "the repository is empty" would be wrong the other way round: a
    // device that was offline while others edited has a non-empty but stale
    // repository, and checkpoint notifications are best-effort and not stored in
    // the swarm, so it would never catch up. Sync once per document per
    // registration instead: cheap while browsing, and it still resynchronizes
    // every document each time the account comes back online.
    //
    // What we send is a request, not a notification: announcing our own head
    // makes the others pull from us, which is the wrong direction for a replica
    // that is behind.
    {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!syncedDocuments_.emplace(key(conversationId, documentId)).second)
            return;
    }
    // Send from another thread: addToHistory() announces documents while it holds
    // the conversation lock, and requesting a document sends a message, which
    // takes that same lock again. Doing it inline self-deadlocks the caller.
    dht::ThreadPool::io().run([w = account_, conversationId, documentId] {
        if (auto account = w.lock())
            account->requestCollabDocument(conversationId, documentId);
    });
}

void
CollaborativeEditing::onRepositoryUpdated(const std::string& conversationId, const std::string& documentId)
{
    // A remote rename can land on a document nobody has open here, and both
    // early returns below are reachable in that case: drop the cached name first
    // or a client would keep showing the old one until the account restarts.
    {
        std::lock_guard<std::mutex> lk(mutex_);
        nameCache_.erase(key(conversationId, documentId));
        ++nameEpoch_;
    }
    auto session = findSession(conversationId, documentId);
    if (!session || !session->repo)
        return; // not being edited here; the repository is up to date on disk
    {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!session->persistedLoaded)
            return; // never opened: it will be replayed on open
    }
    // What the replica knows before the replay, so that what it learns from it
    // can be told apart from what it already had.
    const auto before = session->doc->encodeStateVector();
    session->doc->takeChanged();
    // Applying an update the replica already knows is a no-op for a CRDT, so
    // replaying the whole set is correct, just more work than strictly needed.
    replayStoredUpdates(session);
    // Nothing at all when the replay taught us nothing -- a rename-only commit,
    // or updates the real-time path had already delivered -- or every client
    // would light an "unread" badge for a document nobody touched. The question
    // is put to yrs rather than answered by measuring the diff: yrs appends the
    // whole deletion set to a diff without diffing it, so a diff carrying no new
    // content still measures a few bytes on any document where a character was
    // ever erased.
    //
    // What is then sent is only what the replay brought, not the whole document:
    // a synchronization usually carries a handful of keystrokes, and re-encoding
    // a 300 kB document for each of them would push megabytes a minute through
    // the client API for nothing.
    if (session->doc->takeChanged())
        emitUpdate(conversationId, documentId, session->doc->encodeDiff(before));
    // Independent of the updates above: an attachment is not part of the CRDT,
    // so a synchronization can bring the payload of a reference the real-time
    // path delivered long before, with no update at all to show for it.
    emitNewAttachments(session);
    // The name travels with the repository now, so a remote rename lands here.
    // It cannot be detected by reading the name around the replay: the caller
    // merges before calling us, so both reads would return the name from after
    // the merge. What we compare against is the last name we told the clients.
    const auto name = session->repo->meta().displayName;
    auto renamed = false;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        nameCache_[key(conversationId, documentId)] = name;
        ++nameEpoch_;
        // Only a name the clients have already been told can be seen to change.
        if (session->announcedName && *session->announcedName != name)
            renamed = true;
        if (session->announcedName)
            session->announcedName = name;
    }
    if (renamed)
        emitRename(conversationId, documentId, name);
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
        // The account is going away: let every document be synchronized again
        // when it comes back, so a device that was offline catches up.
        syncedDocuments_.clear();
    }
    for (const auto& session : sessions) {
        if (session->checkpointTimer) {
            std::lock_guard<std::mutex> lk(session->timerMutex);
            session->checkpointTimer->cancel();
        }
        // No compaction here: the edits are already durable, packing is pure
        // housekeeping, and doing it inline would stall the account
        // unregistration for seconds per open document.
        checkpointNow(session);
    }
}

void
CollaborativeEditing::emitUpdate(const std::string& conversationId,
                                 const std::string& documentId,
                                 const YrsDocument::Bytes& update)
{
    // One channel for every document type: the payload is an opaque Y-CRDT
    // update, and the client merges it into its own replica. That is what makes
    // a plain-text editor and a rich-text editor listen to the same signal.
    emitSignal<libjami::ConfigurationSignal::CollaborativeDocumentUpdate>(accountId_,
                                                                          conversationId,
                                                                          documentId,
                                                                          base64::encode(update));
}

void
CollaborativeEditing::emitRename(const std::string& conversationId,
                                 const std::string& documentId,
                                 const std::string& name)
{
    emitSignal<libjami::ConfigurationSignal::CollaborativeDocumentRenamed>(accountId_, conversationId, documentId, name);
}

void
CollaborativeEditing::emitNewAttachments(const std::shared_ptr<Session>& session)
{
    if (!session->repo)
        return;
    std::vector<std::string> fresh;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        for (auto& id : session->repo->attachmentIds())
            if (session->knownAttachments.insert(id).second)
                fresh.push_back(std::move(id));
    }
    for (const auto& id : fresh)
        emitSignal<libjami::ConfigurationSignal::CollaborativeAttachmentAdded>(accountId_,
                                                                               session->conversationId,
                                                                               session->documentId,
                                                                               id);
}

std::string
CollaborativeEditing::addAttachment(const std::string& conversationId,
                                    const std::string& documentId,
                                    const std::vector<uint8_t>& data)
{
    if (data.empty() || data.size() > CollabRepository::MAX_ATTACHMENT_SIZE) {
        JAMI_WARNING("[Account {}] [Document {}] Attachment refused: {} byte(s), limit is {}",
                     accountId_,
                     documentId,
                     data.size(),
                     CollabRepository::MAX_ATTACHMENT_SIZE);
        return {};
    }
    auto session = ensureSession(conversationId, documentId);
    if (!session || !session->repo)
        return {};
    auto id = session->repo->addAttachment(data);
    if (id.empty())
        return {};
    {
        // Ours already: the client that stored it holds the bytes, and the next
        // synchronization must not announce them back to it.
        std::lock_guard<std::mutex> lk(mutex_);
        session->knownAttachments.insert(id);
    }
    // The reference travels on the real-time path and the payload with the
    // repository, so peers would show a placeholder until the next checkpoint --
    // ten seconds of nothing, or much longer on a document nobody is typing in.
    // Announce straight away instead.
    if (auto account = account_.lock())
        account->syncCollabDocument(conversationId, documentId);
    return id;
}

std::vector<uint8_t>
CollaborativeEditing::attachment(const std::string& conversationId,
                                 const std::string& documentId,
                                 const std::string& attachmentId)
{
    auto session = findSession(conversationId, documentId);
    auto repo = session ? session->repo : nullptr;
    if (!repo) {
        // Readable without an editing session: a client browsing the history of
        // a document it has not opened still has to resolve what it refers to.
        if (auto account = account_.lock())
            repo = CollabRepository::open(account, conversationId, documentId);
    }
    return repo ? repo->attachment(attachmentId) : std::vector<uint8_t> {};
}

} // namespace jami
