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
#include "collab_repository.h"
#include "collab_updates.h"

#include "base64.h"
#include "fileutils.h"
#include "git_def.h"
#include "jamiaccount.h"
#include "json_utils.h"
#include "logger.h"

#include <algorithm>
#include <unordered_set>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <charconv>
#include <ctime>

namespace jami {

namespace {

constexpr std::string_view META_FILE {"meta.json"};
constexpr std::string_view ATTACHMENTS_DIR {"attachments"};
constexpr std::string_view MAIN_REF {"refs/heads/main"};

// Subject line prefix identifying a commit whose message carries CRDT updates.

// A document repository stores text deltas; a larger transfer means the peer is
// not sending what was asked for.
constexpr unsigned MAX_FETCH_SIZE {256 * 1024 * 1024};

// Regular, non-executable file mode for blobs stored in the tree.
constexpr git_filemode_t BLOB_MODE {GIT_FILEMODE_BLOB};

// A document tree only holds meta.json at the root, but a peer serves whatever
// it likes: cap the recursion rather than let a deeply nested tree overflow the
// worker thread's stack.
constexpr unsigned MAX_TREE_DEPTH {8};

// Loose objects tolerated before packing. Each checkpoint writes one, so this
// is roughly one pack per thousand checkpoints: often enough that the repository
// never balloons, rare enough that packing stays off the critical path of
// typing.
constexpr size_t COMPACT_LOOSE_THRESHOLD {1000};

// Pack files tolerated before consolidating. Every fetch from a peer leaves a
// pack behind, and nothing ever merged them: a document synchronized a few
// dozen times ends up with as many packs, each carrying its own copy of the
// shared history.
constexpr size_t COMPACT_PACK_THRESHOLD {4};

struct GitTreeBuilderDeleter
{
    void operator()(git_treebuilder* p) const { git_treebuilder_free(p); }
};
using GitTreeBuilder = std::unique_ptr<git_treebuilder, GitTreeBuilderDeleter>;

struct GitBlobDeleter
{
    void operator()(git_blob* p) const { git_blob_free(p); }
};
using GitBlob = std::unique_ptr<git_blob, GitBlobDeleter>;

struct GitTreeEntryDeleter
{
    void operator()(git_tree_entry* p) const { git_tree_entry_free(p); }
};
using GitTreeEntry = std::unique_ptr<git_tree_entry, GitTreeEntryDeleter>;

std::string
oidToString(const git_oid& oid)
{
    const char* str = git_oid_tostr_s(&oid);
    return str ? std::string {str} : std::string {};
}

/// Read a blob's content, given a tree entry that is expected to be a blob.
std::string
readBlob(git_repository* repo, const git_tree_entry* entry)
{
    if (!entry || git_tree_entry_type(entry) != GIT_OBJECT_BLOB)
        return {};
    git_blob* blob_ptr = nullptr;
    if (git_blob_lookup(&blob_ptr, repo, git_tree_entry_id(entry)) < 0)
        return {};
    GitBlob blob {blob_ptr};
    const auto* data = static_cast<const char*>(git_blob_rawcontent(blob.get()));
    return data ? std::string(data, git_blob_rawsize(blob.get())) : std::string {};
}

/// Read a blob addressed by its path relative to @c tree.
std::string
readBlobAt(git_repository* repo, git_tree* tree, const std::string& path)
{
    if (!tree)
        return {};
    git_tree_entry* entry_ptr = nullptr;
    if (git_tree_entry_bypath(&entry_ptr, tree, path.c_str()) < 0)
        return {};
    GitTreeEntry entry {entry_ptr};
    return readBlob(repo, entry.get());
}

/// The rename clock carried by a meta.json blob, or 0 when it has none: a
/// repository written before the clock existed, or one that was never renamed.
int64_t
metaNameClock(git_repository* repo, const git_tree_entry* entry)
{
    const auto content = readBlob(repo, entry);
    if (content.empty())
        return 0;
    Json::Value root;
    if (!json::parse(content, root))
        return 0;
    return root.get("nameClock", 0).asInt64();
}

/**
 * Union of two trees, computed identically on every replica.
 *
 * The CRDT updates live in the commit messages, so the commit graph already
 * unions them and this only has to reconcile the descriptive files at the root.
 * Those can genuinely differ between replicas, and are resolved by taking the
 * greater object id: a rule that is commutative, associative and idempotent,
 * hence independent of the merge order, which is what lets two replicas
 * converge on the same tree.
 *
 * meta.json is the exception, because the greater-oid rule has no idea which
 * side is the newer one. A checkpoint written concurrently with a rename copies
 * the ancestor's meta.json forward unchanged, so the union would compare the
 * renamed file against the one from before the rename and, one time in two,
 * keep the stale one -- silently undoing the rename on every replica. It is
 * therefore ordered by its rename clock first, and only by object id when the
 * clocks tie, which is a genuinely concurrent rename. Ordering on (clock, oid)
 * is still a total order, so the union remains a lattice.
 */
bool unionTrees(git_repository* repo, git_tree* local, git_tree* remote, git_oid& out, unsigned depth, bool attachments);

/// Merge one subtree entry present on the remote side into @p bld.
bool
unionSubtreeEntry(git_repository* repo,
                  git_treebuilder* bld,
                  const char* name,
                  const git_tree_entry* lEntry,
                  const git_tree_entry* rEntry,
                  unsigned depth)
{
    // attachments/ is the one subtree whose entries are named after their own
    // content, and it is merged under that rule instead of the ordering below.
    const bool attachments = depth == 0 && name && name == ATTACHMENTS_DIR;
    GitTree rSub {nullptr};
    {
        git_tree* p = nullptr;
        if (git_tree_lookup(&p, repo, git_tree_entry_id(rEntry)) < 0)
            return false;
        rSub.reset(p);
    }
    GitTree lSub {nullptr};
    if (lEntry) {
        git_tree* p = nullptr;
        if (git_tree_lookup(&p, repo, git_tree_entry_id(lEntry)) < 0)
            return false;
        lSub.reset(p);
    }
    git_oid merged;
    if (!unionTrees(repo, lSub.get(), rSub.get(), merged, depth + 1, attachments))
        return false;
    return git_treebuilder_insert(nullptr, bld, name, &merged, GIT_FILEMODE_TREE) == 0;
}

/// Merge one blob entry present on the remote side into @p bld.
///
/// The descriptive files at the root can differ between replicas. Picking the
/// greater oid resolves them the same way on both whatever the merge order: that
/// is what makes the union a lattice, hence convergent. meta.json is ordered by
/// its rename clock first, for the reason given above unionTrees().
bool
unionBlobEntry(git_repository* repo,
               git_treebuilder* bld,
               const char* name,
               const git_tree_entry* lEntry,
               const git_tree_entry* rEntry)
{
    if (lEntry && name == META_FILE) {
        const auto lClock = metaNameClock(repo, lEntry);
        const auto rClock = metaNameClock(repo, rEntry);
        if (lClock != rClock) {
            const auto* newer = lClock > rClock ? lEntry : rEntry;
            return git_treebuilder_insert(nullptr, bld, name, git_tree_entry_id(newer), git_tree_entry_filemode(newer))
                   == 0;
        }
    }
    const auto* winner = git_tree_entry_id(rEntry);
    auto mode = git_tree_entry_filemode(rEntry);
    if (lEntry) {
        const auto cmp = git_oid_cmp(git_tree_entry_id(lEntry), winner);
        if (cmp > 0) {
            winner = git_tree_entry_id(lEntry);
            mode = git_tree_entry_filemode(lEntry);
        } else if (cmp == 0) {
            // Same content, different filemode: the rule above would make each
            // replica keep the other's mode and the trees would never match.
            // Order this field too.
            mode = std::max(mode, git_tree_entry_filemode(lEntry));
        }
    }
    return git_treebuilder_insert(nullptr, bld, name, winner, mode) == 0;
}

/// Whether @p entry really holds the object it is named after.
///
/// In attachments/ the name @b is the git oid of the content, so this single
/// test is the whole admission rule.
bool
entryHoldsItsName(const git_tree_entry* entry)
{
    const auto* name = git_tree_entry_name(entry);
    return name && git_tree_entry_type(entry) == GIT_OBJECT_BLOB && oidToString(*git_tree_entry_id(entry)) == name;
}

bool
unionTrees(
    git_repository* repo, git_tree* local, git_tree* remote, git_oid& out, unsigned depth = 0, bool attachments = false)
{
    // A legitimate layout is three levels deep. The remote tree is whatever a
    // peer chose to serve, so cap the recursion rather than let a deeply nested
    // one overflow the worker thread's stack.
    if (depth > MAX_TREE_DEPTH)
        return false;

    git_treebuilder* bld_ptr = nullptr;
    if (git_treebuilder_new(&bld_ptr, repo, local) < 0)
        return false;
    GitTreeBuilder bld {bld_ptr};

    // Done before the merge, not after: an entry a peer forged under the name of
    // one of ours would otherwise win the ordering below, be dropped on read for
    // not matching its name, and take the legitimate attachment with it. Any
    // member of the swarm could then erase an image for everyone. Filtering the
    // local side too heals a tree that already merged such an entry.
    if (attachments)
        git_treebuilder_filter(
            bld.get(), [](const git_tree_entry* entry, void*) { return entryHoldsItsName(entry) ? 0 : 1; }, nullptr);

    const size_t count = remote ? git_tree_entrycount(remote) : 0;
    for (size_t i = 0; i < count; ++i) {
        const auto* rEntry = git_tree_entry_byindex(remote, i);
        if (!rEntry)
            return false;
        const auto* name = git_tree_entry_name(rEntry);
        const auto type = git_tree_entry_type(rEntry);
        const auto* lEntry = local ? git_tree_entry_byname(local, name) : nullptr;

        // Two admissible attachments sharing a name necessarily hold the same
        // object, so there is nothing to order: refusing the rest is enough.
        if (attachments && !entryHoldsItsName(rEntry))
            continue;

        // A tree entry only ever holds a tree or a blob here. A submodule would
        // point outside this repository, and there is nothing sensible to merge.
        if (type != GIT_OBJECT_TREE && type != GIT_OBJECT_BLOB)
            return false;
        // Same name, different kind on each side: the peer is not serving the
        // layout we wrote, and resolving that by object id would let a blob
        // replace a whole subtree. Refuse the merge instead.
        if (lEntry && git_tree_entry_type(lEntry) != type)
            return false;

        // Every failure below has to abort the whole union. Skipping an entry
        // would still produce a merge commit recording the remote as a parent,
        // so the dropped deltas would never be looked at again.
        const bool ok = type == GIT_OBJECT_TREE ? unionSubtreeEntry(repo, bld.get(), name, lEntry, rEntry, depth)
                                                : unionBlobEntry(repo, bld.get(), name, lEntry, rEntry);
        if (!ok)
            return false;
    }
    return git_treebuilder_write(&out, bld.get()) == 0;
}

/// Whether @p tree holds only what this class ever writes: subtrees and blobs,
/// nested no deeper than @c MAX_TREE_DEPTH.
///
/// unionTrees() already refuses anything else, but it only runs when the two
/// histories have diverged. Adopting an empty repository's first tip, or
/// fast-forwarding onto a peer's, takes the remote commit whole and never looks
/// inside it -- so those two paths need this check of their own.
///
/// What it keeps out is not hypothetical: a tree entry of any other kind names
/// an object that is not in this repository (git never fetches the target of a
/// submodule entry), and compaction then walks it as if it were a blob.
bool
treeShapeIsSupported(git_repository* repo, git_tree* tree, unsigned depth = 0)
{
    if (!tree || depth > MAX_TREE_DEPTH)
        return false;
    const size_t n = git_tree_entrycount(tree);
    for (size_t i = 0; i < n; ++i) {
        const auto* entry = git_tree_entry_byindex(tree, i);
        if (!entry)
            return false;
        const auto type = git_tree_entry_type(entry);
        if (type == GIT_OBJECT_BLOB)
            continue;
        if (type != GIT_OBJECT_TREE)
            return false;
        git_tree* sub = nullptr;
        if (git_tree_lookup(&sub, repo, git_tree_entry_id(entry)) < 0)
            return false;
        GitTree guard {sub};
        if (!treeShapeIsSupported(repo, sub, depth + 1))
            return false;
    }
    return true;
}

/// Insert (or replace) an entry in a copy of @c parent, returning the new tree.
bool
withEntry(
    git_repository* repo, git_tree* parent, std::string_view name, const git_oid& id, git_filemode_t mode, git_oid& out)
{
    git_treebuilder* bld_ptr = nullptr;
    if (git_treebuilder_new(&bld_ptr, repo, parent) < 0)
        return false;
    GitTreeBuilder bld {bld_ptr};
    if (git_treebuilder_insert(nullptr, bld.get(), std::string(name).c_str(), &id, mode) < 0)
        return false;
    return git_treebuilder_write(&out, bld.get()) == 0;
}

/// The @c attachments/ subtree of @c root, or nullptr when there is none.
GitTree
attachmentsTree(git_repository* repo, git_tree* root)
{
    if (!root)
        return GitTree {nullptr};
    git_tree_entry* entry_ptr = nullptr;
    if (git_tree_entry_bypath(&entry_ptr, root, std::string(ATTACHMENTS_DIR).c_str()) < 0)
        return GitTree {nullptr};
    GitTreeEntry entry {entry_ptr};
    if (git_tree_entry_type(entry.get()) != GIT_OBJECT_TREE)
        return GitTree {nullptr};
    git_tree* p = nullptr;
    if (git_tree_lookup(&p, repo, git_tree_entry_id(entry.get())) < 0)
        return GitTree {nullptr};
    return GitTree {p};
}

/// The attachment entry named @c id, only if it really holds the object it is
/// named after. Everything in this subtree arrives by merging what a peer chose
/// to serve, so an entry whose name does not match its content is a peer
/// substituting bytes under an id a client already trusts.
const git_tree_entry*
attachmentEntry(git_tree* dir, const std::string& id)
{
    if (!dir)
        return nullptr;
    const auto* entry = git_tree_entry_byname(dir, id.c_str());
    if (!entry || git_tree_entry_type(entry) != GIT_OBJECT_BLOB)
        return nullptr;
    return oidToString(*git_tree_entry_id(entry)) == id ? entry : nullptr;
}

} // namespace

class CollabRepository::Impl
{
public:
    Impl(const std::shared_ptr<JamiAccount>& account, std::string path)
        : account_(account)
        , accountId_(account ? account->getAccountID() : std::string {})
        , deviceId_(account ? account->currentDeviceId() : std::string {})
        , path_(std::move(path))
    {}

    GitRepository repository() const
    {
        git_repository* repo = nullptr;
        if (git_repository_open(&repo, path_.c_str()) != 0)
            return nullptr;
        return GitRepository {repo};
    }

    /// Tip commit of @c main, or nullptr when the repository is empty.
    GitCommit headCommit(git_repository* repo) const
    {
        git_oid oid;
        if (git_reference_name_to_id(&oid, repo, std::string(MAIN_REF).c_str()) < 0)
            return nullptr;
        git_commit* commit = nullptr;
        if (git_commit_lookup(&commit, repo, &oid) < 0)
            return nullptr;
        return GitCommit {commit};
    }

    GitTree headTree(const GitCommit& commit) const
    {
        if (!commit)
            return nullptr;
        git_tree* tree = nullptr;
        if (git_commit_tree(&tree, commit.get()) < 0)
            return nullptr;
        return GitTree {tree};
    }

    /// Signature identifying this device; mirrors the conversation repository.
    GitSignature signature() const
    {
        auto account = account_.lock();
        if (!account)
            return nullptr;
        auto name = account->getUsername();
        if (name.empty())
            name = deviceId_;
        git_signature* sig = nullptr;
        if (git_signature_new(&sig, name.c_str(), deviceId_.c_str(), std::time(nullptr), 0) < 0
            && git_signature_new(&sig, deviceId_.c_str(), deviceId_.c_str(), std::time(nullptr), 0) < 0)
            return nullptr;
        return GitSignature {sig};
    }

    /**
     * Create a commit signed with the account identity and move @c main to it.
     * Signing lets peers verify that a checkpoint really comes from a member's
     * device, exactly like conversation commits.
     */
    std::string commit(git_repository* repo,
                       const git_oid& treeId,
                       const std::string& message,
                       const std::vector<git_commit*>& parents)
    {
        auto account = account_.lock();
        if (!account)
            return {};
        auto sig = signature();
        if (!sig)
            return {};

        git_tree* tree_ptr = nullptr;
        if (git_tree_lookup(&tree_ptr, repo, &treeId) < 0)
            return {};
        GitTree tree {tree_ptr};

        git_buf to_sign = {};
        // The last argument of git_commit_create_buffer is of type
        // 'const git_commit **' in all versions of libgit2 except 1.8.0,
        // 1.8.1 and 1.8.3, in which it is of type 'git_commit *const *'.
#if LIBGIT2_VER_MAJOR == 1 && LIBGIT2_VER_MINOR == 8 \
    && (LIBGIT2_VER_REVISION == 0 || LIBGIT2_VER_REVISION == 1 || LIBGIT2_VER_REVISION == 3)
        git_commit* const* parentsPtr = parents.data();
#else
        const git_commit** parentsPtr = const_cast<const git_commit**>(parents.data());
#endif
        if (git_commit_create_buffer(&to_sign,
                                     repo,
                                     sig.get(),
                                     sig.get(),
                                     nullptr,
                                     message.c_str(),
                                     tree.get(),
                                     parents.size(),
                                     parents.empty() ? nullptr : parentsPtr)
            < 0) {
            JAMI_ERROR("[Account {}] [Document {}] Unable to create commit buffer", accountId_, path_);
            return {};
        }

        auto toSignVec = std::vector<uint8_t>(to_sign.ptr, to_sign.ptr + to_sign.size);
        auto signedBuf = account->identity().first->sign(toSignVec);
        auto signedStr = base64::encode(signedBuf);
        git_oid commitId;
        if (git_commit_create_with_signature(&commitId, repo, to_sign.ptr, signedStr.c_str(), "signature") < 0) {
            JAMI_ERROR("[Account {}] [Document {}] Unable to sign commit", accountId_, path_);
            git_buf_dispose(&to_sign);
            return {};
        }
        git_buf_dispose(&to_sign);

        git_reference* ref = nullptr;
        if (git_reference_create(&ref, repo, std::string(MAIN_REF).c_str(), &commitId, true, nullptr) < 0) {
            JAMI_ERROR("[Account {}] [Document {}] Unable to move main", accountId_, path_);
            return {};
        }
        git_reference_free(ref);
        return oidToString(commitId);
    }

    /**
     * The certificate of a member device, from the pinned store when it is
     * there and from the conversation repository when it is not.
     *
     * The fallback is not an optimisation: a document travels between devices
     * that never talked to each other, so the author of a commit we receive
     * from a third device may well be someone we never connected to and whose
     * certificate was therefore never pinned. Refusing on that basis would drop
     * legitimate work. The conversation repository holds devices/<id>.crt for
     * every member device, and it is itself validated, so it is the same source
     * of truth the conversation code falls back to.
     */
    std::shared_ptr<dht::crypto::Certificate> deviceCertificate(JamiAccount& account,
                                                                const std::string& deviceId,
                                                                const std::string& conversationId) const
    {
        if (auto cert = account.certStore().getCertificate(deviceId))
            return cert;
        if (!isValidId(conversationId))
            return {};
        const auto file = fileutils::get_data_dir() / accountId_ / "conversations" / conversationId / "devices"
                          / fmt::format("{}.crt", deviceId);
        if (!std::filesystem::is_regular_file(file))
            return {};
        try {
            return std::make_shared<dht::crypto::Certificate>(fileutils::loadFile(file));
        } catch (const std::exception& e) {
            JAMI_WARNING("[Account {}] Unable to load certificate for device {}: {}", accountId_, deviceId, e.what());
            return {};
        }
    }

    /**
     * Whether one commit really was written by the member device it names.
     *
     * A document repository is replicated from the other members, and a peer
     * serving this transport hands over whatever objects it likes: being allowed
     * to open the channel says nothing about who wrote the commits it serves.
     * Only the signature does, which is why every commit carries one.
     *
     * Three things have to hold, and the cheap ones come first: the named device
     * is one we can produce a certificate for, its owner is a member of this
     * conversation, and the signature over the commit content verifies against
     * that device's public key. The last one is what makes the first two
     * unforgeable.
     */
    bool commitIsAuthentic(git_repository* repo, const git_oid& oid, const std::string& conversationId) const
    {
        auto account = account_.lock();
        if (!account)
            return false;

        git_commit* commit_ptr = nullptr;
        if (git_commit_lookup(&commit_ptr, repo, &oid) < 0)
            return false;
        GitCommit commit {commit_ptr};

        // The device id is carried in the author email, as everywhere else in
        // this daemon: that is what signature() writes.
        const git_signature* author = git_commit_author(commit.get());
        if (!author || !author->email)
            return false;
        const std::string deviceId {author->email};
        if (!isValidId(deviceId))
            return false;

        auto cert = deviceCertificate(*account, deviceId, conversationId);
        if (!cert) {
            JAMI_WARNING("[Account {}] [Document {}] Refusing commit {}: no certificate for device {}",
                         accountId_,
                         path_,
                         oidToString(oid),
                         deviceId);
            return false;
        }

        const auto uri = cert->issuer ? cert->issuer->getId().toString() : cert->getIssuerUID();
        auto* cm = account->convModule(true);
        if (uri.empty() || !cm || !cm->isPeerAuthorized(conversationId, uri, deviceId, true)) {
            JAMI_WARNING("[Account {}] [Document {}] Refusing commit {}: device {} is not a member of {}",
                         accountId_,
                         path_,
                         oidToString(oid),
                         deviceId,
                         conversationId);
            return false;
        }

        git_buf sig = {}, signedData = {};
        if (git_commit_extract_signature(&sig, &signedData, repo, const_cast<git_oid*>(&oid), "signature") < 0) {
            JAMI_WARNING("[Account {}] [Document {}] Refusing commit {}: it carries no signature",
                         accountId_,
                         path_,
                         oidToString(oid));
            return false;
        }
        const auto raw = base64::decode(std::string_view(sig.ptr, sig.size));
        const bool ok = !raw.empty()
                        && cert->getPublicKey().checkSignature(reinterpret_cast<const uint8_t*>(signedData.ptr),
                                                               signedData.size,
                                                               raw.data(),
                                                               raw.size());
        git_buf_dispose(&sig);
        git_buf_dispose(&signedData);
        if (!ok)
            JAMI_WARNING("[Account {}] [Document {}] Refusing commit {}: it is not signed by device {}",
                         accountId_,
                         path_,
                         oidToString(oid),
                         deviceId);
        return ok;
    }

    /**
     * Whether every commit a fetch just brought in is authentic.
     *
     * The whole new history, not just the tip: a forged ancestor under an honest
     * tip is the obvious way past a check that only looks at what a ref points
     * to, and its updates would be collected all the same. What we already hold
     * was verified when it arrived and is hidden from the walk, so the cost is
     * proportional to what is new rather than to the length of the history.
     */
    bool historyIsAuthentic(git_repository* repo,
                            const git_oid& remote,
                            const git_oid* local,
                            const std::string& conversationId) const
    {
        git_revwalk* walker_ptr = nullptr;
        if (git_revwalk_new(&walker_ptr, repo) < 0)
            return false;
        GitRevWalker walker {walker_ptr};
        if (git_revwalk_push(walker.get(), &remote) < 0)
            return false;
        if (local && git_revwalk_hide(walker.get(), local) < 0)
            return false;

        git_oid oid;
        while (git_revwalk_next(&oid, walker.get()) == 0) {
            if (!commitIsAuthentic(repo, oid, conversationId))
                return false;
        }
        return true;
    }

    std::weak_ptr<JamiAccount> account_;
    std::string accountId_;
    std::string deviceId_;
    std::string path_;
    mutable std::mutex mutex_;
    // A fetch writes a pack without holding mutex_, on purpose: it is a network
    // operation and must not block typing. Compaction therefore has to know
    // whether one overlapped it before it removes any pack.
    std::atomic_uint fetchesStarted_ {0};
    std::atomic_uint fetchesFinished_ {0};
};

CollabRepository::CollabRepository(const std::shared_ptr<JamiAccount>& account,
                                   std::string conversationId,
                                   std::string documentId,
                                   std::string path)
    : pimpl_(std::make_unique<Impl>(account, path))
    , conversationId_(std::move(conversationId))
    , documentId_(std::move(documentId))
    , path_(std::move(path))
{}

CollabRepository::~CollabRepository() = default;

std::string
CollabRepository::truncatedName(std::string name)
{
    if (name.size() <= MAX_DOCUMENT_NAME_SIZE)
        return name;
    // Cut on a code point boundary, so the result is still valid UTF-8 and can be
    // put back into JSON.
    size_t cut = MAX_DOCUMENT_NAME_SIZE;
    while (cut > 0 && (static_cast<unsigned char>(name[cut]) & 0xC0) == 0x80)
        --cut;
    name.resize(cut);
    return name;
}

bool
CollabRepository::isValidId(std::string_view id)
{
    // Conversation, document, commit and device ids are all generated as
    // fixed-length lowercase hexadecimal strings. Enforcing that alphabet here is
    // what keeps a peer-supplied id from reaching outside the account's own
    // directory, or from being read as a git refspec: no separator, no "..", no
    // absolute path, nothing the filesystem or git could interpret. Every path
    // helper below funnels through it.
    if (id.empty() || id.size() > 64)
        return false;
    return std::all_of(id.begin(), id.end(), [](unsigned char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}

std::filesystem::path
CollabRepository::conversationPath(const std::string& accountId, const std::string& conversationId)
{
    if (!isValidId(conversationId))
        return {};
    return fileutils::get_data_dir() / accountId / "collab" / conversationId;
}

std::filesystem::path
CollabRepository::documentPath(const std::string& accountId,
                               const std::string& conversationId,
                               const std::string& documentId)
{
    if (!isValidId(documentId))
        return {};
    auto base = conversationPath(accountId, conversationId);
    if (base.empty())
        return {};
    return base / documentId;
}

std::vector<std::string>
CollabRepository::listDocuments(const std::string& accountId, const std::string& conversationId)
{
    std::vector<std::string> ids;
    std::error_code ec;
    auto base = conversationPath(accountId, conversationId);
    if (base.empty())
        return ids;
    for (const auto& entry : std::filesystem::directory_iterator(base, ec)) {
        if (!entry.is_directory(ec))
            continue;
        // The same check documentPath() makes, for the same reason: whatever
        // else ended up in this directory, only names this code could have
        // written are handed back as document ids.
        auto name = entry.path().filename().string();
        if (isValidId(name))
            ids.emplace_back(std::move(name));
    }
    return ids;
}

namespace {
/// The marker file standing for a document removed from this device.
/// Empty if either id is invalid. It sits beside the repositories rather than
/// inside one: the repository is exactly what removal erases.
std::filesystem::path
localRemovalPath(const std::string& accountId, const std::string& conversationId, const std::string& documentId)
{
    if (!CollabRepository::isValidId(documentId))
        return {};
    auto base = CollabRepository::conversationPath(accountId, conversationId);
    if (base.empty())
        return {};
    // listDocuments() only ever hands back directories whose name is a valid id,
    // so a marker can never be mistaken for a document of its own.
    return base / (documentId + ".removed");
}
} // namespace

bool
CollabRepository::markLocallyRemoved(const std::string& accountId,
                                     const std::string& conversationId,
                                     const std::string& documentId)
{
    auto path = localRemovalPath(accountId, conversationId, documentId);
    if (path.empty())
        return false;
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    // The file is only ever tested for existence, so it stays empty: writing
    // anything in it would be one more thing to keep in step with the truth.
    std::ofstream marker(path, std::ios::binary | std::ios::trunc);
    if (!marker) {
        JAMI_WARNING("[Account {}] [Document {}] Could not mark the document as removed from this device",
                     accountId,
                     documentId);
        return false;
    }
    return true;
}

bool
CollabRepository::clearLocalRemoval(const std::string& accountId,
                                    const std::string& conversationId,
                                    const std::string& documentId)
{
    auto path = localRemovalPath(accountId, conversationId, documentId);
    if (path.empty())
        return false;
    std::error_code ec;
    std::filesystem::remove(path, ec);
    return !ec;
}

std::vector<std::string>
CollabRepository::listLocallyRemoved(const std::string& accountId, const std::string& conversationId)
{
    std::vector<std::string> ids;
    std::error_code ec;
    auto base = conversationPath(accountId, conversationId);
    if (base.empty())
        return ids;
    static constexpr std::string_view SUFFIX = ".removed";
    for (const auto& entry : std::filesystem::directory_iterator(base, ec)) {
        auto name = entry.path().filename().string();
        if (name.size() <= SUFFIX.size() || !std::string_view(name).ends_with(SUFFIX))
            continue;
        auto id = name.substr(0, name.size() - SUFFIX.size());
        if (isValidId(id))
            ids.emplace_back(std::move(id));
    }
    return ids;
}

std::shared_ptr<CollabRepository>
CollabRepository::openOrInit(const std::shared_ptr<JamiAccount>& account,
                             const std::string& conversationId,
                             const std::string& documentId)
{
    return instance(account, conversationId, documentId, true);
}

std::shared_ptr<CollabRepository>
CollabRepository::instance(const std::shared_ptr<JamiAccount>& account,
                           const std::string& conversationId,
                           const std::string& documentId,
                           bool create)
{
    if (!account)
        return nullptr;
    auto path = documentPath(account->getAccountID(), conversationId, documentId);
    if (path.empty()) {
        JAMI_WARNING("[Account {}] Refusing document id {} in conversation {}: not a valid id",
                     account->getAccountID(),
                     documentId,
                     conversationId);
        return nullptr;
    }
    const bool exists = std::filesystem::exists(path / "HEAD");
    if (!exists && !create)
        return nullptr;
    // One instance per repository, process-wide. The per-instance mutex is what
    // serializes checkpoints against merges, so handing two callers their own
    // object would let a checkpoint and a fetch both move refs/heads/main from
    // the same starting point, and one of the two sets of edits would be lost.
    // Every entry point funnels through here for that reason.
    static std::mutex registryMtx;
    static std::map<std::string, std::weak_ptr<CollabRepository>> registry;
    std::lock_guard registryLk(registryMtx);
    auto key = path.string();
    if (auto it = registry.find(key); it != registry.end()) {
        if (auto existing = it->second.lock())
            return existing;
        registry.erase(it);
    }
    // Drop entries whose repository is gone, so the map does not grow with every
    // document ever touched.
    for (auto it = registry.begin(); it != registry.end();)
        it = it->second.expired() ? registry.erase(it) : std::next(it);

    auto repo = std::shared_ptr<CollabRepository>(
        new CollabRepository(account, conversationId, documentId, path.string()));

    if (!exists) {
        std::error_code ec;
        std::filesystem::create_directories(path, ec);
        git_repository* raw = nullptr;
        git_repository_init_options opts;
        git_repository_init_options_init(&opts, GIT_REPOSITORY_INIT_OPTIONS_VERSION);
        // Bare: the payload lives in the object database, and no working copy is
        // needed since trees are built programmatically. Halves the footprint.
        opts.flags = GIT_REPOSITORY_INIT_MKPATH | GIT_REPOSITORY_INIT_BARE;
        opts.initial_head = "main";
        if (git_repository_init_ext(&raw, path.string().c_str(), &opts) < 0) {
            JAMI_ERROR("[Account {}] [Document {}] Unable to initialize repository",
                       account->getAccountID(),
                       documentId);
            return nullptr;
        }
        git_repository_free(raw);
    }
    registry[key] = repo;
    return repo;
}

std::shared_ptr<CollabRepository>
CollabRepository::create(const std::shared_ptr<JamiAccount>& account,
                         const std::string& conversationId,
                         const std::string& documentId,
                         const std::string& displayName,
                         const std::string& mimeType)
{
    if (!account)
        return nullptr;
    auto path = documentPath(account->getAccountID(), conversationId, documentId);
    if (path.empty() || std::filesystem::exists(path / "HEAD")) {
        JAMI_WARNING("[Account {}] [Document {}] Repository already exists", account->getAccountID(), documentId);
        return nullptr;
    }
    auto self = openOrInit(account, conversationId, documentId);
    if (!self)
        return nullptr;

    auto repo = self->pimpl_->repository();
    if (!repo)
        return nullptr;

    Json::Value meta;
    meta["documentId"] = documentId;
    meta["conversationId"] = conversationId;
    meta["displayName"] = truncatedName(displayName);
    meta["mimeType"] = mimeType.empty() ? DEFAULT_MIME_TYPE : mimeType;
    meta["createdBy"] = account->getUsername();
    meta["createdAt"] = static_cast<Json::Int64>(std::time(nullptr));
    auto metaStr = json::toString(meta);

    git_oid metaBlob;
    if (git_blob_create_from_buffer(&metaBlob, repo.get(), metaStr.data(), metaStr.size()) < 0)
        return nullptr;

    git_oid treeId;
    if (!withEntry(repo.get(), nullptr, META_FILE, metaBlob, BLOB_MODE, treeId))
        return nullptr;

    auto commitId = self->pimpl_->commit(repo.get(), treeId, "document created", {});
    if (commitId.empty())
        return nullptr;
    JAMI_LOG("[Account {}] [Document {}] Repository created at {}", account->getAccountID(), documentId, self->path_);
    return self;
}

std::shared_ptr<CollabRepository>
CollabRepository::open(const std::shared_ptr<JamiAccount>& account,
                       const std::string& conversationId,
                       const std::string& documentId)
{
    // Through the registry like every other entry point: two objects for one
    // repository would each hold their own mutex, and a checkpoint racing a
    // merge would lose one of the two sets of edits.
    return instance(account, conversationId, documentId, false);
}

std::string
CollabRepository::head() const
{
    std::lock_guard lk(pimpl_->mutex_);
    auto repo = pimpl_->repository();
    if (!repo)
        return {};
    git_oid oid;
    if (git_reference_name_to_id(&oid, repo.get(), std::string(MAIN_REF).c_str()) < 0)
        return {};
    return oidToString(oid);
}

CollabRepository::Meta
CollabRepository::meta() const
{
    std::lock_guard lk(pimpl_->mutex_);
    Meta out;
    out.documentId = documentId_;
    out.conversationId = conversationId_;

    auto repo = pimpl_->repository();
    if (!repo)
        return out;
    auto commit = pimpl_->headCommit(repo.get());
    auto tree = pimpl_->headTree(commit);
    auto content = readBlobAt(repo.get(), tree.get(), std::string(META_FILE));
    if (content.empty())
        return out;

    Json::Value root;
    if (!json::parse(content, root))
        return out;
    // meta.json is merged in from other members: its name is remote input, and it
    // ends up in a signal every client displays.
    out.displayName = truncatedName(root.get("displayName", "").asString());
    out.mimeType = root.get("mimeType", DEFAULT_MIME_TYPE).asString();
    out.createdBy = root.get("createdBy", "").asString();
    out.createdAt = root.get("createdAt", 0).asInt64();
    return out;
}

std::string
CollabRepository::appendCheckpoint(const std::vector<std::string>& updates)
{
    if (updates.empty())
        return {};
    std::lock_guard lk(pimpl_->mutex_);
    auto repo = pimpl_->repository();
    if (!repo)
        return {};

    auto parent = pimpl_->headCommit(repo.get());
    auto root = pimpl_->headTree(parent);

    // A document opened before its first fetch has no commit yet, so there is no
    // tree to carry over. Commit an empty one rather than drop the edits: the
    // merge that follows the fetch unions it away.
    git_oid treeId;
    if (root) {
        treeId = *git_tree_id(root.get());
    } else {
        git_treebuilder* bld_ptr = nullptr;
        if (git_treebuilder_new(&bld_ptr, repo.get(), nullptr) < 0)
            return {};
        GitTreeBuilder bld {bld_ptr};
        if (git_treebuilder_write(&treeId, bld.get()) < 0)
            return {};
    }

    // The updates go in the message, and the tree is carried over untouched: a
    // checkpoint costs one commit object and nothing else. Storing them as files
    // instead would force a new subtree per device plus a new root tree at every
    // checkpoint, which measures larger than the deltas themselves.
    std::string message = fmt::format("checkpoint: {} update(s)\n\n", updates.size());
    for (const auto& u : updates) {
        message += u;
        message += '\n';
    }

    std::vector<git_commit*> parents;
    if (parent)
        parents.push_back(parent.get());
    return pimpl_->commit(repo.get(), treeId, message, parents);
}

std::string
CollabRepository::setDisplayName(const std::string& displayName)
{
    std::lock_guard lk(pimpl_->mutex_);
    auto repo = pimpl_->repository();
    if (!repo)
        return {};
    auto parent = pimpl_->headCommit(repo.get());
    auto root = pimpl_->headTree(parent);
    if (!root)
        return {};

    auto content = readBlobAt(repo.get(), root.get(), std::string(META_FILE));
    Json::Value meta;
    if (!content.empty())
        json::parse(content, meta);
    meta["displayName"] = truncatedName(displayName);
    // Marks this meta.json as newer than the one it replaces, so that a
    // checkpoint written concurrently -- which carries the ancestor's meta.json
    // forward untouched -- cannot win the merge and undo the rename.
    meta["nameClock"] = meta.get("nameClock", 0).asInt64() + 1;
    auto metaStr = json::toString(meta);

    git_oid metaBlob;
    if (git_blob_create_from_buffer(&metaBlob, repo.get(), metaStr.data(), metaStr.size()) < 0)
        return {};
    git_oid newRoot;
    if (!withEntry(repo.get(), root.get(), META_FILE, metaBlob, BLOB_MODE, newRoot))
        return {};

    std::vector<git_commit*> parents;
    if (parent)
        parents.push_back(parent.get());
    return pimpl_->commit(repo.get(), newRoot, "document renamed", parents);
}

std::string
CollabRepository::addAttachment(const std::vector<uint8_t>& data)
{
    if (data.empty() || data.size() > MAX_ATTACHMENT_SIZE)
        return {};
    std::lock_guard lk(pimpl_->mutex_);
    auto repo = pimpl_->repository();
    if (!repo)
        return {};

    git_oid blobId;
    if (git_blob_create_from_buffer(&blobId, repo.get(), data.data(), data.size()) < 0)
        return {};
    const auto id = oidToString(blobId);

    auto parent = pimpl_->headCommit(repo.get());
    auto root = pimpl_->headTree(parent);
    auto dir = attachmentsTree(repo.get(), root.get());
    // Storing the same payload twice must not write a commit: the same image
    // pasted into two paragraphs, or edited back in after a delete, would
    // otherwise grow the history for nothing. The blob above is already there
    // and is left for the next compaction to collect if it is not referenced.
    if (attachmentEntry(dir.get(), id))
        return id;

    git_oid newDir;
    if (!withEntry(repo.get(), dir.get(), id, blobId, BLOB_MODE, newDir))
        return {};
    git_oid newRoot;
    if (!withEntry(repo.get(), root.get(), ATTACHMENTS_DIR, newDir, GIT_FILEMODE_TREE, newRoot))
        return {};

    std::vector<git_commit*> parents;
    if (parent)
        parents.push_back(parent.get());
    if (pimpl_->commit(repo.get(), newRoot, fmt::format("attachment: {} ({} bytes)", id, data.size()), parents).empty())
        return {};
    return id;
}

std::vector<uint8_t>
CollabRepository::attachment(const std::string& attachmentId) const
{
    if (!isValidId(attachmentId))
        return {};
    std::lock_guard lk(pimpl_->mutex_);
    auto repo = pimpl_->repository();
    if (!repo)
        return {};
    auto commit = pimpl_->headCommit(repo.get());
    auto root = pimpl_->headTree(commit);
    auto dir = attachmentsTree(repo.get(), root.get());
    const auto* entry = attachmentEntry(dir.get(), attachmentId);
    if (!entry)
        return {};

    git_blob* blob_ptr = nullptr;
    if (git_blob_lookup(&blob_ptr, repo.get(), git_tree_entry_id(entry)) < 0)
        return {};
    GitBlob blob {blob_ptr};
    const auto size = git_blob_rawsize(blob.get());
    const auto* raw = static_cast<const uint8_t*>(git_blob_rawcontent(blob.get()));
    // A peer serves whatever it likes: an entry can name a blob far larger than
    // we would ever have written ourselves.
    if (!raw || size == 0 || static_cast<size_t>(size) > MAX_ATTACHMENT_SIZE)
        return {};
    return {raw, raw + size};
}

std::vector<std::string>
CollabRepository::attachmentIds() const
{
    std::lock_guard lk(pimpl_->mutex_);
    std::vector<std::string> out;
    auto repo = pimpl_->repository();
    if (!repo)
        return out;
    auto commit = pimpl_->headCommit(repo.get());
    auto root = pimpl_->headTree(commit);
    auto dir = attachmentsTree(repo.get(), root.get());
    if (!dir)
        return out;

    const auto count = git_tree_entrycount(dir.get());
    out.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        const auto* entry = git_tree_entry_byindex(dir.get(), i);
        if (!entry || git_tree_entry_type(entry) != GIT_OBJECT_BLOB)
            continue;
        const auto* name = git_tree_entry_name(entry);
        if (!name)
            continue;
        // Same check as attachment(): an entry that does not hold the object it
        // is named after is never handed to a client, so it is not announced.
        if (oidToString(*git_tree_entry_id(entry)) == name)
            out.emplace_back(name);
    }
    return out;
}

/// Append the base64 CRDT updates carried by one commit message to @c out.
static void
collectMessageUpdates(const char* message, std::vector<std::string>& out)
{
    if (!message)
        return;
    // The parsing and its limits live in collab_updates.h so that the checks
    // exercise the same code this does, rather than a copy of it free to drift.
    if (!collab::appendMessageUpdates(std::string_view {message}, out))
        JAMI_WARNING("Stopping at {} updates in a single checkpoint", collab::MAX_UPDATES_PER_COMMIT);
}

/**
 * Collect every CRDT update recorded in the history reachable from @c tip,
 * oldest checkpoint first.
 *
 * The set of updates of a commit is the union of those of its ancestors, so a
 * merge commit yields both sides without any tree being touched: the commit
 * graph performs the union. Y-CRDT updates are commutative, so the interleaving
 * between devices does not affect the result.
 */
static std::vector<std::string>
collectUpdates(git_repository* repo, const git_oid& tip)
{
    std::vector<std::string> out;
    git_revwalk* walker_ptr = nullptr;
    if (git_revwalk_new(&walker_ptr, repo) < 0)
        return out;
    GitRevWalker walker {walker_ptr};
    if (git_revwalk_push(walker.get(), &tip) < 0)
        return out;
    git_revwalk_sorting(walker.get(), GIT_SORT_TOPOLOGICAL | GIT_SORT_REVERSE);

    git_oid oid;
    size_t bytes = 0;
    while (git_revwalk_next(&oid, walker.get()) == 0) {
        git_commit* commit_ptr = nullptr;
        if (git_commit_lookup(&commit_ptr, repo, &oid) < 0)
            continue;
        GitCommit commit {commit_ptr};
        const auto before = out.size();
        collectMessageUpdates(git_commit_message(commit.get()), out);
        for (auto i = before; i < out.size(); ++i)
            bytes += out[i].size();
        // Stopping mid-history is a loss, but a bounded one: what was collected
        // still applies, and the CRDT converges again once the rest arrives.
        // Growing without limit is not bounded at all.
        if (collab::collectionIsExhausted(out.size(), bytes)) {
            JAMI_WARNING("Stopping the update walk at {} updates / {} bytes", out.size(), bytes);
            break;
        }
    }
    return out;
}

std::vector<std::string>
CollabRepository::updates() const
{
    std::lock_guard lk(pimpl_->mutex_);
    auto repo = pimpl_->repository();
    if (!repo)
        return {};
    git_oid oid;
    if (git_reference_name_to_id(&oid, repo.get(), std::string(MAIN_REF).c_str()) < 0)
        return {};
    return collectUpdates(repo.get(), oid);
}

std::optional<std::vector<std::string>>
CollabRepository::updatesAt(const std::string& commitId) const
{
    if (!isValidId(commitId))
        return std::nullopt;
    std::lock_guard lk(pimpl_->mutex_);
    auto repo = pimpl_->repository();
    if (!repo)
        return std::nullopt;

    git_oid oid;
    if (git_oid_fromstr(&oid, commitId.c_str()) < 0)
        return std::nullopt;
    git_commit* commit_ptr = nullptr;
    if (git_commit_lookup(&commit_ptr, repo.get(), &oid) < 0)
        return std::nullopt;
    GitCommit commit {commit_ptr};

    // Only serve commits that belong to this document's history: a peer could
    // otherwise name any object present in the repository.
    git_reference* ref_ptr = nullptr;
    if (git_reference_lookup(&ref_ptr, repo.get(), std::string(MAIN_REF).c_str()) < 0)
        return std::nullopt;
    GitReference ref {ref_ptr};
    const auto* headOid = git_reference_target(ref.get());
    if (!headOid)
        return std::nullopt;
    if (git_oid_cmp(headOid, &oid) != 0 && git_graph_descendant_of(repo.get(), headOid, &oid) != 1)
        return std::nullopt;

    return collectUpdates(repo.get(), oid);
}

namespace {

struct ObjectCounts
{
    size_t loose {0};
    size_t packFiles {0};
};

/// Count loose object files and pack files under objects/.
ObjectCounts
countObjects(const std::string& path)
{
    ObjectCounts counts;
    std::error_code ec;
    const std::filesystem::path objects = std::filesystem::path(path) / "objects";
    for (auto it = std::filesystem::directory_iterator(objects / "pack", ec);
         it != std::filesystem::directory_iterator();
         it.increment(ec)) {
        if (ec)
            break;
        // Only the packs themselves. Each one is accompanied by an .idx, and
        // git leaves .rev and temporary files there too, so counting every file
        // would roughly double the figure and fire compaction twice as early.
        if (it->path().extension() == ".pack")
            ++counts.packFiles;
    }
    for (const auto& entry : std::filesystem::directory_iterator(objects, ec)) {
        // Loose objects live in 256 two-hex-digit directories; "pack" and
        // "info" are neither.
        const auto name = entry.path().filename().string();
        if (name.size() != 2 || !entry.is_directory(ec))
            continue;
        for (auto it = std::filesystem::directory_iterator(entry.path(), ec);
             it != std::filesystem::directory_iterator();
             it.increment(ec)) {
            if (ec)
                break;
            ++counts.loose;
        }
    }
    return counts;
}

/// Whether the repository has accumulated enough clutter to be worth packing.
bool
worthCompacting(const ObjectCounts& counts)
{
    return counts.loose > COMPACT_LOOSE_THRESHOLD || counts.packFiles > COMPACT_PACK_THRESHOLD;
}

/// Collect every object reachable from @p treeId: the tree itself, its subtrees
/// and its blobs. Bounded by the same depth limit as the rest of the class.
///
/// @p seen carries across calls and is what keeps this affordable. A document
/// tree holds one entry per checkpoint, so without it commit N would enumerate
/// N blobs and the walk would cost O(checkpoints^2) -- hundreds of megabytes of
/// resident memory on a repository of a few megabytes. A tree already visited
/// has had its whole subtree visited too, so the recursion stops there.
bool
collectReachable(git_repository* repo,
                 const git_oid& treeId,
                 std::unordered_set<std::string>& seen,
                 std::vector<git_oid>& trees,
                 std::vector<git_oid>& blobs,
                 unsigned depth = 0)
{
    if (depth > MAX_TREE_DEPTH)
        return true; // see the default case below: skipped, never fatal
    if (!seen.insert(oidToString(treeId)).second)
        return true; // this subtree was reached from another commit already
    git_tree* tree = nullptr;
    if (git_tree_lookup(&tree, repo, &treeId) < 0)
        return false;
    GitTree guard {tree};
    trees.push_back(treeId);
    const size_t n = git_tree_entrycount(tree);
    for (size_t i = 0; i < n; ++i) {
        const git_tree_entry* e = git_tree_entry_byindex(tree, i);
        const git_oid* id = git_tree_entry_id(e);
        switch (git_tree_entry_type(e)) {
        case GIT_OBJECT_TREE:
            if (!collectReachable(repo, *id, seen, trees, blobs, depth + 1))
                return false;
            break;
        case GIT_OBJECT_BLOB:
            if (seen.insert(oidToString(*id)).second)
                blobs.push_back(*id);
            break;
        default:
            // Anything else names an object this repository does not hold, so
            // the pack cannot carry it. Treating it as a blob, as this loop used
            // to, made git_packbuilder_insert() fail and compaction give up --
            // permanently, since the entry stays in the tree and every later
            // attempt walks it again. A peer only has to serve one such entry
            // once, and the document then grows without bound.
            //
            // Skipping costs nothing: pruneLoose() deletes only what was packed,
            // so an object left out here is never removed either.
            break;
        }
    }
    return true;
}

} // namespace

bool
CollabRepository::needsCompaction() const
{
    return worthCompacting(countObjects(path_));
}

namespace {

/// Every object reachable from any ref, grouped by kind.
struct ReachableObjects
{
    std::vector<git_oid> commits, trees, blobs;

    size_t size() const { return commits.size() + trees.size() + blobs.size(); }
};

/// Walk every ref and gather what it reaches. Returns false on any incomplete
/// walk: a walk cut short by an unreadable commit would leave part of the history
/// out of the pack, and the caller's pruning would then delete objects that are
/// still needed.
bool
collectFromAllRefs(git_repository* repo, ReachableObjects& out)
{
    git_revwalk* walkPtr = nullptr;
    if (git_revwalk_new(&walkPtr, repo) < 0)
        return false;
    std::unique_ptr<git_revwalk, decltype(&git_revwalk_free)> walk {walkPtr, git_revwalk_free};
    // Every ref, not just main: a document fetched from a peer sits under
    // refs/remotes/<device>/main until it is merged, and dropping the pack that
    // carries it would lose it.
    if (git_revwalk_push_glob(walk.get(), "refs/*") < 0)
        return false;

    std::unordered_set<std::string> seen;
    git_oid oid;
    int walkStatus = 0;
    while ((walkStatus = git_revwalk_next(&oid, walk.get())) == 0) {
        out.commits.push_back(oid);
        git_commit* c = nullptr;
        if (git_commit_lookup(&c, repo, &oid) < 0)
            return false;
        GitCommit commit {c};
        if (!collectReachable(repo, *git_commit_tree_id(c), seen, out.trees, out.blobs))
            return false;
    }
    return walkStatus == GIT_ITEROVER && !out.commits.empty();
}

/// Write a pack holding every object of @p objects into @p packDir. On success,
/// @p stalePacks lists the packs that were there before and that the new one
/// makes redundant.
bool
writePack(git_repository* repo,
          const ReachableObjects& objects,
          const std::string& packDir,
          const std::string& documentId,
          std::vector<std::filesystem::path>& stalePacks)
{
    git_packbuilder* pbPtr = nullptr;
    if (git_packbuilder_new(&pbPtr, repo) < 0)
        return false;
    std::unique_ptr<git_packbuilder, decltype(&git_packbuilder_free)> pb {pbPtr, git_packbuilder_free};
    // Commits first, then trees, then blobs: libgit2 packs best in recency
    // order, and the delta chains between successive checkpoint messages are
    // what makes the pack small.
    for (const auto& group : {std::cref(objects.commits), std::cref(objects.trees), std::cref(objects.blobs)})
        for (const auto& id : group.get())
            if (git_packbuilder_insert(pb.get(), &id, nullptr) < 0)
                return false;

    std::error_code ec;
    std::filesystem::create_directories(packDir, ec);

    // Remember the packs written by previous compactions. The new pack holds
    // every reachable object, so they become redundant; leaving them behind
    // would make each compaction *add* a full copy of the history.
    for (const auto& entry : std::filesystem::directory_iterator(packDir, ec))
        if (entry.is_regular_file(ec))
            stalePacks.push_back(entry.path());

    if (git_packbuilder_write(pb.get(), packDir.c_str(), 0, nullptr, nullptr) < 0) {
        JAMI_WARNING("[Document {}] Could not write pack: {}",
                     documentId,
                     git_error_last() ? git_error_last()->message : "unknown");
        return false;
    }

    // A pack is named after a hash of its own contents, so compacting twice over
    // an unchanged set of objects writes the same name again, over the file that
    // is already there -- and that name is in the list above. Deleting it would
    // empty the repository of everything it has. Only reachable after the walk,
    // git_packbuilder_name() returns nothing before the write.
    if (const char* written = git_packbuilder_name(pb.get())) {
        const auto fresh = fmt::format("pack-{}", written);
        stalePacks.erase(std::remove_if(stalePacks.begin(),
                                        stalePacks.end(),
                                        [&](const std::filesystem::path& p) { return p.stem().string() == fresh; }),
                         stalePacks.end());
    } else {
        // Without the name there is no way to tell the new pack from the old
        // ones: keep them all rather than risk removing it.
        stalePacks.clear();
    }

    // The pack is on disk and readable before anything is removed, so a reader
    // that misses a loose object finds it there.
    if (auto* odb = [&] {
            git_odb* o = nullptr;
            return git_repository_odb(&o, repo) == 0 ? o : nullptr;
        }()) {
        git_odb_refresh(odb);
        git_odb_free(odb);
    }
    return true;
}

/// Remove the loose files of objects that are now in the pack. Returns how many
/// were removed.
size_t
pruneLoose(const std::filesystem::path& objects, const ReachableObjects& packed)
{
    size_t pruned = 0;
    std::error_code ec;
    for (const auto& group : {std::cref(packed.commits), std::cref(packed.trees), std::cref(packed.blobs)}) {
        for (const auto& id : group.get()) {
            const auto hex = oidToString(id);
            if (std::filesystem::remove(objects / hex.substr(0, 2) / hex.substr(2), ec))
                ++pruned;
        }
    }
    return pruned;
}

} // namespace

bool
CollabRepository::compact(bool force)
{
    std::lock_guard lk(pimpl_->mutex_);
    if (!force && !worthCompacting(countObjects(path_)))
        return false;
    // Sampled before the walk, checked again before any pack is removed.
    const auto fetchesAtStart = pimpl_->fetchesStarted_.load();
    if (pimpl_->fetchesFinished_ != fetchesAtStart)
        return false; // a fetch is in flight
    auto repo = pimpl_->repository();
    if (!repo)
        return false;
    if (!pimpl_->headCommit(repo.get()))
        return false;

    // Walk the refs and gather everything they reach. Only these objects are
    // packed and pruned: anything left over by an interrupted write stays
    // untouched.
    ReachableObjects objects;
    if (!collectFromAllRefs(repo.get(), objects))
        return false;

    const auto packDir = (std::filesystem::path(path_) / "objects" / "pack").string();
    std::vector<std::filesystem::path> stalePacks;
    if (!writePack(repo.get(), objects, packDir, documentId_, stalePacks))
        return false;

    // Unlinking a pack another handle is reading is harmless on POSIX, and
    // libgit2 rescans when an object it expected is no longer where it was.
    // But a fetch that overlapped us may have written a pack we listed as stale
    // and pointed a ref at it after our walk: dropping it would lose objects no
    // one else has. Leaving the old packs in place merely postpones the gain to
    // the next compaction.
    if (pimpl_->fetchesStarted_ == fetchesAtStart && pimpl_->fetchesFinished_ == fetchesAtStart) {
        std::error_code ec;
        for (const auto& stale : stalePacks)
            std::filesystem::remove(stale, ec);
    } else {
        JAMI_DEBUG("[Document {}] A fetch overlapped compaction, keeping the previous packs", documentId_);
    }

    const auto pruned = pruneLoose(std::filesystem::path(path_) / "objects", objects);
    JAMI_LOG("[Document {}] Packed {} object(s), pruned {} loose file(s)", documentId_, objects.size(), pruned);
    return true;
}

bool
CollabRepository::isEmpty() const
{
    std::lock_guard lk(pimpl_->mutex_);
    auto repo = pimpl_->repository();
    if (!repo)
        return true;
    return pimpl_->headCommit(repo.get()) == nullptr;
}

std::vector<CollabRepository::HistoryEntry>
CollabRepository::history(size_t max) const
{
    std::lock_guard lk(pimpl_->mutex_);
    std::vector<HistoryEntry> out;
    auto repo = pimpl_->repository();
    if (!repo)
        return out;

    git_revwalk* walker_ptr = nullptr;
    if (git_revwalk_new(&walker_ptr, repo.get()) < 0)
        return out;
    GitRevWalker walker {walker_ptr};
    if (git_revwalk_push_ref(walker.get(), std::string(MAIN_REF).c_str()) < 0)
        return out;
    git_revwalk_sorting(walker.get(), GIT_SORT_TOPOLOGICAL | GIT_SORT_TIME);

    git_oid oid;
    while (git_revwalk_next(&oid, walker.get()) == 0) {
        git_commit* commit_ptr = nullptr;
        if (git_commit_lookup(&commit_ptr, repo.get(), &oid) < 0)
            continue;
        GitCommit commit {commit_ptr};
        HistoryEntry entry;
        entry.commitId = oidToString(oid);
        entry.timestamp = git_commit_time(commit.get());
        if (const auto* sig = git_commit_author(commit.get())) {
            entry.author = sig->name ? sig->name : "";
            entry.deviceId = sig->email ? sig->email : "";
        }
        if (const auto* msg = git_commit_message(commit.get())) {
            unsigned count = 0;
            const std::string_view view {msg};
            if (view.substr(0, collab::CHECKPOINT_PREFIX.size()) == collab::CHECKPOINT_PREFIX) {
                const auto* begin = view.data() + collab::CHECKPOINT_PREFIX.size();
                std::from_chars(begin, view.data() + view.size(), count);
            }
            entry.deltaCount = count;
        }
        out.push_back(std::move(entry));
        if (max && out.size() >= max)
            break;
    }
    return out;
}

bool
CollabRepository::fetch(const std::string& remoteDeviceId)
{
    // The device id is interpolated into a remote name, a channel URL and a ref
    // path. It reaches us from the network, so hold it to the same alphabet as
    // the document and conversation ids: no separator, no "..", nothing git or
    // the filesystem could read as anything but one path component.
    if (!isValidId(remoteDeviceId)) {
        JAMI_WARNING("[Account {}] [Document {}] Refusing to fetch from an invalid device id",
                     pimpl_->accountId_,
                     documentId_);
        return false;
    }
    auto repo = pimpl_->repository();
    if (!repo)
        return false;

    // Tell compaction that packs may appear behind its back.
    ++pimpl_->fetchesStarted_;
    struct FetchMark
    {
        std::atomic_uint& finished;
        ~FetchMark() { ++finished; }
    } mark {pimpl_->fetchesFinished_};

    git_remote* remotePtr = nullptr;
    {
        // Only the remote's registration is serialized: it rewrites the shared
        // config file. The transfer below must not hold this lock — it reads a
        // peer's socket, and a peer that simply stops writing would otherwise
        // block every checkpoint, and account teardown with them.
        std::lock_guard lk(pimpl_->mutex_);
        // The remote is named after the device, so several peers can be tracked
        // side by side in refs/remotes/<deviceId>/.
        auto res = git_remote_lookup(&remotePtr, repo.get(), remoteDeviceId.c_str());
        if (res == GIT_ENOTFOUND) {
            auto url = fmt::format("collab://{}/{}/{}", remoteDeviceId, conversationId_, documentId_);
            if (git_remote_create(&remotePtr, repo.get(), remoteDeviceId.c_str(), url.c_str()) < 0) {
                JAMI_ERROR("[Account {}] [Document {}] Unable to create remote {}",
                           pimpl_->accountId_,
                           documentId_,
                           remoteDeviceId);
                return false;
            }
        } else if (res != 0) {
            return false;
        }
    }
    GitRemote remote {remotePtr};

    // Safe to run unlocked: this repository handle is our own (a fresh one is
    // opened per call), and the fetch only writes refs/remotes/<device>/main,
    // which no other path writes and only mergeRemote reads. Concurrent fetches
    // for the same device are already deduplicated by the caller.

    git_fetch_options fetchOpts;
    git_fetch_options_init(&fetchOpts, GIT_FETCH_OPTIONS_VERSION);
    fetchOpts.follow_redirects = GIT_REMOTE_REDIRECT_NONE;
    fetchOpts.callbacks.transfer_progress = [](const git_indexer_progress* stats, void*) {
        // A document repository holds text deltas; anything this large means the
        // peer is not sending what we asked for.
        if (stats->received_bytes > MAX_FETCH_SIZE) {
            JAMI_ERROR("Abort fetching document repository, the fetch is too big: {} bytes", stats->received_bytes);
            return -1;
        }
        return 0;
    };

    auto refspec = fmt::format("+{}:refs/remotes/{}/main", MAIN_REF, remoteDeviceId);
    char* refspecPtr = refspec.data();
    git_strarray refspecs {&refspecPtr, 1};
    if (git_remote_fetch(remote.get(), &refspecs, &fetchOpts, "fetch") < 0) {
        const git_error* err = giterr_last();
        JAMI_WARNING("[Account {}] [Document {}] Unable to fetch from {}: {}",
                     pimpl_->accountId_,
                     documentId_,
                     remoteDeviceId,
                     err ? err->message : "(unknown)");
        return false;
    }
    return true;
}

bool
CollabRepository::mergeRemote(const std::string& remoteDeviceId)
{
    // Same reason as in fetch(): this value builds a ref path.
    if (!isValidId(remoteDeviceId)) {
        JAMI_WARNING("[Account {}] [Document {}] Refusing to merge from an invalid device id",
                     pimpl_->accountId_,
                     documentId_);
        return false;
    }
    std::lock_guard lk(pimpl_->mutex_);
    auto repo = pimpl_->repository();
    if (!repo)
        return false;

    auto remoteRef = fmt::format("refs/remotes/{}/main", remoteDeviceId);
    git_oid remoteOid;
    if (git_reference_name_to_id(&remoteOid, repo.get(), remoteRef.c_str()) < 0)
        return false;

    git_commit* remote_ptr = nullptr;
    if (git_commit_lookup(&remote_ptr, repo.get(), &remoteOid) < 0)
        return false;
    GitCommit remote {remote_ptr};

    git_tree* remoteTree_ptr = nullptr;
    if (git_commit_tree(&remoteTree_ptr, remote.get()) < 0)
        return false;
    GitTree remoteTree {remoteTree_ptr};

    // Checked here rather than in each branch below, so that no path can move
    // refs/heads/main onto a tip whose tree we have not looked inside.
    if (!treeShapeIsSupported(repo.get(), remoteTree.get())) {
        JAMI_WARNING("[Account {}] [Document {}] Refusing a remote tip whose tree is not "
                     "one this document could have written",
                     pimpl_->accountId_,
                     documentId_);
        return false;
    }

    auto local = pimpl_->headCommit(repo.get());

    // A peer that is allowed to open the channel is not thereby allowed to
    // dictate the contents of this document: the objects it serves are whatever
    // it chose to send. Only the signatures say who really wrote them, so the
    // whole history this fetch added is verified before any branch below can
    // move refs/heads/main onto it. What we already hold went through the same
    // check when it arrived and is hidden from the walk.
    if (!pimpl_->historyIsAuthentic(repo.get(),
                                    remoteOid,
                                    local ? git_commit_id(local.get()) : nullptr,
                                    conversationId_)) {
        JAMI_WARNING("[Account {}] [Document {}] Refusing a remote history from {}: it carries "
                     "commits that no member device signed",
                     pimpl_->accountId_,
                     documentId_,
                     remoteDeviceId);
        return false;
    }

    if (!local) {
        // Nothing local yet: adopt the remote tip verbatim.
        git_reference* ref = nullptr;
        if (git_reference_create(&ref, repo.get(), std::string(MAIN_REF).c_str(), &remoteOid, true, nullptr) < 0)
            return false;
        git_reference_free(ref);
        return true;
    }

    const auto localOid = *git_commit_id(local.get());
    if (git_oid_equal(&localOid, &remoteOid))
        return false;

    // Already up to date when the remote tip is an ancestor of ours.
    if (git_graph_descendant_of(repo.get(), &localOid, &remoteOid) == 1)
        return false;

    // Fast-forward when we have nothing the remote does not already have.
    if (git_graph_descendant_of(repo.get(), &remoteOid, &localOid) == 1) {
        git_reference* ref = nullptr;
        if (git_reference_create(&ref, repo.get(), std::string(MAIN_REF).c_str(), &remoteOid, true, nullptr) < 0)
            return false;
        git_reference_free(ref);
        return true;
    }

    auto localTree = pimpl_->headTree(local);

    // Diverged: union the trees. This never conflicts (see class documentation),
    // so no textual merge and no user-visible conflict can happen.
    git_oid mergedTree;
    if (!unionTrees(repo.get(), localTree.get(), remoteTree.get(), mergedTree, 0))
        return false;

    std::vector<git_commit*> parents {local.get(), remote.get()};
    return !pimpl_->commit(repo.get(), mergedTree, "merge: union of collaborative updates", parents).empty();
}

} // namespace jami
