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
#include "string_utils.h"

#include <dhtnet/multiplexed_socket.h>
#include <msgpack.hpp>
#include <opendht/thread_pool.h>

#include <algorithm>
#include <chrono>
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

// Ceilings on what a single message may carry. Both the client API and the
// channels hand us opaque blobs, and both are held in memory before the engine
// gets a say. The point is not to guess a "correct" size but to keep one message
// from being able to allocate without bound: a CRDT update stays well under a
// megabyte even when it carries the whole state of a large document, and an
// awareness state is a cursor plus a display name.
static constexpr size_t MAX_UPDATE_SIZE {8 * 1024 * 1024};
static constexpr size_t MAX_AWARENESS_SIZE {8 * 1024};
/// A whole awareness message may carry one entry per peer, not just ours.
static constexpr size_t MAX_AWARENESS_MESSAGE_SIZE {64 * 1024};

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

// What a real-time frame carries: a Y-CRDT update or an awareness message. One
// byte on the wire, so a frame type from a newer daemon is skipped over rather
// than choked on.
constexpr uint8_t FRAME_UPDATE {0};
constexpr uint8_t FRAME_AWARENESS {1};

// A frame is: varuint payload length, one tag byte, the payload. The length is
// variable so the framing costs one byte on the frames that matter -- a
// keystroke's update is a few dozen bytes -- while still naming sizes up to the
// caps above.
void
appendVarUint(std::vector<uint8_t>& out, uint64_t value)
{
    while (value >= 0x80) {
        out.push_back(static_cast<uint8_t>(0x80 | (value & 0x7F)));
        value >>= 7;
    }
    out.push_back(static_cast<uint8_t>(value));
}

enum class VarUint { OK, INCOMPLETE, MALFORMED };

// Reads seven bits per byte, least significant group first, the high bit
// marking that another byte follows. What is being read comes from a peer, so
// an integer that never terminates has to end the parse, not the process.
VarUint
readVarUint(const uint8_t* data, size_t size, uint64_t& value, size_t& consumed)
{
    value = 0;
    for (size_t i = 0; i < size; ++i) {
        if (i >= 10)
            return VarUint::MALFORMED; // longer than any uint64 ever encodes to
        value |= static_cast<uint64_t>(data[i] & 0x7F) << (7 * i);
        if ((data[i] & 0x80) == 0) {
            consumed = i + 1;
            return VarUint::OK;
        }
    }
    return size >= 10 ? VarUint::MALFORMED : VarUint::INCOMPLETE;
}

/// One client's slice of an awareness message, as it travels the wire.
struct AwarenessWire
{
    uint64_t clientId {0};
    /// Bumped by its owner on every change. An entry whose clock is not greater
    /// than the one already held is ignored, which is what keeps a message that
    /// took a longer route from resurrecting a state everyone has moved past.
    uint64_t clock {0};
    /// The client's state as a JSON document, or "null" for a client that is
    /// gone. Opaque here: its shape is the editors' agreement.
    std::string state;
    MSGPACK_DEFINE(clientId, clock, state)
};

std::vector<uint8_t>
encodeAwareness(const std::vector<AwarenessWire>& entries)
{
    msgpack::sbuffer buffer;
    msgpack::pack(buffer, entries);
    const auto* data = reinterpret_cast<const uint8_t*>(buffer.data());
    return {data, data + buffer.size()};
}

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
    // The last name handed to the clients. A remote rename cannot be spotted by
    // reading the repository before and after a synchronization -- the merge is
    // already done by the time we hear about it -- so this is what tells a name
    // that changed from one the clients have seen. Empty until the document is
    // opened: announcing a rename to a client that was never told the first name
    // would be a phantom event. Guarded by mutex_.
    std::optional<std::string> announcedName;
    // Whether a client currently has the document open. A closed holder keeps
    // replicating -- that is what holding is -- but its client is not told about
    // updates it is not looking at; reopening hands the converged state over
    // instead. Guarded by mutex_.
    bool open {false};
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

    // Real-time state. Kept under its own lock: the upkeep timer walks it from
    // the io thread while the clients write to it, and neither has any business
    // waiting on the manager-wide lock to do so.
    std::mutex protocolMutex;
    // The live channels to the peer devices that also have the document open,
    // per device. Two devices opening towards each other at once can end up
    // with a channel each, which is why this holds a list: dropping one of the
    // pair would have each side keep the one the other just closed. Frames are
    // sent on every one of them and a duplicate merges as a no-op -- that is
    // what a CRDT is for.
    std::map<std::string, std::vector<std::shared_ptr<dhtnet::ChannelSocket>>> channels;
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
CollaborativeEditing::findSessionByDocument(const std::string& documentId)
{
    std::lock_guard<std::mutex> lk(mutex_);
    for (const auto& [_, session] : sessions_)
        if (session && session->documentId == documentId)
            return session;
    return nullptr;
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
    if (!isAnnouncedDocument(conversationId, documentId)) {
        JAMI_ERROR("[Account {}] [Document {}] Not removing from this device: it was not announced in "
                   "conversation {}",
                   accountId_,
                   documentId,
                   conversationId);
        return false;
    }

    if (auto account = account_.lock())
        if (auto* cm = account->convModule())
            cm->removeDocumentReplica(documentId);
    dropLocalReplica(conversationId, documentId);
    emitSignal<libjami::ConversationSignal::CollaborativeDocumentRemoved>(accountId_, conversationId, documentId, false);
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
    // conversation. Without this check, a client naming arbitrary ids in an
    // open call would have us create a repository on disk for each.
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
    if (session)
        closeRealtimeChannels(session);
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
        std::string announcedName;
        for (const auto& doc : documents(conversationId)) {
            auto idIt = doc.find("id");
            if (idIt == doc.end() || idIt->second != documentId)
                continue;
            if (auto authorIt = doc.find("author"); authorIt != doc.end())
                announcer = authorIt->second;
            if (auto nameIt = doc.find("displayName"); nameIt != doc.end())
                announcedName = nameIt->second;
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
            session->open = true;
            // The client saw the announcement's name; recording it is what lets
            // a rename that lands with (or after) the clone be seen as one.
            if (!session->announcedName)
                session->announcedName = announcedName;
        }
        // The announcer is the likeliest holder, but no peer is a reliable
        // one: it may be offline, or be this very account -- its creator
        // reopening after a leave took the replica away, and only the
        // account's other devices can then serve it. Which members hold a
        // copy cannot be known without the repository, so every joined
        // member is a candidate; the module fetches from a couple and lets
        // its fallback rounds walk the rest.
        std::vector<std::string> candidates {announcer};
        for (const auto& member : cm->getConversationMembers(conversationId)) {
            auto uriIt = member.find("uri");
            if (uriIt == member.end() || uriIt->second == announcer)
                continue;
            // Invited, banned and left members cannot hold a replica:
            // holding one starts with an open, which they are refused.
            auto roleIt = member.find("role");
            if (roleIt == member.end() || (roleIt->second != "admin" && roleIt->second != "member"))
                continue;
            candidates.emplace_back(uriIt->second);
        }
        // The parent swarm already knows who is reachable right now: members
        // with a connected device come first, so the initial fetches go to
        // peers that can actually answer. Ties keep the announcer in front
        // as the likeliest holder.
        if (auto parent = cm->getConversation(conversationId)) {
            std::set<std::string> online;
            for (const auto& device : parent->peersToSyncWith()) {
                auto uri = parent->uriFromDevice(device.toString());
                if (!uri.empty())
                    online.emplace(std::move(uri));
            }
            std::stable_partition(candidates.begin(), candidates.end(), [&](const auto& uri) {
                return online.count(uri) != 0;
            });
        }
        cm->cloneDocumentFrom(documentId, candidates);
        // No channels yet: they need the members recorded in the repository, so
        // the clone's completion is what opens them. Until then the document is
        // open and empty, exactly like a conversation still syncing.
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
        session->open = true;
        if (!session->announcedName)
            session->announcedName = name;
    }
    auto state = session->doc->encodeStateAsUpdate();
    // Reach out to the other devices editing the document. What they produced
    // while nothing was open here is not asked for -- there is no handshake --
    // and does not need to be: it arrives with its producer's next checkpoint,
    // while everything from here on arrives live.
    connectRealtimeChannels(session);
    return state;
}

void
CollaborativeEditing::closeDocument(const std::string& conversationId, const std::string& documentId)
{
    auto session = findSession(conversationId, documentId);
    if (!session)
        return;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        session->open = false;
    }
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
    // The channels only exist between devices that are editing, and this one no
    // longer is. A reopen starts them over.
    closeRealtimeChannels(session);
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

namespace {

/// Frame a payload and write it to one channel.
void
writeFrame(const std::shared_ptr<dhtnet::ChannelSocket>& socket, uint8_t tag, const std::vector<uint8_t>& payload)
{
    std::vector<uint8_t> frame;
    frame.reserve(payload.size() + 11);
    appendVarUint(frame, payload.size());
    frame.push_back(tag);
    frame.insert(frame.end(), payload.begin(), payload.end());
    std::error_code ec;
    socket->write(frame.data(), frame.size(), ec);
    if (ec)
        JAMI_WARNING("Unable to send a {} byte collaborative frame: {}", frame.size(), ec.message());
}

} // namespace

bool
CollaborativeEditing::acceptsRealtimeChannel(const std::string& documentId,
                                             const std::string& peer,
                                             const std::string& deviceId)
{
    // Only for a document being edited here. A closed holder replicates through
    // the swarm and has no use for live frames; a device that never held the
    // document has nothing to accept them into; and answering for ids nobody
    // opened would let a peer probe what this device knows.
    auto session = findSessionByDocument(documentId);
    if (!session)
        return false;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!session->open)
            return false;
    }
    // Membership per the document's own replica, never the parent
    // conversation's. Invited counts as in: a document member is only ever
    // invited by a holder having just vouched for it while serving its clone,
    // and its join follows by itself -- there is no pending-invitation state to
    // wrongly admit. A joiner whose commits have not reached us at all is still
    // refused, and reached out to the moment they are merged.
    auto conversation = documentConversation(documentId);
    if (!conversation)
        return false; // the clone is still in flight: nothing to check against
    return conversation->isPeerAuthorized(peer, deviceId, true);
}

void
CollaborativeEditing::onRealtimeChannel(const std::string& documentId,
                                        const std::string& peer,
                                        const std::string& deviceId,
                                        std::shared_ptr<dhtnet::ChannelSocket> socket)
{
    auto session = findSessionByDocument(documentId);
    bool open = false;
    if (session) {
        std::lock_guard<std::mutex> lk(mutex_);
        open = session->open;
    }
    if (!open) {
        // The document was closed while the channel was being set up.
        socket->shutdown();
        return;
    }
    {
        std::lock_guard<std::mutex> lk(session->protocolMutex);
        session->channels[deviceId].push_back(socket);
    }

    // The parsing state of this one channel. Only its own receive callback
    // touches it, and those are delivered in order, so it needs no lock.
    auto buffer = std::make_shared<std::vector<uint8_t>>();
    std::weak_ptr<CollaborativeEditing> wthis = weak_from_this();
    std::weak_ptr<Session> wsession = session;
    std::weak_ptr<dhtnet::ChannelSocket> wsocket = socket;
    socket->setOnRecv([wthis, wsession, wsocket, peer, buffer](const uint8_t* data, size_t size) {
        auto sthis = wthis.lock();
        auto session = wsession.lock();
        if (!sthis || !session)
            return size;
        buffer->insert(buffer->end(), data, data + size);
        size_t pos = 0;
        while (pos < buffer->size()) {
            uint64_t length = 0;
            size_t consumed = 0;
            const auto res = readVarUint(buffer->data() + pos, buffer->size() - pos, length, consumed);
            if (res == VarUint::INCOMPLETE)
                break;
            // The length is judged before anything is held against it: a peer
            // must not have us buffer megabytes of a frame that could only be
            // refused once complete. Nothing recovers a framing violation --
            // there is no way back into sync with a stream that lies about its
            // lengths -- so the channel goes down with it.
            if (res == VarUint::MALFORMED || length > MAX_UPDATE_SIZE) {
                buffer->clear();
                if (auto socket = wsocket.lock())
                    socket->shutdown();
                return size;
            }
            if (buffer->size() - pos - consumed < 1 + length)
                break; // the rest of the frame is still in flight
            const uint8_t tag = (*buffer)[pos + consumed];
            sthis->onFrame(session, peer, tag, buffer->data() + pos + consumed + 1, length);
            pos += consumed + 1 + length;
        }
        buffer->erase(buffer->begin(), buffer->begin() + pos);
        return size;
    });
    socket->onShutdown([wsession, wsocket, deviceId](const std::error_code& /*ec*/) {
        auto session = wsession.lock();
        if (!session)
            return;
        auto socket = wsocket.lock();
        std::lock_guard<std::mutex> lk(session->protocolMutex);
        auto it = session->channels.find(deviceId);
        if (it == session->channels.end())
            return;
        auto& list = it->second;
        list.erase(std::remove_if(list.begin(), list.end(), [&](const auto& s) { return !socket || s == socket; }),
                   list.end());
        if (list.empty())
            session->channels.erase(it);
    });

    // Tell the newcomer at once who this device is in the document: our
    // awareness state only re-announces itself at the renewal cadence, and an
    // editor joining a session should not stare at an empty document for
    // fifteen seconds before the cursors appear.
    std::vector<uint8_t> hello;
    {
        std::lock_guard<std::mutex> lk(session->protocolMutex);
        if (!session->localState.empty())
            hello = encodeAwareness({{clientId_, session->localClock, session->localState}});
    }
    if (!hello.empty())
        writeFrame(socket, FRAME_AWARENESS, hello);
}

void
CollaborativeEditing::connectRealtimeChannels(const std::shared_ptr<Session>& session)
{
    auto account = account_.lock();
    if (!account)
        return;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!session->open)
            return;
    }
    auto conversation = documentConversation(session->documentId);
    if (!conversation)
        return; // the clone is still in flight: its completion comes back here
    // Every device the repository lists is asked, not just the ones the
    // document's swarm is connected to right now: the channel dials through the
    // connection manager, which reaches a device the DRT has not settled on
    // yet. Most are declined -- the other end only accepts while it has the
    // document open -- and the ones that matter are the editors.
    const auto ownDevice = std::string(account->currentDeviceId());
    for (const auto& [member, devices] : conversation->memberDevices()) {
        for (const auto& device : devices) {
            const auto deviceId = device.toString();
            if (deviceId == ownDevice)
                continue;
            {
                std::lock_guard<std::mutex> lk(session->protocolMutex);
                if (session->channels.count(deviceId) != 0)
                    continue; // already talking to it
            }
            // The socket is handled where the accepting side's is, in
            // YdocChannelHandler::onReady; a refusal needs nothing done.
            account->connectYdocDevice(device, session->documentId);
        }
    }
}

void
CollaborativeEditing::closeRealtimeChannels(const std::shared_ptr<Session>& session)
{
    decltype(session->channels) channels;
    {
        std::lock_guard<std::mutex> lk(session->protocolMutex);
        channels = std::move(session->channels);
        session->channels.clear();
    }
    // Outside the lock: shutting a socket down runs its shutdown handler, which
    // takes the same lock to unregister -- and finds nothing left to.
    for (auto& [_, list] : channels)
        for (auto& socket : list)
            socket->shutdown();
}

void
CollaborativeEditing::broadcastFrame(const std::shared_ptr<Session>& session,
                                     uint8_t tag,
                                     const std::vector<uint8_t>& payload)
{
    if (payload.empty())
        return;
    // Written outside the lock: a send can stall on a congested peer, and the
    // receive path needs the lock to route what the others are saying.
    std::vector<std::shared_ptr<dhtnet::ChannelSocket>> sockets;
    {
        std::lock_guard<std::mutex> lk(session->protocolMutex);
        for (const auto& [_, list] : session->channels)
            sockets.insert(sockets.end(), list.begin(), list.end());
    }
    for (const auto& socket : sockets)
        writeFrame(socket, tag, payload);
}

void
CollaborativeEditing::onFrame(
    const std::shared_ptr<Session>& session, const std::string& from, uint8_t tag, const uint8_t* payload, size_t size)
{
    switch (tag) {
    case FRAME_UPDATE:
        onRemoteUpdate(session, YrsDocument::Bytes(payload, payload + size));
        break;
    case FRAME_AWARENESS:
        onAwarenessPayload(session, from, payload, size);
        break;
    default:
        // A frame type from a newer daemon. Skipping it -- its length is known
        // -- is what lets one be added without every peer upgrading first.
        break;
    }
}

void
CollaborativeEditing::onRemoteUpdate(const std::shared_ptr<Session>& session, const YrsDocument::Bytes& update)
{
    if (update.empty())
        return;
    // Cleared before the update rather than read after it alone, so that what
    // this update brought is not confused with what an earlier one did.
    session->doc->takeChanged();
    if (!session->doc->applyUpdate(update))
        return; // malformed: don't hand it to the clients
    // Nothing when the update taught the replica nothing: a frame that raced a
    // checkpoint fetch carries what the replay already merged, and forwarding
    // it would light an "unread" badge on a document nobody touched.
    if (!session->doc->takeChanged())
        return;
    // Channels only exist while the document is open here, but a frame can slip
    // in between the close and the sockets going down; reopening hands the
    // merged state over instead.
    {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!session->open)
            return;
    }
    // Not persisted here: the device that produced it checkpoints it into its own
    // repository and it reaches ours through synchronization. Storing it again
    // would keep one copy per member of every single edit.
    emitUpdate(session->conversationId, session->documentId, update);
}

void
CollaborativeEditing::onAwarenessPayload(const std::shared_ptr<Session>& session,
                                         const std::string& from,
                                         const uint8_t* data,
                                         size_t size)
{
    if (size > MAX_AWARENESS_MESSAGE_SIZE)
        return;
    std::vector<AwarenessWire> entries;
    try {
        msgpack::object_handle oh = msgpack::unpack(reinterpret_cast<const char*>(data), size);
        oh.get().convert(entries);
    } catch (const std::exception&) {
        return; // a peer sent something that is not an awareness message
    }

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
        emitSignal<libjami::ConversationSignal::CollaborativeAwarenessChanged>(accountId_,
                                                                               session->conversationId,
                                                                               session->documentId,
                                                                               from,
                                                                               clientId,
                                                                               state);
    for (auto clientId : left)
        emitSignal<libjami::ConversationSignal::CollaborativeParticipantLeft>(accountId_,
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
    AwarenessWire entry;
    {
        std::lock_guard<std::mutex> lk(session->protocolMutex);
        // Withdrawing a state that was never shared would tell the members about
        // an editor they were never told about in the first place.
        if (state.empty() && session->localState.empty())
            return;
        entry.clientId = clientId_;
        entry.clock = ++session->localClock;
        // "null" is how a client that is no longer there is spelled.
        entry.state = state.empty() ? "null" : state;
        session->localState = state;
        session->localAnnounced = std::chrono::steady_clock::now();
    }
    broadcastFrame(session, FRAME_AWARENESS, encodeAwareness({entry}));
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
        emitSignal<libjami::ConversationSignal::CollaborativeParticipantLeft>(accountId_,
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
    // Real-time path: hand the incremental update to the devices editing along.
    broadcastFrame(session, FRAME_UPDATE, update);
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
    emitSignal<libjami::ConversationSignal::CollaborativeDocumentRemoved>(accountId_, conversationId, documentId, true);
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
    // What is sent is only what the replay brought, not the whole document: a
    // synchronization usually carries a handful of keystrokes, and re-encoding
    // a 300 kB document for each of them would push megabytes a minute through
    // the client API for nothing. A closed holder's client is not handed the
    // content either -- it gets the converged state when it reopens -- but it
    // is told that there is some: an update with an empty payload, which is
    // what lets it badge a document nobody here is watching.
    bool tellClient;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        tellClient = session->open;
    }
    if (session->doc->takeChanged())
        emitUpdate(conversationId, documentId, tellClient ? session->doc->encodeDiff(before) : YrsDocument::Bytes {});
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
    // A synchronization is also how new editors become reachable: the clone
    // this session may have been waiting for just landed, or a joiner's
    // membership commits were just merged -- the very merge that entitles the
    // device this replica refused a moment ago to its channel.
    connectRealtimeChannels(session);
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
    emitSignal<libjami::ConversationSignal::CollaborativeDocumentUpdate>(accountId_, conversationId, documentId, update);
}

void
CollaborativeEditing::emitRename(const std::string& conversationId,
                                 const std::string& documentId,
                                 const std::string& name)
{
    emitSignal<libjami::ConversationSignal::CollaborativeDocumentRenamed>(accountId_, conversationId, documentId, name);
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
        emitSignal<libjami::ConversationSignal::CollaborativeAttachmentAdded>(accountId_,
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
