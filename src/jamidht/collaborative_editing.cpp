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
#include "string_utils.h"

#include <opendht/thread_pool.h>

#include <algorithm>
#include <chrono>
#include <functional>
#include <future>
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

// What a document holds when its creator did not say: the simplest thing an
// editor can be pointed at.
static constexpr const char DEFAULT_DOC_MIME_TYPE[] = "text/plain";

// Ceilings on what the repository is asked to hold per item. A name is a label,
// not a place to keep content; an attachment is bounded because it is stored
// and replicated whole.
static constexpr size_t MAX_DOCUMENT_NAME_SIZE {256};
static constexpr size_t MAX_ATTACHMENT_SIZE {16 * 1024 * 1024};

// Key added to a document listing, alongside those read from the announcing
// commit: whether this device still holds the document. Part of the client API,
// documented on getCollaborativeDocuments().
static constexpr char DOCUMENT_STORED_LOCALLY[] = "storedLocally";

// Ceilings on what a single message may carry. Both the client API and the swarm
// hand us opaque blobs, and both decode into memory before the engine gets a say.
// The point is not to guess a "correct" size but to keep one message from being
// able to allocate without bound: a CRDT update stays well under a megabyte even
// when it carries the whole state of a large document, and an awareness state is
// a cursor plus a display name.
static constexpr size_t MAX_UPDATE_SIZE {8 * 1024 * 1024};
static constexpr size_t MAX_AWARENESS_SIZE {8 * 1024};
/// A whole awareness message may carry one entry per peer, not just ours.
static constexpr size_t MAX_AWARENESS_MESSAGE_SIZE {64 * 1024};
/// base64 costs 4 bytes per 3, plus padding and any line breaks a client adds.
static constexpr size_t MAX_ENCODED_UPDATE_SIZE {MAX_UPDATE_SIZE / 3 * 4 + 1024};

// Awareness upkeep, with the cadence the yjs protocol settled on. A state is
// re-announced well before it is due to expire, so that losing one announcement
// does not make an editor blink out of the document; a peer that announces
// nothing for a whole timeout is one whose device is gone, not one who stopped
// typing, and its cursor is withdrawn.
static constexpr std::chrono::seconds AWARENESS_TIMEOUT {30};
static constexpr std::chrono::seconds AWARENESS_RENEW {AWARENESS_TIMEOUT / 2};
static constexpr std::chrono::seconds AWARENESS_SWEEP {AWARENESS_TIMEOUT / 10};
/// How many peers may hold a state in one document at once. An authorized member
/// picks its own client ids, so nothing but this stops one from filling the
/// table with ids nobody is behind.
static constexpr size_t MAX_AWARENESS_PEERS {256};

namespace {

// Cap a document name, cutting on a code point boundary so the result is still
// valid UTF-8 and can be put back into JSON.
std::string
truncatedName(std::string name)
{
    if (name.size() <= MAX_DOCUMENT_NAME_SIZE)
        return name;
    size_t cut = MAX_DOCUMENT_NAME_SIZE;
    while (cut > 0 && (static_cast<unsigned char>(name[cut]) & 0xC0) == 0x80)
        --cut;
    name.resize(cut);
    return name;
}

// Conversation, document, commit and device ids are all generated as
// fixed-length lowercase hexadecimal strings. Enforcing that alphabet on what a
// peer supplies is what keeps an id from being read as anything but an id.
bool
isValidId(std::string_view id)
{
    if (id.empty() || id.size() > 64)
        return false;
    return std::all_of(id.begin(), id.end(), [](unsigned char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}

} // namespace

/// What one client id is currently sharing in a document.
struct AwarenessPeer
{
    uint64_t clock {0};
    /// JSON, or empty once the client withdrew its state.
    std::string state;
    std::chrono::steady_clock::time_point lastSeen;
    /// The account that announced this client id. Held so that a member cannot
    /// speak for a client id another one already claimed, and so that a state
    /// can still be attributed to a person when it reaches the clients.
    std::string owner;
};

struct CollaborativeEditing::Session
{
    std::string conversationId;
    std::string documentId;
    std::unique_ptr<YrsDocument> doc;
    // Whether the conversation had announced the document when the session was
    // built, i.e. whether this one counts against the unannounced cap. Flipped
    // once the announcement lands. Guarded by mutex_.
    bool announced {true};
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

    // y-protocol state. Kept under its own lock: the upkeep timer walks it from
    // the io thread while the clients write to it, and neither has any business
    // waiting on the manager-wide lock to do so.
    std::mutex protocolMutex;
    // Devices we already answered a state vector to. Both sides of a pair have
    // to offer theirs for the two to converge, but answering an offer with
    // another one unconditionally would have them trade offers forever; a device
    // is therefore offered ours once, and only ever answered afterwards.
    std::set<std::string> syncedDevices;
    std::map<uint64_t, AwarenessPeer> awareness;
    /// This device's own entry in the table above. Its clock is what tells peers
    /// which of two states they hold is the later one, so it only ever grows.
    uint64_t localClock {0};
    std::string localState;
    std::chrono::steady_clock::time_point localAnnounced;
    std::unique_ptr<asio::steady_timer> awarenessTimer;
    bool upkeepRunning {false};
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

std::shared_ptr<Conversation>
CollaborativeEditing::documentConversation(const std::string& documentId)
{
    auto account = account_.lock();
    if (!account)
        return nullptr;
    auto* cm = account->convModule(true);
    if (!cm)
        return nullptr;
    auto conversation = cm->getConversation(documentId);
    if (!conversation || conversation->mode() != ConversationMode::DOCUMENT)
        return nullptr;
    return conversation;
}

std::shared_ptr<CollaborativeEditing::Session>
CollaborativeEditing::findSession(const std::string& conversationId, const std::string& documentId)
{
    std::lock_guard<std::mutex> lk(mutex_);
    auto it = sessions_.find(key(conversationId, documentId));
    return it != sessions_.end() ? it->second : nullptr;
}

std::shared_ptr<CollaborativeEditing::Session>
CollaborativeEditing::ensureSession(const std::string& conversationId, const std::string& documentId, bool announced)
{
    std::lock_guard<std::mutex> lk(mutex_);
    auto k = key(conversationId, documentId);
    if (auto it = sessions_.find(k); it != sessions_.end()) {
        // A session created from live updates predates the announcement; stop
        // counting it against the unannounced cap once the document is known.
        if (announced)
            it->second->announced = true;
        return it->second;
    }

    auto session = std::make_shared<Session>();
    session->conversationId = conversationId;
    session->documentId = documentId;
    session->announced = announced;
    session->doc = std::make_unique<YrsDocument>(replicaId());
    session->checkpointTimer = std::make_unique<asio::steady_timer>(*ioContext_);
    session->awarenessTimer = std::make_unique<asio::steady_timer>(*ioContext_);
    sessions_.emplace(k, session);
    return session;
}

std::string
CollaborativeEditing::createDocument(const std::string& conversationId,
                                     const std::string& name,
                                     const std::string& mimeType)
{
    // Settle the media type here rather than in each of the two places that
    // record it: an empty one in the initial commit and none at all in the
    // announcement would list the document as having no type while its
    // repository claimed one.
    const std::string type = mimeType.empty() ? DEFAULT_DOC_MIME_TYPE : mimeType;

    auto account = account_.lock();
    if (!account)
        return {};
    auto* cm = account->convModule();
    if (!cm)
        return {};
    // The document gets a swarm repository of its own, holding its content and
    // history; its id is the repository's. The creator is its first member.
    auto documentId = cm->startDocument(conversationId, type);
    if (documentId.empty()) {
        JAMI_ERROR("[Account {}] Unable to create a document repository in conversation {}", accountId_, conversationId);
        return {};
    }

    // The name describes the document, it is not part of its content: it lives
    // in the repository's profile, like a conversation's title, and reaches the
    // other holders through the ordinary repository synchronization.
    const auto stored = truncatedName(name);
    if (!stored.empty())
        cm->updateConversationInfos(documentId, {{"title", stored}}, false);

    auto session = ensureSession(conversationId, documentId);
    {
        std::lock_guard<std::mutex> lk(mutex_);
        session->persistedLoaded = true; // the repository is newborn: nothing to replay
        session->announcedName = stored;
        nameCache_[key(conversationId, documentId)] = stored;
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
    auto announceResult = std::make_shared<std::promise<bool>>();
    auto announcedDone = announceResult->get_future();
    cm->createCommit(conversationId,
                     CommitMessage::collabDocCreated(documentId, stored, type),
                     true,
                     {},
                     [w = weak_from_this(),
                      conversationId,
                      documentId,
                      accountId = accountId_,
                      announceResult](bool ok, const std::string&) {
                         announceResult->set_value(ok);
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
                             sthis->dropLocalReplica(conversationId, documentId);
                             // Undo startDocument() too: a repository nobody was
                             // ever told about would still be reloaded as held on
                             // every restart.
                             if (auto account = sthis->account_.lock())
                                 if (auto* cm = account->convModule())
                                     cm->removeDocumentReplica(documentId);
                         }
                     });
    // The commit is written on another thread; wait for it, so that a document
    // this call hands back is really there: listed in the conversation, and
    // announced to the members. Without this, creating a document then listing
    // the conversation's documents would be a race against our own commit.
    if (announcedDone.wait_for(std::chrono::seconds(30)) != std::future_status::ready) {
        // Never resolving would mean the conversation vanished under us and the
        // commit callback was dropped; the rollback above cannot run either, so
        // all that is left is to say the announcement never happened.
        JAMI_ERROR("[Account {}] Document {} announcement never completed in conversation {}",
                   accountId_,
                   documentId,
                   conversationId);
        return {};
    }
    if (!announcedDone.get())
        return {};
    return documentId;
}

bool
CollaborativeEditing::removeDocument(const std::string& conversationId, const std::string& documentId)
{
    auto account = account_.lock();
    if (!account)
        return false;
    auto* cm = account->convModule();
    if (!cm)
        return false;
    // Removing a document means retiring the commit that announced it, so that
    // commit has to be found first. Its id is also the only thing the removal
    // carries: the swarm ties an edition to the author of what it edits, which is
    // what keeps a member from retiring somebody else's document.
    std::string announcementId;
    for (const auto& doc : documents(conversationId)) {
        auto idIt = doc.find("id");
        if (idIt == doc.end() || idIt->second != documentId)
            continue;
        if (auto annIt = doc.find("announcement"); annIt != doc.end())
            announcementId = annIt->second;
        break;
    }
    if (announcementId.empty()) {
        JAMI_WARNING("[Account {}] [Document {}] Not removing: no announcement found in conversation {}",
                     accountId_,
                     documentId,
                     conversationId);
        return false;
    }
    // The peers apply the removal when the commit reaches them, and this device
    // does the same through addToHistory() once the commit lands locally: nothing
    // is erased here, so a commit that never happens leaves the document intact.
    // editMessage() is what checks that we authored the announcement, exactly as
    // it does for a shared file.
    cm->editMessage(conversationId, {}, announcementId);
    return true;
}

bool
CollaborativeEditing::removeDocumentLocally(const std::string& conversationId, const std::string& documentId)
{
    // Only documents the conversation announced: anything else is not a
    // document of this conversation, whatever a caller cares to name.
    if (!isAnnouncedDocument(conversationId, documentId)) {
        JAMI_WARNING("[Account {}] [Document {}] Not removing from this device: it was not announced in "
                     "conversation {}",
                     accountId_,
                     documentId,
                     conversationId);
        return false;
    }
    // Removal is the replica going away, nothing more: no marker survives it.
    // Whether this device holds a document is simply whether its repository is
    // here, and nothing replicates one this device did not ask for -- an
    // announcement paged back in records an id, a peer cannot push a clone, and
    // a commit notification for an unheld repository is ignored.
    if (auto account = account_.lock())
        if (auto* cm = account->convModule())
            cm->removeDocumentReplica(documentId);
    dropLocalReplica(conversationId, documentId);
    emitSignal<libjami::ConfigurationSignal::CollaborativeDocumentRemoved>(accountId_,
                                                                           conversationId,
                                                                           documentId,
                                                                           false);
    return true;
}

void
CollaborativeEditing::setName(const std::string& conversationId, const std::string& documentId, const std::string& name)
{
    auto account = account_.lock();
    if (!account)
        return;
    auto* cm = account->convModule();
    if (!cm)
        return;
    if (!documentConversation(documentId)) {
        JAMI_WARNING("[Account {}] [Document {}] Unable to rename: this device does not hold the document",
                     accountId_,
                     documentId);
        return;
    }
    // The name describes the document, it is not part of its content: keeping it
    // in the repository's profile rather than inside the CRDT is what lets the
    // daemon stay blind to what the document holds. It reaches the other holders
    // through the ordinary repository synchronization, and the swarm's own
    // validation is what restricts who may write it.
    //
    // The repository caps what it stores, so remember and announce what it really
    // holds, or the cache would answer a name no other member will ever see.
    const auto stored = truncatedName(name);
    cm->updateConversationInfos(documentId, {{"title", stored}}, true);
    {
        std::lock_guard<std::mutex> lk(mutex_);
        if (auto it = sessions_.find(key(conversationId, documentId)); it != sessions_.end())
            it->second->announcedName = stored;
        nameCache_[key(conversationId, documentId)] = stored;
        ++nameEpoch_;
    }
    emitRename(conversationId, documentId, stored);
}

std::string
CollaborativeEditing::documentName(const std::string& conversationId, const std::string& documentId)
{
    // Reading a name must stay cheap. Clients ask for it constantly: once per
    // document to list a conversation's documents, and once per message delegate
    // built while scrolling a conversation. Reading it from the repository's
    // profile means git lookups on the caller's thread -- the client's UI thread
    // -- so the answer is cached, and the cache is refreshed wherever the name
    // can change: in setName and on synchronization.
    const auto k = key(conversationId, documentId);
    uint64_t epoch = 0;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        if (auto it = nameCache_.find(k); it != nameCache_.end())
            return it->second;
        epoch = nameEpoch_;
    }
    auto conversation = documentConversation(documentId);
    if (!conversation)
        return {}; // not held here: nothing worth remembering
    std::string name;
    auto infos = conversation->infos();
    if (auto it = infos.find("title"); it != infos.end())
        name = it->second;
    std::lock_guard<std::mutex> lk(mutex_);
    // Only remember it if nothing invalidated the cache while we were reading.
    // A synchronization landing during the read above would otherwise be
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
    auto docs = conversation->collaborativeDocuments();
    // The conversation knows which documents exist; whether this device still
    // holds one is whether its repository is here. A client has to be able to
    // tell them apart: one opens on what is already here, the other has to be
    // fetched back first.
    for (auto& doc : docs) {
        auto it = doc.find("id");
        doc[DOCUMENT_STORED_LOCALLY] = (it != doc.end() && documentConversation(it->second)) ? TRUE_STR : FALSE_STR;
    }
    return docs;
}

bool
CollaborativeEditing::isAnnouncedDocument(const std::string& conversationId, const std::string& documentId)
{
    // A document only exists once a COLLAB_DOC commit announced it in the
    // conversation. Without this check, a member could name arbitrary ids in
    // instant messages and have us create a bare repository on disk for each.
    {
        std::lock_guard<std::mutex> lk(announcedMtx_);
        if (auto rmIt = removed_.find(conversationId); rmIt != removed_.end() && rmIt->second.count(documentId) != 0)
            return false;
        if (auto it = announced_.find(conversationId); it != announced_.end())
            return it->second.count(documentId) != 0;
    }
    // First question asked about this conversation: walking its whole history is
    // expensive, so do it once and let onDocumentAnnounced() keep the set fresh.
    std::set<std::string> ids;
    for (const auto& doc : documents(conversationId)) {
        if (auto it = doc.find("id"); it != doc.end())
            ids.emplace(it->second);
    }
    std::lock_guard<std::mutex> lk(announcedMtx_);
    auto& set = announced_[conversationId];
    set.merge(ids);
    // documents() already drops the retired ones, so a removal met earlier cannot
    // be undone by this merge; but a removal recorded meanwhile still wins.
    if (auto rmIt = removed_.find(conversationId); rmIt != removed_.end() && rmIt->second.count(documentId) != 0)
        return false;
    return set.count(documentId) != 0;
}

bool
CollaborativeEditing::knowsDocument(const std::string& documentId)
{
    std::lock_guard<std::mutex> lk(announcedMtx_);
    for (const auto& [_, ids] : announced_)
        if (ids.count(documentId) != 0)
            return true;
    return false;
}

bool
CollaborativeEditing::isRemovedDocument(const std::string& conversationId, const std::string& documentId)
{
    std::lock_guard<std::mutex> lk(announcedMtx_);
    auto it = removed_.find(conversationId);
    return it != removed_.end() && it->second.count(documentId) != 0;
}

void
CollaborativeEditing::dropLocalReplica(const std::string& conversationId, const std::string& documentId)
{
    std::shared_ptr<Session> session;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        auto k = key(conversationId, documentId);
        if (auto it = sessions_.find(k); it != sessions_.end()) {
            session = it->second;
            sessions_.erase(it);
        }
        nameCache_.erase(k);
        ++nameEpoch_;
    }
    // Drop the live replica without checkpointing it first: the repository is
    // going with it, and writing to it would only race with the erase. The
    // timer is cancelled for the same reason -- it would fire on a repository
    // that no longer exists.
    if (session && session->checkpointTimer) {
        std::lock_guard<std::mutex> lk(session->timerMutex);
        session->checkpointTimer->cancel();
    }
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
        if (session && !session->announced)
            ++unannounced;
    }
    return unannounced < MAX_UNANNOUNCED;
}

YrsDocument::Bytes
CollaborativeEditing::openDocument(const std::string& conversationId, const std::string& documentId)
{
    // A document only exists once the conversation announced it. Opening one
    // that was never announced would clone from any id a caller cares to name,
    // and bypass the very gate that decides which documents exist here.
    if (!isAnnouncedDocument(conversationId, documentId)) {
        JAMI_WARNING("[Account {}] Refusing to open document {}: it was not announced in conversation {}",
                     accountId_,
                     documentId,
                     conversationId);
        return {};
    }
    auto account = account_.lock();
    if (!account)
        return {};
    auto* cm = account->convModule();
    if (!cm)
        return {};
    auto conversation = documentConversation(documentId);
    if (!conversation) {
        // Opening is what opts this device into holding a replica: clone the
        // document's swarm from its announcer. The clone lands asynchronously
        // -- through the very pipeline a conversation invite uses -- and
        // reports through onRepositoryUpdated(), which replays it into this
        // session and hands the client the difference. Until then the document
        // is open and empty, exactly like a conversation still syncing.
        std::string announcer;
        for (const auto& doc : documents(conversationId)) {
            auto idIt = doc.find("id");
            if (idIt == doc.end() || idIt->second != documentId)
                continue;
            if (auto authorIt = doc.find("author"); authorIt != doc.end())
                announcer = authorIt->second;
            break;
        }
        if (announcer.empty()) {
            JAMI_WARNING("[Account {}] Unable to open document {}: its announcer is unknown", accountId_, documentId);
            return {};
        }
        auto session = ensureSession(conversationId, documentId);
        {
            // Nothing on disk yet, so nothing to replay; flagging it now is
            // what lets the clone's completion replay into this session.
            std::lock_guard<std::mutex> lk(mutex_);
            session->persistedLoaded = true;
        }
        cm->cloneDocumentFrom(documentId, announcer);
        // Live edits are not gated on the clone: members with the document open
        // answer this with what this replica is missing.
        sendFrame(conversationId, documentId, yprotocol::syncStep1(session->doc->encodeStateVector()));
        return session->doc->encodeStateAsUpdate();
    }
    auto session = ensureSession(conversationId, documentId);
    // Rebuild the CRDT state from persisted commits if this session was just created,
    // so a document opens with its full content even when the daemon restarted or the
    // commits were never replayed yet.
    loadPersistedState(session);
    // Remember the name the client is about to see, so a later rename can be
    // told from it.
    {
        auto infos = conversation->infos();
        auto it = infos.find("title");
        const auto name = it != infos.end() ? it->second : std::string {};
        std::lock_guard<std::mutex> lk(mutex_);
        if (!session->announcedName)
            session->announcedName = name;
    }
    auto state = session->doc->encodeStateAsUpdate();
    // Offer the members what this replica already holds, so that the ones with
    // the document open answer with just the difference. This is also what makes
    // a second device of the same account catch up on edits that were made while
    // it was not looking, without waiting for the next checkpoint to be fetched.
    sendFrame(conversationId, documentId, yprotocol::syncStep1(session->doc->encodeStateVector()));
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
    // Withdraw this device's awareness state, which is what clears its cursor
    // for the other members. Done explicitly rather than left to expire so that
    // closing an editor is seen at once instead of a timeout later.
    publishAwareness(session, {});
    {
        std::lock_guard<std::mutex> lk(session->protocolMutex);
        // Nothing has been offered to these devices any more: a later reopen has
        // to start the exchange over rather than assume it already happened.
        session->syncedDevices.clear();
    }
}

void
CollaborativeEditing::applyUpdate(const std::string& conversationId,
                                  const std::string& documentId,
                                  const YrsDocument::Bytes& update)
{
    auto session = findSession(conversationId, documentId);
    if (!session)
        return;
    if (update.size() > MAX_UPDATE_SIZE) {
        JAMI_WARNING("[Account {}] [Document {}] Discarding a {} byte update from the client: "
                     "over the {} byte limit",
                     accountId_,
                     documentId,
                     update.size(),
                     MAX_UPDATE_SIZE);
        return;
    }
    // Merge before forwarding: an update the engine rejects must not be sent to
    // the members nor written to the repository.
    if (!session->doc->applyUpdate(update))
        return;
    onLocalUpdate(session, update);
}

YrsDocument::Bytes
CollaborativeEditing::documentState(const std::string& conversationId, const std::string& documentId)
{
    auto session = findSession(conversationId, documentId);
    return session ? session->doc->encodeStateAsUpdate() : YrsDocument::Bytes {};
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
    if (!isValidId(conversationId) || !isValidId(documentId))
        return;

    // Being able to send us a message is not the same as being allowed to edit
    // this document: a plain contact could otherwise inject text into the live
    // replicas of a swarm it does not belong to.
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

    // "k" tells a y-protocol frame ("y") apart from whatever a newer revision
    // of this envelope may carry. Repository synchronization is not driven from
    // here: a document is a swarm, and its commits announce themselves through
    // the ordinary conversation pipeline.
    auto kind = root.get("k", "y").asString();
    if (kind != "y")
        return; // nothing else is spoken here

    yprotocol::Bytes frame;
    try {
        auto encoded = root["m"].asString();
        if (encoded.size() > MAX_ENCODED_UPDATE_SIZE) {
            JAMI_WARNING("[Account {}] [Document {}] Dropping a {} byte frame from {}: over the limit",
                         accountId_,
                         documentId,
                         encoded.size(),
                         from);
            return;
        }
        frame = base64::decode(encoded);
    } catch (const std::exception&) {
        return; // a peer sent something that is not base64
    }
    if (frame.empty())
        return;

    yprotocol::Decoder decoder(frame);
    uint64_t messageType = 0;
    if (!decoder.readVarUint(messageType))
        return;
    switch (static_cast<yprotocol::Message>(messageType)) {
    case yprotocol::Message::SYNC:
        onSyncMessage(conversationId, documentId, from, fromDevice, decoder);
        break;
    case yprotocol::Message::AWARENESS:
        if (auto session = admitSession(conversationId, documentId))
            onAwarenessMessage(session, from, decoder);
        break;
    default:
        // A message type from a newer revision of the protocol. Ignoring it is
        // what the yjs implementations do, and it is what lets one be added
        // without every peer having to be upgraded first.
        break;
    }
}

std::shared_ptr<CollaborativeEditing::Session>
CollaborativeEditing::admitSession(const std::string& conversationId, const std::string& documentId)
{
    // No replica is held for the document if this device does not hold its
    // repository: what arrives stays in memory and is persisted only where a
    // repository exists to receive it.
    //
    // A document known to be removed is refused outright: it can never be opened
    // again, so a replica held for it would accumulate what nothing would ever
    // read, and peers that have not seen the removal yet keep sending updates.
    if (isRemovedDocument(conversationId, documentId))
        return nullptr;
    const bool announced = isAnnouncedDocument(conversationId, documentId);
    if (!announced && !admitUnannounced(conversationId, documentId)) {
        // An authorized member can name any id it likes here. Sessions for ids
        // the conversation never announced are therefore capped: past the cap
        // they are dropped rather than allowed to accumulate a YrsDocument each.
        JAMI_WARNING("[Account {}] Dropping a collaborative message for unannounced document {} in "
                     "conversation {}: too many unannounced documents already pending",
                     accountId_,
                     documentId,
                     conversationId);
        return nullptr;
    }
    return ensureSession(conversationId, documentId, announced);
}

void
CollaborativeEditing::onSyncMessage(const std::string& conversationId,
                                    const std::string& documentId,
                                    const std::string& from,
                                    const std::string& fromDevice,
                                    yprotocol::Decoder& decoder)
{
    uint64_t step = 0;
    const uint8_t* payload = nullptr;
    size_t payloadSize = 0;
    if (!decoder.readVarUint(step) || !decoder.readVarUint8Array(payload, payloadSize))
        return;

    if (static_cast<yprotocol::Sync>(step) == yprotocol::Sync::STEP1) {
        // A peer says what it holds. Answered only when this device has the
        // document open: waking every member to build a replica each time
        // someone opens a document would cost far more than it is worth, and a
        // member that is not editing has nothing the repository does not already
        // carry.
        auto session = findSession(conversationId, documentId);
        if (!session)
            return;
        yprotocol::Bytes stateVector(payload, payload + payloadSize);
        auto diff = session->doc->encodeDiff(stateVector);
        sendFrame(conversationId, documentId, yprotocol::syncStep2(diff), from, fromDevice);
        // Offer ours in return, once per device: without it the exchange is
        // one-sided and this replica's own edits would only reach that peer at
        // the next checkpoint.
        bool offer = false;
        {
            std::lock_guard<std::mutex> lk(session->protocolMutex);
            offer = session->syncedDevices.insert(fromDevice).second;
        }
        if (offer)
            sendFrame(conversationId,
                      documentId,
                      yprotocol::syncStep1(session->doc->encodeStateVector()),
                      from,
                      fromDevice);
        return;
    }

    // STEP2 and UPDATE both carry an update to merge; they differ only in what
    // prompted them. Nothing else does, so nothing else is merged: handing an
    // unknown sub-type to the engine would mean guessing that a later revision
    // of the protocol, or a malformed frame, happens to carry an update there.
    const auto sync = static_cast<yprotocol::Sync>(step);
    if (sync != yprotocol::Sync::STEP2 && sync != yprotocol::Sync::UPDATE) {
        JAMI_WARNING("[Account {}] [Document {}] Ignoring sync message of unknown sub-type {}",
                     accountId_,
                     documentId,
                     step);
        return;
    }
    if (payloadSize == 0)
        return;
    auto session = admitSession(conversationId, documentId);
    if (!session)
        return;
    YrsDocument::Bytes update(payload, payload + payloadSize);
    // Cleared before the update rather than read after it alone, so that what
    // this update brought is not confused with what an earlier one did.
    session->doc->takeChanged();
    if (!session->doc->applyUpdate(update))
        return; // malformed: don't hand it to the clients
    // Nothing when the update taught the replica nothing. Opening a document
    // makes a peer send us the state it holds, which is usually the state we
    // already have: applying it succeeds and changes nothing, yet forwarding it
    // lit an "unread" badge on a conversation whose document nobody had touched.
    // The synchronization path guards itself the same way, for the same reason.
    if (!session->doc->takeChanged())
        return;
    // Not persisted here: the device that produced it checkpoints it into its own
    // repository and it reaches ours through synchronization. Storing it again
    // would keep one copy per member of every single edit.
    emitUpdate(conversationId, documentId, update);
}

void
CollaborativeEditing::onAwarenessMessage(const std::shared_ptr<Session>& session,
                                         const std::string& from,
                                         yprotocol::Decoder& decoder)
{
    const uint8_t* body = nullptr;
    size_t bodySize = 0;
    if (!decoder.readVarUint8Array(body, bodySize) || bodySize > MAX_AWARENESS_MESSAGE_SIZE)
        return;
    std::vector<yprotocol::AwarenessEntry> entries;
    if (!yprotocol::decodeAwarenessUpdate(body, bodySize, entries))
        return;

    // What the local clients have to be told, gathered while the table is locked
    // and emitted once it is not: a signal handler is entitled to call back into
    // this manager.
    std::vector<std::pair<uint64_t, std::string>> changed;
    std::vector<uint64_t> left;
    bool contested = false;
    const auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lk(session->protocolMutex);
        for (const auto& entry : entries) {
            if (entry.state.size() > MAX_AWARENESS_SIZE)
                continue; // a presence state, not a payload
            if (entry.clientId == clientId_) {
                // Someone is speaking for this device. Nothing legitimate does
                // that, and the protocol's own answer -- outrun it so that peers
                // keep our real state -- is also the right one here.
                contested = true;
                continue;
            }
            auto it = session->awareness.find(entry.clientId);
            if (it != session->awareness.end()) {
                if (it->second.owner != from)
                    continue; // a member may not speak for another's client id
                // Strictly greater: two states with the same clock are the same
                // state having taken two routes, and re-emitting one of them
                // would make a cursor jump back to where it already was.
                if (entry.clock <= it->second.clock)
                    continue;
            } else {
                if (session->awareness.size() >= MAX_AWARENESS_PEERS)
                    continue;
                if (entry.state.empty() || entry.state == "null")
                    continue; // a client that is gone and was never here
            }
            const bool gone = entry.state.empty() || entry.state == "null";
            if (gone) {
                session->awareness.erase(entry.clientId);
                left.push_back(entry.clientId);
            } else {
                auto& peer = session->awareness[entry.clientId];
                peer.clock = entry.clock;
                peer.state = entry.state;
                peer.lastSeen = now;
                peer.owner = from;
                changed.emplace_back(entry.clientId, entry.state);
            }
        }
    }

    for (const auto& [clientId, state] : changed)
        emitSignal<libjami::ConfigurationSignal::CollaborativeAwarenessChanged>(accountId_,
                                                                                session->conversationId,
                                                                                session->documentId,
                                                                                from,
                                                                                clientId,
                                                                                state);
    for (auto clientId : left)
        emitSignal<libjami::ConfigurationSignal::CollaborativeParticipantLeft>(accountId_,
                                                                               session->conversationId,
                                                                               session->documentId,
                                                                               from,
                                                                               clientId);
    if (contested) {
        JAMI_WARNING("[Account {}] [Document {}] {} announced this device's client id; re-announcing",
                     accountId_,
                     session->documentId,
                     from);
        std::string state;
        {
            std::lock_guard<std::mutex> lk(session->protocolMutex);
            state = session->localState;
        }
        publishAwareness(session, state);
    }
    scheduleAwarenessUpkeep(session);
}

void
CollaborativeEditing::sendFrame(const std::string& conversationId,
                                const std::string& documentId,
                                const yprotocol::Bytes& frame,
                                const std::string& peer,
                                const std::string& device)
{
    auto account = account_.lock();
    if (!account || frame.empty())
        return;
    Json::Value root;
    root["cid"] = conversationId;
    root["did"] = documentId;
    root["k"] = "y";
    // The envelope is JSON and the frame is binary, so it travels base64 on this
    // hop. That is a property of the transport the members already share, not of
    // the protocol: what the clients hand over and receive is the bytes.
    root["m"] = base64::encode(frame);
    std::map<std::string, std::string> payload {{MIME_TYPE_COLLAB, json::toString(root)}};
    if (peer.empty()) {
        account->sendInstantMessage(conversationId, payload);
        return;
    }
    // Answering only the device that asked. Handing the whole difference to
    // every member would give away the very saving the state vector just bought.
    //
    // An empty device is a deliberate fallback, not an unaddressed send: it
    // reaches every connected device of that one member, which is still the
    // answer to a question only that member asked. Callers here always know the
    // device, since it comes from the certificate the message was received on.
    std::random_device rd;
    std::mt19937_64 gen(rd());
    auto token = std::uniform_int_distribution<uint64_t> {1, JAMI_ID_MAX_VAL}(gen);
    account->sendMessage(peer, device, payload, token, false, true);
}

void
CollaborativeEditing::setAwareness(const std::string& conversationId,
                                   const std::string& documentId,
                                   const std::string& state)
{
    if (state.size() > MAX_AWARENESS_SIZE) {
        JAMI_WARNING("[Account {}] [Document {}] Refusing to broadcast an oversized awareness state",
                     accountId_,
                     documentId);
        return;
    }
    // Only for a document this device has open: an awareness state is about
    // where its editor is, and there is no editor before that.
    if (auto session = findSession(conversationId, documentId))
        publishAwareness(session, state);
}

void
CollaborativeEditing::publishAwareness(const std::shared_ptr<Session>& session, const std::string& state)
{
    yprotocol::AwarenessEntry entry;
    {
        std::lock_guard<std::mutex> lk(session->protocolMutex);
        // Withdrawing a state that was never shared would tell the members about
        // an editor they were never told about in the first place.
        if (state.empty() && session->localState.empty())
            return;
        entry.clientId = clientId_;
        entry.clock = ++session->localClock;
        // "null" is how the protocol spells a client that is no longer there.
        entry.state = state.empty() ? "null" : state;
        session->localState = state;
        session->localAnnounced = std::chrono::steady_clock::now();
    }
    sendFrame(session->conversationId, session->documentId, yprotocol::awarenessMessage({entry}));
    scheduleAwarenessUpkeep(session);
}

void
CollaborativeEditing::scheduleAwarenessUpkeep(const std::shared_ptr<Session>& session)
{
    if (!session->awarenessTimer)
        return;
    {
        std::lock_guard<std::mutex> lk(session->protocolMutex);
        // One timer at a time, and none at all while nobody is editing: a
        // document nobody has open must not keep waking the process up.
        if (session->upkeepRunning)
            return;
        if (session->awareness.empty() && session->localState.empty())
            return;
        session->upkeepRunning = true;
        session->awarenessTimer->expires_after(AWARENESS_SWEEP);
    }
    std::weak_ptr<CollaborativeEditing> wthis = weak_from_this();
    std::weak_ptr<Session> wsession = session;
    session->awarenessTimer->async_wait([wthis, wsession](const asio::error_code& ec) {
        auto sthis = wthis.lock();
        auto session = wsession.lock();
        if (!sthis || !session)
            return;
        {
            std::lock_guard<std::mutex> lk(session->protocolMutex);
            session->upkeepRunning = false;
        }
        if (!ec)
            sthis->awarenessUpkeep(session);
    });
}

void
CollaborativeEditing::awarenessUpkeep(const std::shared_ptr<Session>& session)
{
    const auto now = std::chrono::steady_clock::now();
    std::vector<std::pair<uint64_t, std::string>> expired;
    std::string renew;
    {
        std::lock_guard<std::mutex> lk(session->protocolMutex);
        for (auto it = session->awareness.begin(); it != session->awareness.end();) {
            if (now - it->second.lastSeen >= AWARENESS_TIMEOUT) {
                expired.emplace_back(it->first, it->second.owner);
                it = session->awareness.erase(it);
            } else {
                ++it;
            }
        }
        // Re-announced well before it would expire elsewhere, so that a single
        // lost message does not make this device blink out of the document.
        if (!session->localState.empty() && now - session->localAnnounced >= AWARENESS_RENEW)
            renew = session->localState;
    }
    for (const auto& [clientId, owner] : expired)
        emitSignal<libjami::ConfigurationSignal::CollaborativeParticipantLeft>(accountId_,
                                                                               session->conversationId,
                                                                               session->documentId,
                                                                               owner,
                                                                               clientId);
    if (!renew.empty())
        publishAwareness(session, renew); // rearms the timer on its way out
    else
        scheduleAwarenessUpkeep(session);
}

void
CollaborativeEditing::onLocalUpdate(const std::shared_ptr<Session>& session, const YrsDocument::Bytes& update)
{
    // Real-time path: hand the incremental update to the connected members.
    sendFrame(session->conversationId, session->documentId, yprotocol::syncUpdate(update));
    // Durable path: accumulate the update for the next checkpoint.
    queueUpdate(session, update);
}

void
CollaborativeEditing::replayStoredUpdates(const std::shared_ptr<Session>& session)
{
    auto conversation = documentConversation(session->documentId);
    if (!conversation)
        return;
    // The updates come from peers' commits, so their content is not ours to
    // trust: a malformed one must cost that one update, not the whole replay.
    for (const auto& encoded : conversation->documentUpdates()) {
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
    // What is already stored is not news: the client resolves the attachments
    // of the state it is being handed. Only what arrives afterwards is signalled.
    // The asymmetry is deliberate. For a document being opened the client pulls,
    // asking for each reference it meets while rendering; announcing every stored
    // attachment here would push bytes nobody has asked for yet, on a document the
    // user may never scroll through. The signal exists for the opposite case: an
    // attachment landing in an already open document, which the client has no
    // reason to look for.
    std::vector<std::string> ids;
    if (auto conversation = documentConversation(session->documentId))
        ids = conversation->documentAttachmentIds();
    // Only now: a session flagged as loaded is never replayed again, so flagging
    // it before the replay would freeze a partially rebuilt document.
    std::lock_guard<std::mutex> lk(mutex_);
    session->persistedLoaded = true;
    for (auto& id : ids)
        session->knownAttachments.insert(std::move(id));
}

void
CollaborativeEditing::queueUpdate(const std::shared_ptr<Session>& session, const YrsDocument::Bytes& update)
{
    const bool held = documentConversation(session->documentId) != nullptr;
    bool capReached = false;
    {
        std::lock_guard<std::mutex> lk(session->pendingMutex);
        // Without a repository nothing will ever drain this: keep the last batch
        // so a repository appearing later still saves recent work, and drop the
        // rest rather than growing without bound.
        if (!held && session->pending.size() >= PENDING_HARD_CAP)
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
    auto account = account_.lock();
    if (!account)
        return;
    auto* cm = account->convModule();
    if (!cm)
        return;
    // Before draining anything: a drained batch with nowhere to go is lost.
    // Keeping the batch queued means a replica appearing later -- the document
    // being opened, which is what clones it -- still saves recent work.
    if (!documentConversation(session->documentId))
        return;
    std::vector<std::string> batch;
    {
        std::lock_guard<std::mutex> lk(session->pendingMutex);
        batch.swap(session->pending);
    }
    if (batch.empty())
        return;

    // The checkpoint is a commit in the document's own swarm: committing it is
    // also what announces it, so the other holders fetch it through the same
    // pipeline that moves conversation messages. Nothing else needs sending.
    cm->createCommit(session->documentId,
                     CommitMessage::checkpoint(batch),
                     true,
                     {},
                     [w = weak_from_this(), wsession = std::weak_ptr<Session>(session), batch](bool ok,
                                                                                               const std::string&) {
                         if (ok)
                             return;
                         // Keep the updates queued so the next checkpoint retries
                         // them rather than silently losing the edits they carry,
                         // and make sure a retry is actually scheduled even if the
                         // user has stopped typing.
                         auto sthis = w.lock();
                         auto session = wsession.lock();
                         if (!sthis || !session)
                             return;
                         {
                             std::lock_guard<std::mutex> lk(session->pendingMutex);
                             session->pending.insert(session->pending.begin(), batch.begin(), batch.end());
                         }
                         sthis->scheduleCheckpoint(session, CHECKPOINT_IDLE);
                     });
}

std::vector<std::map<std::string, std::string>>
CollaborativeEditing::history(const std::string& /*conversationId*/, const std::string& documentId, size_t max)
{
    auto conversation = documentConversation(documentId);
    return conversation ? conversation->documentHistory(max) : std::vector<std::map<std::string, std::string>> {};
}

YrsDocument::Bytes
CollaborativeEditing::documentStateAt(const std::string& /*conversationId*/,
                                      const std::string& documentId,
                                      const std::string& commitId)
{
    auto conversation = documentConversation(documentId);
    if (!conversation)
        return {};

    // Nothing at all when the checkpoint is unknown, which is what the public
    // contract promises. It has to be told apart from a checkpoint that exists
    // and holds nothing: the two would otherwise be the same answer, and a
    // client restoring an early, legitimately empty version could not tell
    // whether it was allowed to.
    const auto stored = conversation->documentUpdatesAt(commitId);
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
    return snapshot.encodeStateAsUpdate();
}

void
CollaborativeEditing::onDocumentAnnounced(const std::string& conversationId, const std::string& documentId)
{
    // The author may have retired this announcement. Answered from the cache
    // alone, never by walking the conversation again: this runs while
    // addToHistory() holds the conversation lock, and asking the conversation
    // anything from here is what deadlocks the caller. addToHistory() applies the
    // removals of a batch before its announcements, and a removal is always newer
    // than the announcement it retires, so the cache is already right by now.
    //
    // Nothing is replicated here: holding a replica is a per-device choice, made
    // by opening the document. The announcement only records that it exists.
    std::lock_guard<std::mutex> lk(announcedMtx_);
    if (auto it = removed_.find(conversationId); it != removed_.end() && it->second.count(documentId) != 0)
        return;
    announced_[conversationId].emplace(documentId);
}

void
CollaborativeEditing::onDocumentRemoved(const std::string& conversationId, const std::string& documentId)
{
    {
        std::lock_guard<std::mutex> lk(announcedMtx_);
        removed_[conversationId].emplace(documentId);
        if (auto it = announced_.find(conversationId); it != announced_.end())
            it->second.erase(documentId);
    }
    dropLocalReplica(conversationId, documentId);
    // The repository goes too: a document nobody can open again would otherwise
    // outlive its own removal on every device that held it. From another
    // thread: this runs while addToHistory() holds the parent conversation's
    // lock, and tearing a conversation down takes locks of its own.
    dht::ThreadPool::io().run([w = account_, documentId] {
        if (auto account = w.lock())
            if (auto* cm = account->convModule())
                cm->removeDocumentReplica(documentId);
    });
    emitSignal<libjami::ConfigurationSignal::CollaborativeDocumentRemoved>(accountId_, conversationId, documentId, true);
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
    auto conversation = documentConversation(documentId);
    if (!session || !conversation)
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
    std::string name;
    {
        auto infos = conversation->infos();
        if (auto it = infos.find("title"); it != infos.end())
            name = it->second;
    }
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
                                                                          update);
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
    auto conversation = documentConversation(session->documentId);
    if (!conversation)
        return;
    auto ids = conversation->documentAttachmentIds();
    std::vector<std::string> fresh;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        for (auto& id : ids)
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
    if (data.empty() || data.size() > MAX_ATTACHMENT_SIZE) {
        JAMI_WARNING("[Account {}] [Document {}] Attachment refused: {} byte(s), limit is {}",
                     accountId_,
                     documentId,
                     data.size(),
                     MAX_ATTACHMENT_SIZE);
        return {};
    }
    auto account = account_.lock();
    if (!account)
        return {};
    auto* cm = account->convModule();
    if (!cm)
        return {};
    // The payload is a commit in the document's own swarm, and committing it is
    // also what announces it: peers fetch it straight away rather than showing a
    // placeholder until the next checkpoint.
    auto id = cm->addDocumentAttachment(documentId, data);
    if (id.empty())
        return {};
    if (auto session = findSession(conversationId, documentId)) {
        // Ours already: the client that stored it holds the bytes, and the next
        // synchronization must not announce them back to it.
        std::lock_guard<std::mutex> lk(mutex_);
        session->knownAttachments.insert(id);
    }
    return id;
}

std::vector<uint8_t>
CollaborativeEditing::attachment(const std::string& /*conversationId*/,
                                 const std::string& documentId,
                                 const std::string& attachmentId)
{
    // Readable without an editing session: a client browsing the history of a
    // document it has not opened still has to resolve what it refers to.
    auto conversation = documentConversation(documentId);
    return conversation ? conversation->documentAttachment(attachmentId) : std::vector<uint8_t> {};
}

} // namespace jami
