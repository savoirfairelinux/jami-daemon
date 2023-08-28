/*
 *  Copyright (C) 2019-2023 Savoir-faire Linux Inc.
 *  Author: Sébastien Blin <sebastien.blin@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#include "conversationrepository.h"

#include "account_const.h"
#include "base64.h"
#include "jamiaccount.h"
#include "fileutils.h"
#include "gittransport.h"
#include "string_utils.h"
#include "client/ring_signal.h"
#include "vcard.h"

using random_device = dht::crypto::random_device;

#include <ctime>
#include <fstream>
#include <future>
#include <json/json.h>
#include <regex>
#include <exception>
#include <optional>

using namespace std::string_view_literals;
constexpr auto DIFF_REGEX = " +\\| +[0-9]+.*"sv;
constexpr size_t MAX_FETCH_SIZE {256 * 1024 * 1024}; // 256Mb

namespace jami {

static const std::regex regex_display_name("<|>");

inline std::string_view
as_view(const git_blob* blob)
{
    return std::string_view(static_cast<const char*>(git_blob_rawcontent(blob)),
                            git_blob_rawsize(blob));
}
inline std::string_view
as_view(const GitObject& blob)
{
    return as_view(reinterpret_cast<git_blob*>(blob.get()));
}

enum class CallbackResult { Skip, Break, Ok };

using PreConditionCb
    = std::function<CallbackResult(const std::string&, const GitAuthor&, const GitCommit&)>;
using PostConditionCb
    = std::function<bool(const std::string&, const GitAuthor&, ConversationCommit&)>;

class ConversationRepository::Impl
{
public:
    Impl(const std::weak_ptr<JamiAccount>& account, const std::string& id)
        : account_(account)
        , id_(id)
    {
        initMembers();
    }

    // NOTE! We use temporary GitRepository to avoid to keep file opened (TODO check why
    // git_remote_fetch() leaves pack-data opened)
    GitRepository repository() const
    {
        auto shared = account_.lock();
        if (!shared)
            return {nullptr, git_repository_free};
        auto path = fileutils::get_data_dir().string() + "/" + shared->getAccountID() + "/" + "conversations"
                    + "/" + id_;
        git_repository* repo = nullptr;
        auto err = git_repository_open(&repo, path.c_str());
        if (err < 0) {
            JAMI_ERROR("Couldn't open git repository: {} ({})",
                     path,
                     git_error_last()->message);
            return {nullptr, git_repository_free};
        }
        return {std::move(repo), git_repository_free};
    }

    std::string getDisplayName() const
    {
        auto shared = account_.lock();
        if (!shared)
            return {};
        auto name = shared->getDisplayName();
        if (name.empty())
            name = std::string(shared->currentDeviceId());
        return std::regex_replace(name, regex_display_name, "");
    }

    GitSignature signature();
    bool mergeFastforward(const git_oid* target_oid, int is_unborn);
    std::string createMergeCommit(git_index* index, const std::string& wanted_ref);

    bool validCommits(const std::vector<ConversationCommit>& commits) const;
    bool checkValidUserDiff(const std::string& userDevice,
                            const std::string& commitId,
                            const std::string& parentId) const;
    bool checkVote(const std::string& userDevice,
                   const std::string& commitId,
                   const std::string& parentId) const;
    bool checkEdit(const std::string& userDevice, const ConversationCommit& commit) const;
    bool isValidUserAtCommit(const std::string& userDevice, const std::string& commitId) const;
    bool checkInitialCommit(const std::string& userDevice,
                            const std::string& commitId,
                            const std::string& commitMsg) const;
    bool checkValidAdd(const std::string& userDevice,
                       const std::string& uriMember,
                       const std::string& commitid,
                       const std::string& parentId) const;
    bool checkValidJoins(const std::string& userDevice,
                         const std::string& uriMember,
                         const std::string& commitid,
                         const std::string& parentId) const;
    bool checkValidRemove(const std::string& userDevice,
                          const std::string& uriMember,
                          const std::string& commitid,
                          const std::string& parentId) const;
    bool checkValidVoteResolution(const std::string& userDevice,
                                  const std::string& uriMember,
                                  const std::string& commitId,
                                  const std::string& parentId,
                                  const std::string& voteType) const;
    bool checkValidProfileUpdate(const std::string& userDevice,
                                 const std::string& commitid,
                                 const std::string& parentId) const;

    bool add(const std::string& path);
    void addUserDevice();
    // Verify that the device in the repository is still valid
    bool validateDevice();
    std::string commit(const std::string& msg, bool verifyDevice = true);
    ConversationMode mode() const;

    // NOTE! GitDiff needs to be deteleted before repo
    GitDiff diff(git_repository* repo, const std::string& idNew, const std::string& idOld) const;
    std::string diffStats(const std::string& newId, const std::string& oldId) const;
    std::string diffStats(const GitDiff& diff) const;

    std::vector<ConversationCommit> behind(const std::string& from) const;
    void forEachCommit(PreConditionCb&& preCondition,
                       std::function<void(ConversationCommit&&)>&& emplaceCb,
                       PostConditionCb&& postCondition,
                       const std::string& from = "",
                       bool logIfNotFound = true) const;
    std::vector<ConversationCommit> log(const LogOptions& options) const;
    std::vector<std::map<std::string, std::string>> search(const Filter& filter) const;

    GitObject fileAtTree(const std::string& path, const GitTree& tree) const;
    GitObject memberCertificate(std::string_view memberUri, const GitTree& tree) const;
    // NOTE! GitDiff needs to be deteleted before repo
    GitTree treeAtCommit(git_repository* repo, const std::string& commitId) const;
    std::string getCommitType(const std::string& commitMsg) const;

    std::vector<std::string> getInitialMembers() const;

    bool resolveBan(const std::string_view type, const std::string& uri);
    bool resolveUnban(const std::string_view type, const std::string& uri);

    std::weak_ptr<JamiAccount> account_;
    const std::string id_;
    mutable std::optional<ConversationMode> mode_ {};

    // Members utils
    mutable std::mutex membersMtx_ {};
    std::vector<ConversationMember> members_ {};

    std::vector<ConversationMember> members() const
    {
        std::lock_guard<std::mutex> lk(membersMtx_);
        return members_;
    }

    std::map<std::string, std::vector<DeviceId>> devices(bool ignoreExpired = true) const
    {
        auto acc = account_.lock();
        auto repo = repository();
        if (!repo or !acc)
            return {};
        std::map<std::string, std::vector<DeviceId>> memberDevices;
        std::string deviceDir = fmt::format("{}devices/", git_repository_workdir(repo.get()));
        for (const auto& file: dhtnet::fileutils::readDirectory(deviceDir)) {
            std::shared_ptr<dht::crypto::Certificate> cert;
            try {
                cert = std::make_shared<dht::crypto::Certificate>(
                    fileutils::loadFile(deviceDir + file));
                if (!cert)
                    continue;
                if (ignoreExpired && cert->getExpiration() < std::chrono::system_clock::now())
                    continue;
                auto issuerUid = cert->getIssuerUID();
                if (!acc->certStore().getCertificate(issuerUid)) {
                    // Check that parentCert
                    auto memberFile = fmt::format("{}members/{}.crt", git_repository_workdir(repo.get()), issuerUid);
                    auto adminFile = fmt::format("{}admins/{}.crt", git_repository_workdir(repo.get()), issuerUid);
                    auto parentCert = std::make_shared<dht::crypto::Certificate>(dhtnet::fileutils::loadFile(std::filesystem::is_regular_file(memberFile) ? memberFile : adminFile));
                    if (parentCert && (ignoreExpired || parentCert->getExpiration() < std::chrono::system_clock::now()))
                        acc->certStore().pinCertificate(parentCert, true); // Pin certificate to local store if not already done
                }
                if (!acc->certStore().getCertificate(cert->getPublicKey().getLongId().toString())) {
                    acc->certStore().pinCertificate(cert, true); // Pin certificate to local store if not already done
                }
                memberDevices[cert->getIssuerUID()].emplace_back(cert->getPublicKey().getLongId());

            } catch (const std::exception&) {}
        }
        return memberDevices;
    }

    std::optional<ConversationCommit> getCommit(const std::string& commitId,
                                                bool logIfNotFound = true) const
    {
        LogOptions options;
        options.from = commitId;
        options.nbOfCommits = 1;
        options.logIfNotFound = logIfNotFound;
        auto commits = log(options);
        if (commits.empty())
            return std::nullopt;
        return std::move(commits[0]);
    }

    bool resolveConflicts(git_index* index, const std::string& other_id);

    std::vector<std::string> memberUris(std::string_view filter,
                                        const std::set<MemberRole>& filteredRoles) const
    {
        std::lock_guard<std::mutex> lk(membersMtx_);
        std::vector<std::string> ret;
        if (filter.empty() and filteredRoles.empty())
            ret.reserve(members_.size());
        for (const auto& member : members_) {
            if ((filteredRoles.find(member.role) != filteredRoles.end())
                or (not filter.empty() and filter == member.uri))
                continue;
            ret.emplace_back(member.uri);
        }
        return ret;
    }

    void initMembers();

    std::optional<std::map<std::string, std::string>> convCommitToMap(
        const ConversationCommit& commit) const;

    // Permissions
    MemberRole updateProfilePermLvl_ {MemberRole::ADMIN};

    /**
     * Retrieve the user related to a device
     * @note deviceToUri_ is used to cache result and avoid to
     * always load the certificate
     */
    std::string uriFromDevice(const std::string& deviceId) const
    {
        auto acc = account_.lock();
        if (!acc)
            return {};
        std::lock_guard<std::mutex> lk(deviceToUriMtx_);
        auto it = deviceToUri_.find(deviceId);
        if (it != deviceToUri_.end())
            return it->second;

        auto repo = repository();
        if (!repo)
            return {};

        auto cert = acc->certStore().getCertificate(deviceId);
        if (!cert || !cert->issuer) {
            // Not pinned, so load certificate from repo
            auto deviceFile = std::filesystem::path(git_repository_workdir(repo.get())) / "devices" / fmt::format("{}.crt", deviceId);
            if (!std::filesystem::is_regular_file(deviceFile))
                return {};
            try {
                cert = std::make_shared<dht::crypto::Certificate>(fileutils::loadFile(deviceFile));
            } catch (const std::exception&) {}
            if (!cert)
                return {};
        }
        auto issuerUid = cert->issuer ? cert->issuer->getId().toString() : cert->getIssuerUID();
        if (issuerUid.empty())
            return {};
        deviceToUri_.insert({deviceId, issuerUid});
        return issuerUid;
    }
    mutable std::mutex deviceToUriMtx_;
    mutable std::map<std::string, std::string> deviceToUri_;

    /**
     * Verify that a certificate modification is correct
     * @param certPath      Where the certificate is saved (relative path)
     * @param userUri       Account we want for this certificate
     * @param oldCert       Previous certificate. getId() should return the same id as the new
     * certificate.
     * @note There is a few exception because JAMS certificates are buggy right now
     */
    bool verifyCertificate(std::string_view certContent,
                           const std::string& userUri,
                           std::string_view oldCert = ""sv) const
    {
        auto cert = dht::crypto::Certificate((const uint8_t*) certContent.data(),
                                             certContent.size());
        auto isDeviceCertificate = cert.getId().toString() != userUri;
        auto issuerUid = cert.getIssuerUID();
        if (isDeviceCertificate && issuerUid.empty()) {
            // Err for Jams certificates
            JAMI_ERROR("Empty issuer for {}", cert.getId().toString());
        }
        if (!oldCert.empty()) {
            auto deviceCert = dht::crypto::Certificate((const uint8_t*) oldCert.data(),
                                                       oldCert.size());
            if (isDeviceCertificate) {
                if (issuerUid != deviceCert.getIssuerUID()) {
                    // NOTE: Here, because JAMS certificate can be incorrectly formatted, there is
                    // just one valid possibility: passing from an empty issuer to
                    // the valid issuer.
                    if (issuerUid != userUri) {
                        JAMI_ERROR("Device certificate with a bad issuer {}", cert.getId().toString());
                        return false;
                    }
                }
            } else if (cert.getId().toString() != userUri) {
                JAMI_ERROR("Certificate with a bad Id {}", cert.getId().toString());
                return false;
            }
            if (cert.getId() != deviceCert.getId()) {
                JAMI_ERROR("Certificate with a bad Id {}", cert.getId().toString());
                return false;
            }
            return true;
        }

        // If it's a device certificate, we need to verify that the issuer is not modified
        if (isDeviceCertificate) {
            // Check that issuer is the one we want.
            // NOTE: Still one case due to incorrectly formatted certificates from JAMS
            if (issuerUid != userUri && !issuerUid.empty()) {
                JAMI_ERROR("Device certificate with a bad issuer {}", cert.getId().toString());
                return false;
            }
        } else if (cert.getId().toString() != userUri) {
            JAMI_ERROR("Certificate with a bad Id {}", cert.getId().toString());
            return false;
        }

        return true;
    }
};

/////////////////////////////////////////////////////////////////////////////////

/**
 * Creates an empty repository
 * @param path       Path of the new repository
 * @return The libgit2's managed repository
 */
GitRepository
create_empty_repository(const std::string& path)
{
    git_repository* repo = nullptr;
    git_repository_init_options opts;
    git_repository_init_options_init(&opts, GIT_REPOSITORY_INIT_OPTIONS_VERSION);
    opts.flags |= GIT_REPOSITORY_INIT_MKPATH;
    opts.initial_head = "main";
    if (git_repository_init_ext(&repo, path.c_str(), &opts) < 0) {
        JAMI_ERROR("Couldn't create a git repository in {}", path);
    }
    return {std::move(repo), git_repository_free};
}

/**
 * Add all files to index
 * @param repo
 * @return if operation is successful
 */
bool
git_add_all(git_repository* repo)
{
    // git add -A
    git_index* index_ptr = nullptr;
    if (git_repository_index(&index_ptr, repo) < 0) {
        JAMI_ERROR("Could not open repository index");
        return false;
    }
    GitIndex index {index_ptr, git_index_free};
    git_strarray array {nullptr, 0};
    git_index_add_all(index.get(), &array, 0, nullptr, nullptr);
    git_index_write(index.get());
    git_strarray_dispose(&array);
    return true;
}

/**
 * Adds initial files. This adds the certificate of the account in the /admins directory
 * the device's key in /devices and the CRLs in /CRLs.
 * @param repo      The repository
 * @return if files were added successfully
 */
bool
add_initial_files(GitRepository& repo,
                  const std::shared_ptr<JamiAccount>& account,
                  ConversationMode mode,
                  const std::string& otherMember = "")
{
    auto deviceId = account->currentDeviceId();
    std::string repoPath = git_repository_workdir(repo.get());
    std::string adminsPath = repoPath + "admins";
    std::string devicesPath = repoPath + "devices";
    std::string invitedPath = repoPath + "invited";
    std::string crlsPath = repoPath + "CRLs" + DIR_SEPARATOR_STR + deviceId;

    if (!dhtnet::fileutils::recursive_mkdir(adminsPath, 0700)) {
        JAMI_ERROR("Error when creating %s. Abort create conversations", adminsPath.c_str());
        return false;
    }

    auto cert = account->identity().second;
    auto deviceCert = cert->toString(false);
    auto parentCert = cert->issuer;
    if (!parentCert) {
        JAMI_ERROR("Parent cert is null!");
        return false;
    }

    // /admins
    std::string adminPath = adminsPath + "/" + parentCert->getId().toString() + ".crt";
    auto file = fileutils::ofstream(adminPath, std::ios::trunc | std::ios::binary);
    if (!file.is_open()) {
        JAMI_ERROR("Could not write data to {}", adminPath);
        return false;
    }
    file << parentCert->toString(true);
    file.close();

    if (!dhtnet::fileutils::recursive_mkdir(devicesPath, 0700)) {
        JAMI_ERROR("Error when creating {}. Abort create conversations", devicesPath);
        return false;
    }

    // /devices
    std::string devicePath = devicesPath + "/" + deviceId + ".crt";
    file = fileutils::ofstream(devicePath, std::ios::trunc | std::ios::binary);
    if (!file.is_open()) {
        JAMI_ERROR("Could not write data to {}", devicePath);
        return false;
    }
    file << deviceCert;
    file.close();

    if (!dhtnet::fileutils::recursive_mkdir(crlsPath, 0700)) {
        JAMI_ERROR("Error when creating {}. Abort create conversations", crlsPath);
        return false;
    }

    // /CRLs
    for (const auto& crl : account->identity().second->getRevocationLists()) {
        if (!crl)
            continue;
        std::string crlPath = crlsPath + DIR_SEPARATOR_STR + deviceId + DIR_SEPARATOR_STR
                              + dht::toHex(crl->getNumber()) + ".crl";
        file = fileutils::ofstream(crlPath, std::ios::trunc | std::ios::binary);
        if (!file.is_open()) {
            JAMI_ERROR("Could not write data to {}", crlPath);
            return false;
        }
        file << crl->toString();
        file.close();
    }

    // /invited for one to one
    if (mode == ConversationMode::ONE_TO_ONE) {
        if (!dhtnet::fileutils::recursive_mkdir(invitedPath, 0700)) {
            JAMI_ERROR("Error when creating {}.", invitedPath);
            return false;
        }
        std::string invitedMemberPath = invitedPath + "/" + otherMember;
        if (std::filesystem::is_regular_file(invitedMemberPath)) {
            JAMI_WARNING("Member {} already present!", otherMember);
            return false;
        }

        auto file = fileutils::ofstream(invitedMemberPath, std::ios::trunc | std::ios::binary);
        if (!file.is_open()) {
            JAMI_ERROR("Could not write data to {}", invitedMemberPath);
            return false;
        }
    }

    if (!git_add_all(repo.get())) {
        return false;
    }

    JAMI_LOG("Initial files added in {}", repoPath);
    return true;
}

/**
 * Sign and create the initial commit
 * @param repo          The git repository
 * @param account       The account who signs
 * @param mode          The mode
 * @param otherMember   If one to one
 * @return          The first commit hash or empty if failed
 */
std::string
initial_commit(GitRepository& repo,
                const std::shared_ptr<JamiAccount>& account,
                ConversationMode mode,
                const std::string& otherMember = "")
{
    auto deviceId = std::string(account->currentDeviceId());
    auto name = account->getDisplayName();
    if (name.empty())
        name = deviceId;
    name = std::regex_replace(name, regex_display_name, "");

    git_signature* sig_ptr = nullptr;
    git_index* index_ptr = nullptr;
    git_oid tree_id, commit_id;
    git_tree* tree_ptr = nullptr;

    // Sign commit's buffer
    if (git_signature_new(&sig_ptr, name.c_str(), deviceId.c_str(), std::time(nullptr), 0) < 0) {
        if (git_signature_new(&sig_ptr, deviceId.c_str(), deviceId.c_str(), std::time(nullptr), 0) < 0) {
            JAMI_ERROR("Unable to create a commit signature.");
            return {};
        }
    }
    GitSignature sig {sig_ptr, git_signature_free};

    if (git_repository_index(&index_ptr, repo.get()) < 0) {
        JAMI_ERROR("Could not open repository index");
        return {};
    }
    GitIndex index {index_ptr, git_index_free};

    if (git_index_write_tree(&tree_id, index.get()) < 0) {
        JAMI_ERROR("Unable to write initial tree from index");
        return {};
    }

    if (git_tree_lookup(&tree_ptr, repo.get(), &tree_id) < 0) {
        JAMI_ERROR("Could not look up initial tree");
        return {};
    }
    GitTree tree = {tree_ptr, git_tree_free};

    Json::Value json;
    json["mode"] = static_cast<int>(mode);
    if (mode == ConversationMode::ONE_TO_ONE) {
        json["invited"] = otherMember;
    }
    json["type"] = "initial";
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";

    git_buf to_sign = {};
    if (git_commit_create_buffer(&to_sign,
                                 repo.get(),
                                 sig.get(),
                                 sig.get(),
                                 nullptr,
                                 Json::writeString(wbuilder, json).c_str(),
                                 tree.get(),
                                 0,
                                 nullptr)
        < 0) {
        JAMI_ERROR("Could not create initial buffer");
        return {};
    }

    std::string signed_str = base64::encode(
        account->identity().first->sign((const uint8_t*) to_sign.ptr, to_sign.size));

    // git commit -S
    if (git_commit_create_with_signature(&commit_id,
                                         repo.get(),
                                         to_sign.ptr,
                                         signed_str.c_str(),
                                         "signature")
        < 0) {
        git_buf_dispose(&to_sign);
        JAMI_ERROR("Could not sign initial commit");
        return {};
    }
    git_buf_dispose(&to_sign);

    // Move commit to main branch
    git_commit* commit = nullptr;
    if (git_commit_lookup(&commit, repo.get(), &commit_id) == 0) {
        git_reference* ref = nullptr;
        git_branch_create(&ref, repo.get(), "main", commit, true);
        git_commit_free(commit);
        git_reference_free(ref);
    }

    auto commit_str = git_oid_tostr_s(&commit_id);
    if (commit_str)
        return commit_str;
    return {};
}

//////////////////////////////////

GitSignature
ConversationRepository::Impl::signature()
{
    auto account = account_.lock();
    auto name = getDisplayName();
    if (!account || name.empty())
        return {nullptr, git_signature_free};

    git_signature* sig_ptr = nullptr;
    // Sign commit's buffer
    auto deviceId = std::string(account->currentDeviceId());
    if (git_signature_new(&sig_ptr, name.c_str(), deviceId.c_str(), std::time(nullptr), 0) < 0) {
        // Maybe the display name is invalid (like " ") - try without
        if (git_signature_new(&sig_ptr, deviceId.c_str(), deviceId.c_str(), std::time(nullptr), 0) < 0) {
            JAMI_ERROR("Unable to create a commit signature.");
            return {nullptr, git_signature_free};
        }
    }
    return {sig_ptr, git_signature_free};
}

std::string
ConversationRepository::Impl::createMergeCommit(git_index* index, const std::string& wanted_ref)
{
    if (!validateDevice()) {
        JAMI_ERROR("Invalid device. Not migrated?");
        return {};
    }
    // The merge will occur between current HEAD and wanted_ref
    git_reference* head_ref_ptr = nullptr;
    auto repo = repository();
    if (!repo || git_repository_head(&head_ref_ptr, repo.get()) < 0) {
        JAMI_ERROR("Could not get HEAD reference");
        return {};
    }
    GitReference head_ref {head_ref_ptr, git_reference_free};

    // Maybe that's a ref, so DWIM it
    git_reference* merge_ref_ptr = nullptr;
    git_reference_dwim(&merge_ref_ptr, repo.get(), wanted_ref.c_str());
    GitReference merge_ref {merge_ref_ptr, git_reference_free};

    GitSignature sig {signature()};

    // Prepare a standard merge commit message
    const char* msg_target = nullptr;
    if (merge_ref) {
        git_branch_name(&msg_target, merge_ref.get());
    } else {
        msg_target = wanted_ref.c_str();
    }

    auto commitMsg = fmt::format("Merge {} '{}'", merge_ref ? "branch" : "commit", msg_target);

    // Setup our parent commits
    GitCommit parents[2] {{nullptr, git_commit_free}, {nullptr, git_commit_free}};
    git_commit* parent = nullptr;
    if (git_reference_peel((git_object**) &parent, head_ref.get(), GIT_OBJ_COMMIT) < 0) {
        JAMI_ERROR("Could not peel HEAD reference");
        return {};
    }
    parents[0] = {parent, git_commit_free};
    git_oid commit_id;
    if (git_oid_fromstr(&commit_id, wanted_ref.c_str()) < 0) {
        return {};
    }
    git_annotated_commit* annotated_ptr = nullptr;
    if (git_annotated_commit_lookup(&annotated_ptr, repo.get(), &commit_id) < 0) {
        JAMI_ERROR("Couldn't lookup commit {}", wanted_ref);
        return {};
    }
    GitAnnotatedCommit annotated {annotated_ptr, git_annotated_commit_free};
    if (git_commit_lookup(&parent, repo.get(), git_annotated_commit_id(annotated.get())) < 0) {
        JAMI_ERROR("Couldn't lookup commit {}", wanted_ref);
        return {};
    }
    parents[1] = {parent, git_commit_free};

    // Prepare our commit tree
    git_oid tree_oid;
    git_tree* tree_ptr = nullptr;
    if (git_index_write_tree_to(&tree_oid, index, repo.get()) < 0) {
        const git_error* err = giterr_last();
        if (err)
            JAMI_ERROR("Couldn't write index: {}", err->message);
        return {};
    }
    if (git_tree_lookup(&tree_ptr, repo.get(), &tree_oid) < 0) {
        JAMI_ERROR("Couldn't lookup tree");
        return {};
    }
    GitTree tree = {tree_ptr, git_tree_free};

    // Commit
    git_buf to_sign = {};
    const git_commit* parents_ptr[2] {parents[0].get(), parents[1].get()};
    if (git_commit_create_buffer(&to_sign,
                                 repo.get(),
                                 sig.get(),
                                 sig.get(),
                                 nullptr,
                                 commitMsg.c_str(),
                                 tree.get(),
                                 2,
                                 &parents_ptr[0])
        < 0) {
        const git_error* err = giterr_last();
        if (err)
            JAMI_ERROR("Could not create commit buffer: {}", err->message);
        return {};
    }

    auto account = account_.lock();
    if (!account)
        return {};
    // git commit -S
    auto to_sign_vec = std::vector<uint8_t>(to_sign.ptr, to_sign.ptr + to_sign.size);
    auto signed_buf = account->identity().first->sign(to_sign_vec);
    std::string signed_str = base64::encode(signed_buf);
    git_oid commit_oid;
    if (git_commit_create_with_signature(&commit_oid,
                                         repo.get(),
                                         to_sign.ptr,
                                         signed_str.c_str(),
                                         "signature")
        < 0) {
        git_buf_dispose(&to_sign);
        JAMI_ERROR("Could not sign commit");
        return {};
    }
    git_buf_dispose(&to_sign);

    auto commit_str = git_oid_tostr_s(&commit_oid);
    if (commit_str) {
        JAMI_LOG("New merge commit added with id: {}", commit_str);
        // Move commit to main branch
        git_reference* ref_ptr = nullptr;
        if (git_reference_create(&ref_ptr, repo.get(), "refs/heads/main", &commit_oid, true, nullptr)
            < 0) {
            const git_error* err = giterr_last();
            if (err) {
                JAMI_ERROR("Could not move commit to main: {}", err->message);
                emitSignal<libjami::ConversationSignal::OnConversationError>(
                    account->getAccountID(),
                    id_,
                    ECOMMIT,
                    err->message);
            }
            return {};
        }
        git_reference_free(ref_ptr);
    }

    // We're done merging, cleanup the repository state & index
    git_repository_state_cleanup(repo.get());

    git_object* target_ptr = nullptr;
    if (git_object_lookup(&target_ptr, repo.get(), &commit_oid, GIT_OBJ_COMMIT) != 0) {
        const git_error* err = giterr_last();
        if (err)
            JAMI_ERROR("failed to lookup OID {}: {}", git_oid_tostr_s(&commit_oid), err->message);
        return {};
    }
    GitObject target {target_ptr, git_object_free};

    git_reset(repo.get(), target.get(), GIT_RESET_HARD, nullptr);

    return commit_str ? commit_str : "";
}

bool
ConversationRepository::Impl::mergeFastforward(const git_oid* target_oid, int is_unborn)
{
    // Initialize target
    git_reference* target_ref_ptr = nullptr;
    auto repo = repository();
    if (!repo) {
        JAMI_ERROR("No repository found");
        return false;
    }
    if (is_unborn) {
        git_reference* head_ref_ptr = nullptr;
        // HEAD reference is unborn, lookup manually so we don't try to resolve it
        if (git_reference_lookup(&head_ref_ptr, repo.get(), "HEAD") < 0) {
            JAMI_ERROR("failed to lookup HEAD ref");
            return false;
        }
        GitReference head_ref {head_ref_ptr, git_reference_free};

        // Grab the reference HEAD should be pointing to
        const auto* symbolic_ref = git_reference_symbolic_target(head_ref.get());

        // Create our main reference on the target OID
        if (git_reference_create(&target_ref_ptr, repo.get(), symbolic_ref, target_oid, 0, nullptr)
            < 0) {
            const git_error* err = giterr_last();
            if (err)
                JAMI_ERROR("failed to create main reference: {}", err->message);
            return false;
        }

    } else if (git_repository_head(&target_ref_ptr, repo.get()) < 0) {
        // HEAD exists, just lookup and resolve
        JAMI_ERROR("failed to get HEAD reference");
        return false;
    }
    GitReference target_ref {target_ref_ptr, git_reference_free};

    // Lookup the target object
    git_object* target_ptr = nullptr;
    if (git_object_lookup(&target_ptr, repo.get(), target_oid, GIT_OBJ_COMMIT) != 0) {
        JAMI_ERROR("failed to lookup OID {}", git_oid_tostr_s(target_oid));
        return false;
    }
    GitObject target {target_ptr, git_object_free};

    // Checkout the result so the workdir is in the expected state
    git_checkout_options ff_checkout_options;
    git_checkout_init_options(&ff_checkout_options, GIT_CHECKOUT_OPTIONS_VERSION);
    ff_checkout_options.checkout_strategy = GIT_CHECKOUT_SAFE;
    if (git_checkout_tree(repo.get(), target.get(), &ff_checkout_options) != 0) {
        JAMI_ERROR("failed to checkout HEAD reference");
        return false;
    }

    // Move the target reference to the target OID
    git_reference* new_target_ref;
    if (git_reference_set_target(&new_target_ref, target_ref.get(), target_oid, nullptr) < 0) {
        JAMI_ERROR("failed to move HEAD reference");
        return false;
    }
    git_reference_free(new_target_ref);

    return true;
}

bool
ConversationRepository::Impl::add(const std::string& path)
{
    auto repo = repository();
    if (!repo)
        return false;
    git_index* index_ptr = nullptr;
    if (git_repository_index(&index_ptr, repo.get()) < 0) {
        JAMI_ERROR("Could not open repository index");
        return false;
    }
    GitIndex index {index_ptr, git_index_free};
    if (git_index_add_bypath(index.get(), path.c_str()) != 0) {
        const git_error* err = giterr_last();
        if (err)
            JAMI_ERROR("Error when adding file: {}", err->message);
        return false;
    }
    return git_index_write(index.get()) == 0;
}

bool
ConversationRepository::Impl::checkValidUserDiff(const std::string& userDevice,
                                                 const std::string& commitId,
                                                 const std::string& parentId) const
{
    // Retrieve tree for recent commit
    auto repo = repository();
    if (!repo)
        return false;
    auto treeNew = treeAtCommit(repo.get(), commitId);
    if (not treeNew)
        return false;
    std::string deviceFile = fmt::format("devices/{}.crt", userDevice);
    if (!fileAtTree(deviceFile, treeNew)) {
        JAMI_ERROR("{} announced but not found", deviceFile.c_str());
        return false;
    }

    // Here, we check that a file device is modified or not.
    auto changedFiles = ConversationRepository::changedFiles(diffStats(commitId, parentId));
    if (changedFiles.size() == 0)
        return true;

    // If a certificate is modified (in the changedFiles), it MUST be a certificate from the user
    // Retrieve userUri
    auto userUri = uriFromDevice(userDevice);
    if (userUri.empty())
        return false;

    std::string adminsFile = fmt::format("admins/{}.crt", userUri);
    std::string membersFile = fmt::format("members/{}.crt", userUri);
    auto treeOld = treeAtCommit(repo.get(), parentId);
    if (not treeNew or not treeOld)
        return false;
    for (const auto& changedFile : changedFiles) {
        if (changedFile == adminsFile || changedFile == membersFile) {
            // In this case, we should verify it's not added (normal commit, not a member change)
            // but only updated
            auto oldFile = fileAtTree(changedFile, treeOld);
            if (!oldFile) {
                JAMI_ERROR("Invalid file modified: {}", changedFile);
                return false;
            }
            auto newFile = fileAtTree(changedFile, treeNew);
            if (!verifyCertificate(as_view(newFile), userUri, as_view(oldFile))) {
                JAMI_ERROR("Invalid certificate {}", changedFile);
                return false;
            }
        } else if (changedFile == deviceFile) {
            // In this case, device is added or modified (certificate expiration)
            auto oldFile = fileAtTree(changedFile, treeOld);
            std::string_view oldCert;
            if (oldFile)
                oldCert = as_view(oldFile);
            auto newFile = fileAtTree(changedFile, treeNew);
            if (!verifyCertificate(as_view(newFile), userUri, oldCert)) {
                JAMI_ERROR("Invalid certificate {}", changedFile);
                return false;
            }
        } else {
            // Invalid file detected
            JAMI_ERROR("Invalid add file detected: {} {}", changedFile, (int) *mode_);
            return false;
        }
    }

    return true;
}

bool
ConversationRepository::Impl::checkEdit(const std::string& userDevice,
                                        const ConversationCommit& commit) const
{
    auto userUri = uriFromDevice(userDevice);
    // Check that edited commit is found, for the same author, and editable (plain/text)
    auto commitMap = convCommitToMap(commit);
    if (commitMap == std::nullopt) {
        return false;
    }
    auto editedId = commitMap->at("edit");
    auto editedCommit = getCommit(editedId);
    if (editedCommit == std::nullopt) {
        JAMI_ERROR("Commit {:s} not found", editedId);
        return false;
    }
    auto editedCommitMap = convCommitToMap(*editedCommit);
    if (editedCommitMap == std::nullopt or editedCommitMap->at("author").empty()
        or editedCommitMap->at("author") != commitMap->at("author")
        or commitMap->at("author") != userUri) {
        JAMI_ERROR("Edited commit {:s} got a different author ({:s})", editedId, commit.id);
        return false;
    }
    if (editedCommitMap->at("type") != "text/plain") {
        JAMI_ERROR("Edited commit {:s} is not text!", editedId);
        return false;
    }
    return true;
}

bool
ConversationRepository::Impl::checkVote(const std::string& userDevice,
                                        const std::string& commitId,
                                        const std::string& parentId) const
{
    // Check that maximum deviceFile and a vote is added
    auto changedFiles = ConversationRepository::changedFiles(diffStats(commitId, parentId));
    if (changedFiles.size() == 0) {
        return true;
    } else if (changedFiles.size() > 2) {
        return false;
    }
    // If modified, it's the first commit of a device, we check
    // that the file wasn't there previously. And the vote MUST be added
    std::string deviceFile = "";
    std::string votedFile = "";
    for (const auto& changedFile : changedFiles) {
        // NOTE: libgit2 return a diff with /, not DIR_SEPARATOR_DIR
        if (changedFile == fmt::format("devices/{}.crt", userDevice)) {
            deviceFile = changedFile;
        } else if (changedFile.find("votes") == 0) {
            votedFile = changedFile;
        } else {
            // Invalid file detected
            JAMI_ERROR("Invalid vote file detected: {}", changedFile);
            return false;
        }
    }

    if (votedFile.empty()) {
        JAMI_WARNING("No vote detected for commit {}", commitId);
        return false;
    }

    auto repo = repository();
    if (!repo)
        return false;
    auto treeNew = treeAtCommit(repo.get(), commitId);
    auto treeOld = treeAtCommit(repo.get(), parentId);
    if (not treeNew or not treeOld)
        return false;

    if (not deviceFile.empty()) {
        if (fileAtTree(deviceFile, treeOld)) {
            JAMI_ERROR("Invalid device file modified: {}", deviceFile);
            return false;
        }
    }

    auto userUri = uriFromDevice(userDevice);
    // Check that voter is admin
    auto adminFile = fmt::format("admins/{}.crt", userUri);

    if (!fileAtTree(adminFile, treeOld)) {
        JAMI_ERROR("Vote from non admin: {}", userUri);
        return false;
    }

    // Check votedFile path
    static const std::regex regex_votes(
        "votes.(\\w+).(members|devices|admins|invited).(\\w+).(\\w+)");
    std::svmatch base_match;
    if (!std::regex_match(votedFile, base_match, regex_votes) or base_match.size() != 5) {
        JAMI_WARNING("Invalid votes path: {}", votedFile);
        return false;
    }

    std::string_view matchedUri = svsub_match_view(base_match[4]);
    if (matchedUri != userUri) {
        JAMI_ERROR("Admin voted for other user: {:s} vs {:s}", userUri, matchedUri);
        return false;
    }
    std::string_view votedUri = svsub_match_view(base_match[3]);
    std::string_view type = svsub_match_view(base_match[2]);
    std::string_view voteType = svsub_match_view(base_match[1]);
    if (voteType != "ban" && voteType != "unban") {
        JAMI_ERROR("Unrecognized vote {:s}", voteType);
        return false;
    }

    // Check that vote file is empty and wasn't modified
    if (fileAtTree(votedFile, treeOld)) {
        JAMI_ERROR("Invalid voted file modified: {:s}", votedFile);
        return false;
    }
    auto vote = fileAtTree(votedFile, treeNew);
    if (!vote) {
        JAMI_ERROR("No vote file found for: {:s}", userUri);
        return false;
    }
    auto voteContent = as_view(vote);
    if (!voteContent.empty()) {
        JAMI_ERROR("Vote file not empty: {:s}", votedFile);
        return false;
    }

    // Check that peer voted is only other device or other member
    if (type != "devices") {
        if (votedUri == userUri) {
            JAMI_ERROR("Detected vote for self: {:s}", votedUri);
            return false;
        }
        if (voteType == "ban") {
            // file in members or admin or invited
            auto invitedFile = fmt::format("invited/{}", votedUri);
            if (!memberCertificate(votedUri, treeOld) && !fileAtTree(invitedFile, treeOld)) {
                JAMI_ERROR("No member file found for vote: {:s}", votedUri);
                return false;
            }
        }
    } else {
        // Check not current device
        if (votedUri == userDevice) {
            JAMI_ERROR("Detected vote for self: {:s}", votedUri);
            return false;
        }
        // File in devices
        deviceFile = fmt::format("devices/{}.crt", votedUri);
        if (!fileAtTree(deviceFile, treeOld)) {
            JAMI_ERROR("No device file found for vote: {:s}", votedUri);
            return false;
        }
    }

    return true;
}

bool
ConversationRepository::Impl::checkValidAdd(const std::string& userDevice,
                                            const std::string& uriMember,
                                            const std::string& commitId,
                                            const std::string& parentId) const
{
    auto userUri = uriFromDevice(userDevice);
    auto repo = repository();
    if (userUri.empty() or not repo)
        return false;

    std::string repoPath = git_repository_workdir(repo.get());
    if (mode() == ConversationMode::ONE_TO_ONE) {
        auto initialMembers = getInitialMembers();
        auto it = std::find(initialMembers.begin(), initialMembers.end(), uriMember);
        if (it == initialMembers.end()) {
            JAMI_ERROR("Invalid add in one to one conversation: {}", uriMember);
            return false;
        }
    }

    // Check that only /invited/uri.crt is added & deviceFile & CRLs
    auto changedFiles = ConversationRepository::changedFiles(diffStats(commitId, parentId));
    if (changedFiles.size() == 0) {
        return false;
    } else if (changedFiles.size() > 3) {
        return false;
    }

    // Check that user added is not sender
    if (userUri == uriMember) {
        JAMI_ERROR("Member tried to add self: {}", userUri);
        return false;
    }

    // If modified, it's the first commit of a device, we check
    // that the file wasn't there previously. And the member MUST be added
    // NOTE: libgit2 return a diff with /, not DIR_SEPARATOR_DIR
    std::string deviceFile = "";
    std::string invitedFile = "";
    std::string crlFile = std::string("CRLs") + "/" + userUri;
    for (const auto& changedFile : changedFiles) {
        if (changedFile == std::string("devices") + "/" + userDevice + ".crt") {
            deviceFile = changedFile;
        } else if (changedFile == std::string("invited") + "/" + uriMember) {
            invitedFile = changedFile;
        } else if (changedFile == crlFile) {
            // Nothing to do
        } else {
            // Invalid file detected
            JAMI_ERROR("Invalid add file detected: {}", changedFile);
            return false;
        }
    }

    auto treeOld = treeAtCommit(repo.get(), parentId);
    if (not treeOld)
        return false;
    auto treeNew = treeAtCommit(repo.get(), commitId);
    if (not treeNew)
        return false;
    auto blob_invite = fileAtTree(invitedFile, treeNew);
    if (!blob_invite) {
        JAMI_ERROR("Invitation not found for commit {}", commitId);
        return false;
    }

    auto invitation = as_view(blob_invite);
    if (!invitation.empty()) {
        JAMI_ERROR("Invitation not empty for commit {}", commitId);
        return false;
    }

    // Check that user not in /banned
    std::string bannedFile = std::string("banned") + "/" + "members" + "/" + uriMember + ".crt";
    if (fileAtTree(bannedFile, treeOld)) {
        JAMI_ERROR("Tried to add banned member: {}", bannedFile);
        return false;
    }

    return true;
}

bool
ConversationRepository::Impl::checkValidJoins(const std::string& userDevice,
                                              const std::string& uriMember,
                                              const std::string& commitId,
                                              const std::string& parentId) const
{
    // Check no other files changed
    auto changedFiles = ConversationRepository::changedFiles(diffStats(commitId, parentId));
    auto invitedFile = fmt::format("invited/{}", uriMember);
    auto membersFile = fmt::format("members/{}.crt", uriMember);
    auto deviceFile = fmt::format("devices/{}.crt", userDevice);

    for (auto& file : changedFiles) {
        if (file != invitedFile && file != membersFile && file != deviceFile) {
            JAMI_ERROR("Unwanted file {} found", file);
            return false;
        }
    }

    // Retrieve tree for commits
    auto repo = repository();
    assert(repo);
    auto treeNew = treeAtCommit(repo.get(), commitId);
    auto treeOld = treeAtCommit(repo.get(), parentId);
    if (not treeNew or not treeOld)
        return false;

    // Check /invited
    if (fileAtTree(invitedFile, treeNew)) {
        JAMI_ERROR("{} invited not removed", uriMember);
        return false;
    }
    if (!fileAtTree(invitedFile, treeOld)) {
        JAMI_ERROR("{} invited not found", uriMember);
        return false;
    }

    // Check /members added
    if (!fileAtTree(membersFile, treeNew)) {
        JAMI_ERROR("{} members not found", uriMember);
        return false;
    }
    if (fileAtTree(membersFile, treeOld)) {
        JAMI_ERROR("{} members found too soon", uriMember);
        return false;
    }

    // Check /devices added
    if (!fileAtTree(deviceFile, treeNew)) {
        JAMI_ERROR("{} devices not found", uriMember);
        return false;
    }

    // Check certificate
    auto blob_device = fileAtTree(deviceFile, treeNew);
    if (!blob_device) {
        JAMI_ERROR("{} announced but not found", deviceFile);
        return false;
    }
    auto deviceCert = dht::crypto::Certificate(as_view(blob_device));
    auto blob_member = fileAtTree(membersFile, treeNew);
    if (!blob_member) {
        JAMI_ERROR("{} announced but not found", userDevice);
        return false;
    }
    auto memberCert = dht::crypto::Certificate(as_view(blob_member));
    if (memberCert.getId().toString() != deviceCert.getIssuerUID() || deviceCert.getIssuerUID() != uriMember) {
        JAMI_ERROR("Incorrect device certificate {} for user {}", userDevice, uriMember);
        return false;
    }

    return true;
}

bool
ConversationRepository::Impl::checkValidRemove(const std::string& userDevice,
                                               const std::string& uriMember,
                                               const std::string& commitId,
                                               const std::string& parentId) const
{
    auto userUri = uriFromDevice(userDevice);

    // Retrieve tree for recent commit
    auto repo = repository();
    if (!repo)
        return false;
    auto treeOld = treeAtCommit(repo.get(), parentId);
    if (not treeOld)
        return false;

    auto changedFiles = ConversationRepository::changedFiles(diffStats(commitId, parentId));
    // NOTE: libgit2 return a diff with /, not DIR_SEPARATOR_DIR
    std::string deviceFile = fmt::format("devices/{}.crt", userDevice);
    std::string adminFile = fmt::format("admins/{}.crt", uriMember);
    std::string memberFile = fmt::format("members/{}.crt", uriMember);
    std::string crlFile = fmt::format("CRLs/{}", uriMember);
    std::string invitedFile = fmt::format("invited/{}", uriMember);
    std::vector<std::string> devicesRemoved;

    // Check that no weird file is added nor removed
    const std::regex regex_devices("devices.(\\w+)\\.crt");
    std::smatch base_match;
    for (const auto& f : changedFiles) {
        if (f == deviceFile || f == adminFile || f == memberFile || f == crlFile
            || f == invitedFile) {
            // Ignore
            continue;
        } else if (std::regex_match(f, base_match, regex_devices)) {
            if (base_match.size() == 2)
                devicesRemoved.emplace_back(base_match[1]);
        } else {
            JAMI_ERROR("Unwanted changed file detected: {}", f);
            return false;
        }
    }

    // Check that removed devices are for removed member (or directly uriMember)
    for (const auto& deviceUri : devicesRemoved) {
        deviceFile = fmt::format("devices/{}.crt", deviceUri);
        if (!fileAtTree(deviceFile, treeOld)) {
            JAMI_ERROR("device not found added ({})", deviceFile);
            return false;
        }
        if (uriMember != uriFromDevice(deviceUri)
            and uriMember != deviceUri /* If device is removed */) {
            JAMI_ERROR("device removed but not for removed user ({})", deviceFile);
            return false;
        }
    }

    return true;
}

bool
ConversationRepository::Impl::checkValidVoteResolution(const std::string& userDevice,
                                                       const std::string& uriMember,
                                                       const std::string& commitId,
                                                       const std::string& parentId,
                                                       const std::string& voteType) const
{
    auto userUri = uriFromDevice(userDevice);

    // Retrieve tree for recent commit
    auto repo = repository();
    if (!repo)
        return false;
    auto treeOld = treeAtCommit(repo.get(), parentId);
    if (not treeOld)
        return false;

    auto changedFiles = ConversationRepository::changedFiles(diffStats(commitId, parentId));
    // NOTE: libgit2 return a diff with /, not DIR_SEPARATOR_DIR
    std::string deviceFile = fmt::format("devices/{}.crt", userDevice);
    std::string adminFile = fmt::format("admins/{}.crt", uriMember);
    std::string memberFile = fmt::format("members/{}.crt", uriMember);
    std::string crlFile = fmt::format("CRLs/{}", uriMember);
    std::string invitedFile = fmt::format("invited/{}", uriMember);
    std::vector<std::string> voters;
    std::vector<std::string> devicesRemoved;
    std::vector<std::string> bannedFiles;
    // Check that no weird file is added nor removed

    const std::regex regex_votes("votes." + voteType
                                 + ".(members|devices|admins|invited).(\\w+).(\\w+)");
    const std::regex regex_devices("devices.(\\w+)\\.crt");
    const std::regex regex_banned("banned.(members|devices|admins).(\\w+)\\.crt");
    const std::regex regex_banned_invited("banned.(invited).(\\w+)");
    std::smatch base_match;
    for (const auto& f : changedFiles) {
        if (f == deviceFile || f == adminFile || f == memberFile || f == crlFile
            || f == invitedFile) {
            // Ignore
            continue;
        } else if (std::regex_match(f, base_match, regex_votes)) {
            if (base_match.size() != 4 or base_match[2] != uriMember) {
                JAMI_ERROR("Invalid vote file detected: {}", f);
                return false;
            }
            voters.emplace_back(base_match[3]);
            // Check that votes were not added here
            if (!fileAtTree(f, treeOld)) {
                JAMI_ERROR("invalid vote added ({})", f);
                return false;
            }
        } else if (std::regex_match(f, base_match, regex_devices)) {
            if (base_match.size() == 2)
                devicesRemoved.emplace_back(base_match[1]);
        } else if (std::regex_match(f, base_match, regex_banned)
                   || std::regex_match(f, base_match, regex_banned_invited)) {
            bannedFiles.emplace_back(f);
            if (base_match.size() != 3 or base_match[2] != uriMember) {
                JAMI_ERROR("Invalid banned file detected : {}", f);
                return false;
            }
        } else {
            JAMI_ERROR("Unwanted changed file detected: {}", f);
            return false;
        }
    }

    // Check that removed devices are for removed member (or directly uriMember)
    for (const auto& deviceUri : devicesRemoved) {
        deviceFile = fmt::format("devices/{}.crt", deviceUri);
        if (voteType == "ban") {
            // If we ban a device, it should be there before
            if (!fileAtTree(deviceFile, treeOld)) {
                JAMI_ERROR("device not found added ({})", deviceFile);
                return false;
            }
        } else if (voteType == "unban") {
            // If we unban a device, it should not be there before
            if (fileAtTree(deviceFile, treeOld)) {
                JAMI_ERROR("device not found added ({})", deviceFile);
                return false;
            }
        }
        if (uriMember != uriFromDevice(deviceUri)
            and uriMember != deviceUri /* If device is removed */) {
            JAMI_ERROR("device removed but not for removed user ({})", deviceFile);
            return false;
        }
    }

    // Check that voters are admins
    adminFile = fmt::format("admins/{}.crt", userUri);
    if (!fileAtTree(adminFile, treeOld)) {
        JAMI_ERROR("admin file ({}) not found", adminFile);
        return false;
    }

    // If not for self check that vote is valid and not added
    auto nbAdmins = 0;
    auto nbVotes = 0;
    std::string repoPath = git_repository_workdir(repo.get());
    for (const auto& certificate : dhtnet::fileutils::readDirectory(repoPath + "admins")) {
        if (certificate.find(".crt") == std::string::npos) {
            JAMI_WARNING("Incorrect file found: {}", certificate);
            continue;
        }
        nbAdmins += 1;
        auto adminUri = certificate.substr(0, certificate.size() - std::string(".crt").size());
        if (std::find(voters.begin(), voters.end(), adminUri) != voters.end()) {
            nbVotes += 1;
        }
    }

    if (nbAdmins == 0 or (static_cast<double>(nbVotes) / static_cast<double>(nbAdmins)) < .5) {
        JAMI_ERROR("Incomplete vote detected (commit: {})", commitId);
        return false;
    }

    // If not for self check that member or device certificate is moved to banned/
    return !bannedFiles.empty();
}

bool
ConversationRepository::Impl::checkValidProfileUpdate(const std::string& userDevice,
                                                      const std::string& commitId,
                                                      const std::string& parentId) const
{
    auto userUri = uriFromDevice(userDevice);
    auto valid = false;
    {
        std::lock_guard<std::mutex> lk(membersMtx_);
        for (const auto& member : members_) {
            if (member.uri == userUri) {
                valid = member.role <= updateProfilePermLvl_;
                break;
            }
        }
    }
    if (!valid) {
        JAMI_ERROR("Profile changed from unauthorized user: {}", userDevice);
        return false;
    }

    // Retrieve tree for recent commit
    auto repo = repository();
    if (!repo)
        return false;
    auto treeNew = treeAtCommit(repo.get(), commitId);
    auto treeOld = treeAtCommit(repo.get(), parentId);
    if (not treeNew or not treeOld)
        return false;

    auto changedFiles = ConversationRepository::changedFiles(diffStats(commitId, parentId));
    // Check that no weird file is added nor removed
    for (const auto& f : changedFiles) {
        if (f == "profile.vcf") {
            // Ignore
        } else {
            JAMI_ERROR("Unwanted changed file detected: {}", f);
            return false;
        }
    }
    return true;
}

bool
ConversationRepository::Impl::isValidUserAtCommit(const std::string& userDevice,
                                                  const std::string& commitId) const
{
    auto acc = account_.lock();
    if (!acc)
        return false;
    auto cert = acc->certStore().getCertificate(userDevice);
    auto hasPinnedCert = cert and cert->issuer;
    auto repo = repository();
    if (not repo)
        return false;

    // Retrieve tree for commit
    auto tree = treeAtCommit(repo.get(), commitId);
    if (not tree)
        return false;

    // Check that /devices/userDevice.crt exists
    std::string deviceFile = fmt::format("devices/{}.crt", userDevice);
    auto blob_device = fileAtTree(deviceFile, tree);
    if (!blob_device) {
        JAMI_ERROR("{} announced but not found", deviceFile);
        return false;
    }
    auto* blob = reinterpret_cast<git_blob*>(blob_device.get());
    auto deviceCert = dht::crypto::Certificate(static_cast<const uint8_t*>(
                                                   git_blob_rawcontent(blob)),
                                               git_blob_rawsize(blob));
    auto userUri = deviceCert.getIssuerUID();
    if (userUri.empty()) {
        JAMI_ERROR("{} got no issuer UID", deviceFile);
        if (not hasPinnedCert) {
            return false;
        } else {
            // HACK: JAMS device's certificate does not contains any issuer
            // So, getIssuerUID() will be empty here, so there is no way
            // to get the userURI from this certificate.
            // Uses pinned certificate if one.
            userUri = cert->issuer->getId().toString();
        }
    }

    // Check that /(members|admins)/userUri.crt exists
    auto blob_parent = memberCertificate(userUri, tree);
    if (not blob_parent) {
        JAMI_ERROR("Certificate not found for {}", userUri);
        return false;
    }

    // Check that certificates were still valid
    blob = reinterpret_cast<git_blob*>(blob_parent.get());
    auto parentCert = dht::crypto::Certificate(static_cast<const uint8_t*>(
                                                   git_blob_rawcontent(blob)),
                                               git_blob_rawsize(blob));

    git_oid oid;
    git_commit* commit_ptr = nullptr;
    if (git_oid_fromstr(&oid, commitId.c_str()) < 0
        || git_commit_lookup(&commit_ptr, repo.get(), &oid) < 0) {
        JAMI_WARNING("Failed to look up commit {}", commitId);
        return false;
    }
    GitCommit commit = {commit_ptr, git_commit_free};

    auto commitTime = std::chrono::system_clock::from_time_t(git_commit_time(commit.get()));
    if (deviceCert.getExpiration() < commitTime) {
        JAMI_ERROR("Certificate {} expired", deviceCert.getId().toString());
        return false;
    }
    if (parentCert.getExpiration() < commitTime) {
        JAMI_ERROR("Certificate {} expired", parentCert.getId().toString());
        return false;
    }

    auto res = parentCert.getId().toString() == userUri;
    if (res && not hasPinnedCert) {
        acc->certStore().pinCertificate(std::move(deviceCert));
        acc->certStore().pinCertificate(std::move(parentCert));
    }
    return res;
}

bool
ConversationRepository::Impl::checkInitialCommit(const std::string& userDevice,
                                                 const std::string& commitId,
                                                 const std::string& commitMsg) const
{
    auto account = account_.lock();
    auto repo = repository();
    if (not account or not repo) {
        JAMI_WARNING("Invalid repository detected");
        return false;
    }
    auto userUri = uriFromDevice(userDevice);
    auto changedFiles = ConversationRepository::changedFiles(diffStats(commitId, ""));
    // NOTE: libgit2 return a diff with /, not DIR_SEPARATOR_DIR

    try {
        mode();
    } catch (...) {
        JAMI_ERROR("Invalid mode detected for commit: {}", commitId);
        return false;
    }

    std::string invited = {};
    if (mode_ == ConversationMode::ONE_TO_ONE) {
        std::string err;
        Json::Value cm;
        Json::CharReaderBuilder rbuilder;
        auto reader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
        if (reader->parse(commitMsg.data(), commitMsg.data() + commitMsg.size(), &cm, &err)) {
            invited = cm["invited"].asString();
        } else {
            JAMI_WARNING("{}", err);
        }
    }

    auto hasDevice = false, hasAdmin = false;
    std::string adminsFile = fmt::format("admins/{}.crt", userUri);
    std::string deviceFile = fmt::format("devices/{}.crt", userDevice);
    std::string crlFile = fmt::format("CRLs/{}", userUri);
    std::string invitedFile = fmt::format("invited/{}", invited);

    git_oid oid;
    git_commit* commit = nullptr;
    if (git_oid_fromstr(&oid, commitId.c_str()) < 0
        || git_commit_lookup(&commit, repo.get(), &oid) < 0) {
        JAMI_WARNING("Failed to look up commit {}", commitId);
        return false;
    }
    GitCommit gc = {commit, git_commit_free};
    git_tree* tNew = nullptr;
    if (git_commit_tree(&tNew, gc.get()) < 0) {
        JAMI_ERROR("Could not look up initial tree");
        return false;
    }
    GitTree treeNew = {tNew, git_tree_free};

    // Check that admin cert is added
    // Check that device cert is added
    // Check CRLs added
    // Check that no other file is added
    // Check if invited file present for one to one.
    for (const auto& changedFile : changedFiles) {
        if (changedFile == adminsFile) {
            hasAdmin = true;
            auto newFile = fileAtTree(changedFile, treeNew);
            if (!verifyCertificate(as_view(newFile), userUri)) {
                JAMI_ERROR("Invalid certificate found {}", changedFile);
                return false;
            }
        } else if (changedFile == deviceFile) {
            hasDevice = true;
            auto newFile = fileAtTree(changedFile, treeNew);
            if (!verifyCertificate(as_view(newFile), userUri)) {
                JAMI_ERROR("Invalid certificate found {}", changedFile);
                return false;
            }
        } else if (changedFile == crlFile || changedFile == invitedFile) {
            // Nothing to do
            continue;
        } else {
            // Invalid file detected
            JAMI_ERROR("Invalid add file detected: {} {}", changedFile, (int) *mode_);
            return false;
        }
    }

    return hasDevice && hasAdmin;
}

bool
ConversationRepository::Impl::validateDevice()
{
    auto repo = repository();
    auto account = account_.lock();
    if (!account || !repo) {
        JAMI_WARNING("Invalid repository detected");
        return false;
    }
    auto path = fmt::format("devices/{}.crt", account->currentDeviceId());
    std::string devicePath = git_repository_workdir(repo.get()) + path;
    if (!std::filesystem::is_regular_file(devicePath)) {
        JAMI_WARNING("Couldn't find file {}", devicePath);
        return false;
    }

    auto wrongDeviceFile = false;
    try {
        auto deviceCert = dht::crypto::Certificate(fileutils::loadFile(devicePath));
        wrongDeviceFile = !account->isValidAccountDevice(deviceCert);
    } catch (const std::exception&) {
        wrongDeviceFile = true;
    }
    if (wrongDeviceFile) {
        JAMI_WARNING("Device's certificate is not valid anymore. Trying to replace certificate with "
                "current one.");
        // Replace certificate with current cert
        auto cert = account->identity().second;
        if (!cert || !account->isValidAccountDevice(*cert)) {
            JAMI_ERROR("Current device's certificate is invalid. A migration is needed");
            return false;
        }
        auto file = fileutils::ofstream(devicePath, std::ios::trunc | std::ios::binary);
        if (!file.is_open()) {
            JAMI_ERROR("Could not write data to {}", devicePath);
            return false;
        }
        file << cert->toString(false);
        file.close();
        if (!add(path)) {
            JAMI_ERROR("Couldn't add file {}", devicePath);
            return false;
        }
    }

    // Check account cert (a new device can be added but account certifcate can be the old one!)
    auto adminPath = fmt::format("admins/{}.crt", account->getUsername());
    auto memberPath = fmt::format("members/{}.crt", account->getUsername());
    std::string parentPath = git_repository_workdir(repo.get());
    std::string relativeParentPath;
    if (std::filesystem::is_regular_file(parentPath + adminPath))
        relativeParentPath = adminPath;
    else if (std::filesystem::is_regular_file(parentPath + memberPath))
        relativeParentPath = memberPath;
    parentPath += relativeParentPath;
    if (relativeParentPath.empty()) {
        JAMI_ERROR("Invalid parent path (not in members or admins");
        return false;
    }
    wrongDeviceFile = false;
    try {
        auto parentCert = dht::crypto::Certificate(fileutils::loadFile(parentPath));
        wrongDeviceFile = !account->isValidAccountDevice(parentCert);
    } catch (const std::exception&) {
        wrongDeviceFile = true;
    }
    if (wrongDeviceFile) {
        JAMI_WARNING("Account's certificate is not valid anymore. Trying to replace certificate with "
                  "current one.");
        auto cert = account->identity().second;
        auto newCert = cert->issuer;
        if (newCert && std::filesystem::is_regular_file(parentPath)) {
            auto file = fileutils::ofstream(parentPath, std::ios::trunc | std::ios::binary);
            if (!file.is_open()) {
                JAMI_ERROR("Could not write data to {}", path);
                return false;
            }
            file << newCert->toString(true);
            file.close();
            if (!add(relativeParentPath)) {
                JAMI_WARNING("Couldn't add file {}", path);
                return false;
            }
        }
    }

    return true;
}

std::string
ConversationRepository::Impl::commit(const std::string& msg, bool verifyDevice)
{
    if (verifyDevice && !validateDevice())
        return {};
    GitSignature sig = signature();
    if (!sig)
        return {};
    auto account = account_.lock();

    // Retrieve current index
    git_index* index_ptr = nullptr;
    auto repo = repository();
    if (!repo)
        return {};
    if (git_repository_index(&index_ptr, repo.get()) < 0) {
        JAMI_ERROR("Could not open repository index");
        return {};
    }
    GitIndex index {index_ptr, git_index_free};

    git_oid tree_id;
    if (git_index_write_tree(&tree_id, index.get()) < 0) {
        JAMI_ERROR("Unable to write initial tree from index");
        return {};
    }

    git_tree* tree_ptr = nullptr;
    if (git_tree_lookup(&tree_ptr, repo.get(), &tree_id) < 0) {
        JAMI_ERROR("Could not look up initial tree");
        return {};
    }
    GitTree tree = {tree_ptr, git_tree_free};

    git_oid commit_id;
    if (git_reference_name_to_id(&commit_id, repo.get(), "HEAD") < 0) {
        JAMI_ERROR("Cannot get reference for HEAD");
        return {};
    }

    git_commit* head_ptr = nullptr;
    if (git_commit_lookup(&head_ptr, repo.get(), &commit_id) < 0) {
        JAMI_ERROR("Could not look up HEAD commit");
        return {};
    }
    GitCommit head_commit {head_ptr, git_commit_free};

    git_buf to_sign = {};
    const git_commit* head_ref[1] = {head_commit.get()};
    if (git_commit_create_buffer(&to_sign,
                                 repo.get(),
                                 sig.get(),
                                 sig.get(),
                                 nullptr,
                                 msg.c_str(),
                                 tree.get(),
                                 1,
                                 &head_ref[0])
        < 0) {
        JAMI_ERROR("Could not create commit buffer");
        return {};
    }

    // git commit -S
    auto to_sign_vec = std::vector<uint8_t>(to_sign.ptr, to_sign.ptr + to_sign.size);
    auto signed_buf = account->identity().first->sign(to_sign_vec);
    std::string signed_str = base64::encode(signed_buf);
    if (git_commit_create_with_signature(&commit_id,
                                         repo.get(),
                                         to_sign.ptr,
                                         signed_str.c_str(),
                                         "signature")
        < 0) {
        JAMI_ERROR("Could not sign commit");
        git_buf_dispose(&to_sign);
        return {};
    }
    git_buf_dispose(&to_sign);

    // Move commit to main branch
    git_reference* ref_ptr = nullptr;
    if (git_reference_create(&ref_ptr, repo.get(), "refs/heads/main", &commit_id, true, nullptr)
        < 0) {
        const git_error* err = giterr_last();
        if (err) {
            JAMI_ERROR("Could not move commit to main: {}", err->message);
            emitSignal<libjami::ConversationSignal::OnConversationError>(
                account->getAccountID(),
                id_,
                ECOMMIT,
                err->message);
        }
        return {};
    }
    git_reference_free(ref_ptr);

    auto commit_str = git_oid_tostr_s(&commit_id);
    if (commit_str) {
        JAMI_LOG("New message added with id: {}", commit_str);
    }
    return commit_str ? commit_str : "";
}

ConversationMode
ConversationRepository::Impl::mode() const
{
    // If already retrieven, return it, else get it from first commit
    if (mode_ != std::nullopt)
        return *mode_;

    LogOptions options;
    options.from = id_;
    options.nbOfCommits = 1;
    auto lastMsg = log(options);
    if (lastMsg.size() == 0) {
        if (auto shared = account_.lock()) {
            emitSignal<libjami::ConversationSignal::OnConversationError>(shared->getAccountID(),
                                                                         id_,
                                                                         EINVALIDMODE,
                                                                         "No initial commit");
        }
        throw std::logic_error("Can't retrieve first commit");
    }
    auto commitMsg = lastMsg[0].commit_msg;

    std::string err;
    Json::Value root;
    Json::CharReaderBuilder rbuilder;
    auto reader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
    if (!reader->parse(commitMsg.data(), commitMsg.data() + commitMsg.size(), &root, &err)) {
        if (auto shared = account_.lock()) {
            emitSignal<libjami::ConversationSignal::OnConversationError>(shared->getAccountID(),
                                                                         id_,
                                                                         EINVALIDMODE,
                                                                         "No initial commit");
        }
        throw std::logic_error("Can't retrieve first commit");
    }
    if (!root.isMember("mode")) {
        if (auto shared = account_.lock()) {
            emitSignal<libjami::ConversationSignal::OnConversationError>(shared->getAccountID(),
                                                                         id_,
                                                                         EINVALIDMODE,
                                                                         "No mode detected");
        }
        throw std::logic_error("No mode detected for initial commit");
    }
    int mode = root["mode"].asInt();

    switch (mode) {
    case 0:
        mode_ = ConversationMode::ONE_TO_ONE;
        break;
    case 1:
        mode_ = ConversationMode::ADMIN_INVITES_ONLY;
        break;
    case 2:
        mode_ = ConversationMode::INVITES_ONLY;
        break;
    case 3:
        mode_ = ConversationMode::PUBLIC;
        break;
    default:
        if (auto shared = account_.lock()) {
            emitSignal<libjami::ConversationSignal::OnConversationError>(shared->getAccountID(),
                                                                         id_,
                                                                         EINVALIDMODE,
                                                                         "Incorrect mode detected");
        }
        throw std::logic_error("Incorrect mode detected");
    }
    return *mode_;
}

std::string
ConversationRepository::Impl::diffStats(const std::string& newId, const std::string& oldId) const
{
    if (auto repo = repository()) {
        if (auto d = diff(repo.get(), newId, oldId))
            return diffStats(d);
    }
    return {};
}

GitDiff
ConversationRepository::Impl::diff(git_repository* repo,
                                   const std::string& idNew,
                                   const std::string& idOld) const
{
    if (!repo) {
        JAMI_ERROR("Cannot get reference for HEAD");
        return {nullptr, git_diff_free};
    }

    // Retrieve tree for commit new
    git_oid oid;
    git_commit* commitNew = nullptr;
    if (idNew == "HEAD") {
        if (git_reference_name_to_id(&oid, repo, "HEAD") < 0) {
            JAMI_ERROR("Cannot get reference for HEAD");
            return {nullptr, git_diff_free};
        }

        if (git_commit_lookup(&commitNew, repo, &oid) < 0) {
            JAMI_ERROR("Could not look up HEAD commit");
            return {nullptr, git_diff_free};
        }
    } else {
        if (git_oid_fromstr(&oid, idNew.c_str()) < 0
            || git_commit_lookup(&commitNew, repo, &oid) < 0) {
            GitCommit new_commit = {commitNew, git_commit_free};
            JAMI_WARNING("Failed to look up commit {}", idNew);
            return {nullptr, git_diff_free};
        }
    }
    GitCommit new_commit = {commitNew, git_commit_free};

    git_tree* tNew = nullptr;
    if (git_commit_tree(&tNew, new_commit.get()) < 0) {
        JAMI_ERROR("Could not look up initial tree");
        return {nullptr, git_diff_free};
    }
    GitTree treeNew = {tNew, git_tree_free};

    git_diff* diff_ptr = nullptr;
    if (idOld.empty()) {
        if (git_diff_tree_to_tree(&diff_ptr, repo, nullptr, treeNew.get(), {}) < 0) {
            JAMI_ERROR("Could not get diff to empty repository");
            return {nullptr, git_diff_free};
        }
        return {diff_ptr, git_diff_free};
    }

    // Retrieve tree for commit old
    git_commit* commitOld = nullptr;
    if (git_oid_fromstr(&oid, idOld.c_str()) < 0 || git_commit_lookup(&commitOld, repo, &oid) < 0) {
        JAMI_WARNING("Failed to look up commit {}", idOld);
        return {nullptr, git_diff_free};
    }
    GitCommit old_commit {commitOld, git_commit_free};

    git_tree* tOld = nullptr;
    if (git_commit_tree(&tOld, old_commit.get()) < 0) {
        JAMI_ERROR("Could not look up initial tree");
        return {nullptr, git_diff_free};
    }
    GitTree treeOld = {tOld, git_tree_free};

    // Calc diff
    if (git_diff_tree_to_tree(&diff_ptr, repo, treeOld.get(), treeNew.get(), {}) < 0) {
        JAMI_ERROR("Could not get diff between {} and {}", idOld, idNew);
        return {nullptr, git_diff_free};
    }
    return {diff_ptr, git_diff_free};
}

std::vector<ConversationCommit>
ConversationRepository::Impl::behind(const std::string& from) const
{
    git_oid oid_local, oid_head, oid_remote;
    auto repo = repository();
    if (!repo)
        return {};
    if (git_reference_name_to_id(&oid_local, repo.get(), "HEAD") < 0) {
        JAMI_ERROR("Cannot get reference for HEAD");
        return {};
    }
    oid_head = oid_local;
    std::string head = git_oid_tostr_s(&oid_head);
    if (git_oid_fromstr(&oid_remote, from.c_str()) < 0) {
        JAMI_ERROR("Cannot get reference for commit {}", from);
        return {};
    }

    git_oidarray bases;
    if (git_merge_bases(&bases, repo.get(), &oid_local, &oid_remote) != 0) {
        JAMI_ERROR("Cannot get any merge base for commit {} and {}", from, head);
        return {};
    }
    for (std::size_t i = 0; i < bases.count; ++i) {
        std::string oid = git_oid_tostr_s(&bases.ids[i]);
        if (oid != head) {
            oid_local = bases.ids[i];
            break;
        }
    }
    git_oidarray_free(&bases);
    std::string to = git_oid_tostr_s(&oid_local);
    if (to == from)
        return {};
    return log(LogOptions {from, to});
}

void
ConversationRepository::Impl::forEachCommit(PreConditionCb&& preCondition,
                                            std::function<void(ConversationCommit&&)>&& emplaceCb,
                                            PostConditionCb&& postCondition,
                                            const std::string& from,
                                            bool logIfNotFound) const
{
    git_oid oid, oidFrom, oidMerge;

    // Note: Start from head to get all merge possibilities and correct linearized parent.
    auto repo = repository();
    if (!repo or git_reference_name_to_id(&oid, repo.get(), "HEAD") < 0) {
        JAMI_ERROR("Cannot get reference for HEAD");
        return;
    }

    if (from != "" && git_oid_fromstr(&oidFrom, from.c_str()) == 0) {
        auto isMergeBase = git_merge_base(&oidMerge, repo.get(), &oid, &oidFrom) == 0
                           && git_oid_equal(&oidMerge, &oidFrom);
        if (!isMergeBase) {
            // We're logging a non merged branch, so, take this one instead of HEAD
            oid = oidFrom;
        }
    }

    git_revwalk* walker_ptr = nullptr;
    if (git_revwalk_new(&walker_ptr, repo.get()) < 0 || git_revwalk_push(walker_ptr, &oid) < 0) {
        GitRevWalker walker {walker_ptr, git_revwalk_free};
        // This fail can be ok in the case we check if a commit exists before pulling (so can fail
        // there). only log if the fail is unwanted.
        if (logIfNotFound)
            JAMI_DEBUG("Couldn't init revwalker for conversation {}", id_);
        return;
    }

    GitRevWalker walker {walker_ptr, git_revwalk_free};
    git_revwalk_sorting(walker.get(), GIT_SORT_TOPOLOGICAL | GIT_SORT_TIME);

    for (auto idx = 0u; !git_revwalk_next(&oid, walker.get()); ++idx) {
        git_commit* commit_ptr = nullptr;
        std::string id = git_oid_tostr_s(&oid);
        if (git_commit_lookup(&commit_ptr, repo.get(), &oid) < 0) {
            JAMI_WARNING("Failed to look up commit {}", id);
            break;
        }
        GitCommit commit {commit_ptr, git_commit_free};

        const git_signature* sig = git_commit_author(commit.get());
        GitAuthor author;
        author.name = sig->name;
        author.email = sig->email;

        std::vector<std::string> parents;
        auto parentsCount = git_commit_parentcount(commit.get());
        for (unsigned int p = 0; p < parentsCount; ++p) {
            std::string parent {};
            const git_oid* pid = git_commit_parent_id(commit.get(), p);
            if (pid) {
                parent = git_oid_tostr_s(pid);
                parents.emplace_back(parent);
            }
        }

        auto result = preCondition(id, author, commit);
        if (result == CallbackResult::Skip)
            continue;
        else if (result == CallbackResult::Break)
            break;

        ConversationCommit cc;
        cc.id = id;
        cc.commit_msg = git_commit_message(commit.get());
        cc.author = std::move(author);
        cc.parents = std::move(parents);
        git_buf signature = {}, signed_data = {};
        if (git_commit_extract_signature(&signature, &signed_data, repo.get(), &oid, "signature")
            < 0) {
            JAMI_WARNING("Could not extract signature for commit {}", id);
        } else {
            cc.signature = base64::decode(
                std::string(signature.ptr, signature.ptr + signature.size));
            cc.signed_content = std::vector<uint8_t>(signed_data.ptr,
                                                     signed_data.ptr + signed_data.size);
        }
        git_buf_dispose(&signature);
        git_buf_dispose(&signed_data);
        cc.timestamp = git_commit_time(commit.get());

        auto post = postCondition(id, author, cc);
        emplaceCb(std::move(cc));

        if (post)
            break;
    }
}

std::vector<ConversationCommit>
ConversationRepository::Impl::log(const LogOptions& options) const
{
    std::vector<ConversationCommit> commits {};
    auto startLogging = options.from == "";
    auto breakLogging = false;
    forEachCommit(
        [&](const auto& id, const auto& author, const auto& commit) {
            if (!commits.empty()) {
                // Set linearized parent
                commits.rbegin()->linearized_parent = id;
            }
            if (options.skipMerge && git_commit_parentcount(commit.get()) > 1) {
                return CallbackResult::Skip;
            }
            if ((options.nbOfCommits != 0 && commits.size() == options.nbOfCommits))
                return CallbackResult::Break; // Stop logging
            if (breakLogging)
                return CallbackResult::Break; // Stop logging
            if (id == options.to) {
                if (options.includeTo)
                    breakLogging = true; // For the next commit
                else
                    return CallbackResult::Break; // Stop logging
            }

            if (!startLogging && options.from != "" && options.from == id)
                startLogging = true;
            if (!startLogging)
                return CallbackResult::Skip; // Start logging after this one

            if (options.fastLog) {
                if (options.authorUri != "") {
                    if (options.authorUri == uriFromDevice(author.email)) {
                        return CallbackResult::Break; // Found author, stop
                    }
                }
                // Used to only count commit
                commits.emplace(commits.end(), ConversationCommit {});
                return CallbackResult::Skip;
            }

            return CallbackResult::Ok; // Continue
        },
        [&](auto&& cc) { commits.emplace(commits.end(), std::forward<decltype(cc)>(cc)); },
        [](auto, auto, auto) { return false; },
        options.from,
        options.logIfNotFound);
    return commits;
}

std::vector<std::map<std::string, std::string>>
ConversationRepository::Impl::search(const Filter& filter) const
{
    std::vector<std::map<std::string, std::string>> commits {};
    // std::regex_constants::ECMAScript is the default flag.
    auto re = std::regex(filter.regexSearch,
                         filter.caseSensitive ? std::regex_constants::ECMAScript
                                              : std::regex_constants::icase);
    forEachCommit(
        [&](const auto& id, const auto& author, auto& commit) {
            if (!commits.empty()) {
                // Set linearized parent
                commits.rbegin()->at("linearizedParent") = id;
            }

            if (!filter.author.empty() && filter.author != uriFromDevice(author.email)) {
                // Filter author
                return CallbackResult::Skip;
            }
            auto commitTime = git_commit_time(commit.get());
            if (filter.before && filter.before < commitTime) {
                // Only get commits before this date
                return CallbackResult::Skip;
            }
            if (filter.after && filter.after > commitTime) {
                // Only get commits before this date
                if (git_commit_parentcount(commit.get()) <= 1)
                    return CallbackResult::Break;
                else
                    return CallbackResult::Skip; // Because we are sorting it with
                                                 // GIT_SORT_TOPOLOGICAL | GIT_SORT_TIME
            }

            return CallbackResult::Ok; // Continue
        },
        [&](auto&& cc) {
            auto content = convCommitToMap(cc);
            auto contentType = content ? content->at("type") : "";
            auto isSearchable = contentType == "text/plain"
                                || contentType == "application/data-transfer+json";
            if (filter.type.empty() && !isSearchable) {
                // Not searchable, at least for now
                return;
            } else if (contentType == filter.type || filter.type.empty()) {
                if (isSearchable) {
                    // If it's a text match the body, else the display name
                    auto body = contentType == "text/plain" ? content->at("body")
                                                            : content->at("displayName");
                    std::smatch body_match;
                    if (std::regex_search(body, body_match, re)) {
                        commits.emplace(commits.end(), std::move(*content));
                    }
                } else {
                    // Matching type, just add it to the results
                    commits.emplace(commits.end(), std::move(*content));
                }
            }
        },
        [&](auto id, auto, auto) {
            if (filter.maxResult != 0 && commits.size() == filter.maxResult)
                return true;
            if (id == filter.lastId)
                return true;
            return false;
        });
    return commits;
}

GitObject
ConversationRepository::Impl::fileAtTree(const std::string& path, const GitTree& tree) const
{
    git_object* blob_ptr = nullptr;
    if (git_object_lookup_bypath(&blob_ptr,
                                 reinterpret_cast<git_object*>(tree.get()),
                                 path.c_str(),
                                 GIT_OBJECT_BLOB)
        != 0) {
        return GitObject {nullptr, git_object_free};
    }
    return GitObject {blob_ptr, git_object_free};
}

GitObject
ConversationRepository::Impl::memberCertificate(std::string_view memberUri,
                                                const GitTree& tree) const
{
    auto blob = fileAtTree(fmt::format("members/{}.crt", memberUri), tree);
    if (not blob)
        blob = fileAtTree(fmt::format("admins/{}.crt", memberUri), tree);
    return blob;
}

GitTree
ConversationRepository::Impl::treeAtCommit(git_repository* repo, const std::string& commitId) const
{
    git_oid oid;
    git_commit* commit = nullptr;
    if (git_oid_fromstr(&oid, commitId.c_str()) < 0 || git_commit_lookup(&commit, repo, &oid) < 0) {
        JAMI_WARNING("Failed to look up commit {}", commitId);
        return GitTree {nullptr, git_tree_free};
    }
    GitCommit gc = {commit, git_commit_free};
    git_tree* tree = nullptr;
    if (git_commit_tree(&tree, gc.get()) < 0) {
        JAMI_ERROR("Could not look up initial tree");
        return GitTree {nullptr, git_tree_free};
    }
    return GitTree {tree, git_tree_free};
}

std::string
ConversationRepository::Impl::getCommitType(const std::string& commitMsg) const
{
    std::string type = {};
    std::string err;
    Json::Value cm;
    Json::CharReaderBuilder rbuilder;
    auto reader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
    if (reader->parse(commitMsg.data(), commitMsg.data() + commitMsg.size(), &cm, &err)) {
        type = cm["type"].asString();
    } else {
        JAMI_WARNING("{}", err);
    }
    return type;
}

std::vector<std::string>
ConversationRepository::Impl::getInitialMembers() const
{
    auto acc = account_.lock();
    if (!acc)
        return {};
    LogOptions options;
    options.from = id_;
    options.nbOfCommits = 1;
    auto firstCommit = log(options);
    if (firstCommit.size() == 0) {
        return {};
    }
    auto commit = firstCommit[0];

    auto authorDevice = commit.author.email;
    auto cert = acc->certStore().getCertificate(authorDevice);
    if (!cert || !cert->issuer)
        return {};
    auto authorId = cert->issuer->getId().toString();
    if (mode() == ConversationMode::ONE_TO_ONE) {
        std::string err;
        Json::Value root;
        Json::CharReaderBuilder rbuilder;
        auto reader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
        if (!reader->parse(commit.commit_msg.data(),
                           commit.commit_msg.data() + commit.commit_msg.size(),
                           &root,
                           &err)) {
            return {authorId};
        }
        if (root.isMember("invited") && root["invited"].asString() != authorId)
            return {authorId, root["invited"].asString()};
    }
    return {authorId};
}

bool
ConversationRepository::Impl::resolveConflicts(git_index* index, const std::string& other_id)
{
    git_index_conflict_iterator* conflict_iterator = nullptr;
    const git_index_entry* ancestor_out = nullptr;
    const git_index_entry* our_out = nullptr;
    const git_index_entry* their_out = nullptr;

    git_index_conflict_iterator_new(&conflict_iterator, index);
    GitIndexConflictIterator ci {conflict_iterator, git_index_conflict_iterator_free};

    git_oid head_commit_id;
    auto repo = repository();
    if (!repo || git_reference_name_to_id(&head_commit_id, repo.get(), "HEAD") < 0) {
        JAMI_ERROR("Cannot get reference for HEAD");
        return false;
    }
    auto commit_str = git_oid_tostr_s(&head_commit_id);
    if (!commit_str)
        return false;
    auto useRemote = (other_id > commit_str); // Choose by commit version

    // NOTE: for now, only authorize conflicts on "profile.vcf"
    std::vector<git_index_entry> new_entries;
    while (git_index_conflict_next(&ancestor_out, &our_out, &their_out, ci.get()) != GIT_ITEROVER) {
        if (ancestor_out && ancestor_out->path && our_out && our_out->path && their_out
            && their_out->path) {
            if (std::string(ancestor_out->path) == "profile.vcf") {
                // Checkout wanted version. copy the index_entry
                git_index_entry resolution = useRemote ? *their_out : *our_out;
                resolution.flags &= GIT_INDEX_STAGE_NORMAL;
                if (!(resolution.flags & GIT_IDXENTRY_VALID))
                    resolution.flags |= GIT_IDXENTRY_VALID;
                // NOTE: do no git_index_add yet, wait for after full conflict checks
                new_entries.push_back(resolution);
                continue;
            }
            JAMI_ERROR("Conflict detected on a file that is not authorized: {}", ancestor_out->path);
            return false;
        }
        return false;
    }

    for (auto& entry : new_entries)
        git_index_add(index, &entry);
    git_index_conflict_cleanup(index);

    // Checkout and cleanup
    git_checkout_options opt;
    git_checkout_options_init(&opt, GIT_CHECKOUT_OPTIONS_VERSION);
    opt.checkout_strategy |= GIT_CHECKOUT_FORCE;
    opt.checkout_strategy |= GIT_CHECKOUT_ALLOW_CONFLICTS;
    if (other_id > commit_str)
        opt.checkout_strategy |= GIT_CHECKOUT_USE_THEIRS;
    else
        opt.checkout_strategy |= GIT_CHECKOUT_USE_OURS;

    if (git_checkout_index(repo.get(), index, &opt) < 0) {
        const git_error* err = giterr_last();
        if (err)
            JAMI_ERROR("Cannot checkout index: {}", err->message);
        return false;
    }

    return true;
}

void
ConversationRepository::Impl::initMembers()
{
    auto repo = repository();
    if (!repo)
        throw std::logic_error("Invalid git repository");

    std::vector<std::string> uris;
    std::lock_guard<std::mutex> lk(membersMtx_);
    members_.clear();
    std::string repoPath = git_repository_workdir(repo.get());
    std::vector<std::string> paths = {repoPath + "/" + "admins",
                                      repoPath + "/" + "members",
                                      repoPath + "/" + "invited",
                                      repoPath + "/" + "banned" + "/" + "members",
                                      repoPath + "/" + "banned" + "/" + "invited"};
    std::vector<MemberRole> roles = {
        MemberRole::ADMIN,
        MemberRole::MEMBER,
        MemberRole::INVITED,
        MemberRole::BANNED,
        MemberRole::BANNED,
    };

    auto i = 0;
    for (const auto& p : paths) {
        for (const auto& f : dhtnet::fileutils::readDirectory(p)) {
            auto pos = f.find(".crt");
            auto uri = f.substr(0, pos);
            auto it = std::find(uris.begin(), uris.end(), uri);
            if (it == uris.end()) {
                members_.emplace_back(ConversationMember {uri, roles[i]});
                uris.emplace_back(uri);
            }
        }
        ++i;
    }

    if (mode() == ConversationMode::ONE_TO_ONE) {
        for (const auto& member : getInitialMembers()) {
            auto it = std::find(uris.begin(), uris.end(), member);
            if (it == uris.end()) {
                // If member is in initial commit, but not in invited, this means that user left.
                members_.emplace_back(ConversationMember {member, MemberRole::LEFT});
            }
        }
    }
}

std::optional<std::map<std::string, std::string>>
ConversationRepository::Impl::convCommitToMap(const ConversationCommit& commit) const
{
    auto authorId = uriFromDevice(commit.author.email);
    if (authorId.empty())
        return std::nullopt;
    std::string parents;
    auto parentsSize = commit.parents.size();
    for (std::size_t i = 0; i < parentsSize; ++i) {
        parents += commit.parents[i];
        if (i != parentsSize - 1)
            parents += ",";
    }
    std::string type {};
    if (parentsSize > 1)
        type = "merge";
    std::string body {};
    std::map<std::string, std::string> message;
    if (type.empty()) {
        std::string err;
        Json::Value cm;
        Json::CharReaderBuilder rbuilder;
        auto reader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
        if (reader->parse(commit.commit_msg.data(),
                          commit.commit_msg.data() + commit.commit_msg.size(),
                          &cm,
                          &err)) {
            for (auto const& id : cm.getMemberNames()) {
                if (id == "type") {
                    type = cm[id].asString();
                    continue;
                }
                message.insert({id, cm[id].asString()});
            }
        } else {
            JAMI_WARNING("{}", err);
        }
    }
    if (type.empty()) {
        return std::nullopt;
    } else if (type == "application/data-transfer+json") {
        // Avoid the client to do the concatenation
        message["fileId"] = commit.id + "_" + message["tid"];
        auto extension = fileutils::getFileExtension(message["displayName"]);
        if (!extension.empty())
            message["fileId"] += "." + extension;
    }
    message["id"] = commit.id;
    message["parents"] = parents;
    message["linearizedParent"] = commit.linearized_parent;
    message["author"] = authorId;
    message["type"] = type;
    message["timestamp"] = std::to_string(commit.timestamp);

    return message;
}

std::string
ConversationRepository::Impl::diffStats(const GitDiff& diff) const
{
    git_diff_stats* stats_ptr = nullptr;
    if (git_diff_get_stats(&stats_ptr, diff.get()) < 0) {
        JAMI_ERROR("Could not get diff stats");
        return {};
    }
    GitDiffStats stats = {stats_ptr, git_diff_stats_free};

    git_diff_stats_format_t format = GIT_DIFF_STATS_FULL;
    git_buf statsBuf = {};
    if (git_diff_stats_to_buf(&statsBuf, stats.get(), format, 80) < 0) {
        JAMI_ERROR("Could not format diff stats");
        return {};
    }

    auto res = std::string(statsBuf.ptr, statsBuf.ptr + statsBuf.size);
    git_buf_dispose(&statsBuf);
    return res;
}

//////////////////////////////////

std::unique_ptr<ConversationRepository>
ConversationRepository::createConversation(const std::shared_ptr<JamiAccount>& account,
                                           ConversationMode mode,
                                           const std::string& otherMember)
{
    // Create temporary directory because we can't know the first hash for now
    std::uniform_int_distribution<uint64_t> dist {0, std::numeric_limits<uint64_t>::max()};
    random_device rdev;
    auto conversationsPath = fileutils::get_data_dir() / account->getAccountID() / "conversations";
    dhtnet::fileutils::check_dir(conversationsPath);
    auto tmpPath = conversationsPath / std::to_string(dist(rdev));
    if (std::filesystem::is_directory(tmpPath)) {
        JAMI_ERROR("{} already exists. Abort create conversations", tmpPath);
        return {};
    }
    if (!dhtnet::fileutils::recursive_mkdir(tmpPath, 0700)) {
        JAMI_ERROR("Error when creating {}. Abort create conversations", tmpPath);
        return {};
    }
    auto repo = create_empty_repository(tmpPath);
    if (!repo) {
        return {};
    }

    // Add initial files
    if (!add_initial_files(repo, account, mode, otherMember)) {
        JAMI_ERROR("Error when adding initial files");
        dhtnet::fileutils::removeAll(tmpPath, true);
        return {};
    }

    // Commit changes
    auto id = initial_commit(repo, account, mode, otherMember);
    if (id.empty()) {
        JAMI_ERROR("Couldn't create initial commit in {}", tmpPath);
        dhtnet::fileutils::removeAll(tmpPath, true);
        return {};
    }

    // Move to wanted directory
    auto newPath = conversationsPath / id;
    if (std::rename(tmpPath.c_str(), newPath.c_str())) {
        JAMI_ERROR("Couldn't move {} in {}", tmpPath, newPath);
        dhtnet::fileutils::removeAll(tmpPath, true);
        return {};
    }

    JAMI_LOG("New conversation initialized in {}", newPath);

    return std::make_unique<ConversationRepository>(account, id);
}

std::unique_ptr<ConversationRepository>
ConversationRepository::cloneConversation(const std::shared_ptr<JamiAccount>& account,
                                          const std::string& deviceId,
                                          const std::string& conversationId)
{
    auto conversationsPath = fileutils::get_data_dir() / account->getAccountID() / "conversations";
    dhtnet::fileutils::check_dir(conversationsPath);
    auto path = conversationsPath / conversationId;
    auto url = fmt::format("git://{}/{}", deviceId, conversationId);

    git_clone_options clone_options;
    git_clone_options_init(&clone_options, GIT_CLONE_OPTIONS_VERSION);
    git_fetch_options_init(&clone_options.fetch_opts, GIT_FETCH_OPTIONS_VERSION);
    clone_options.fetch_opts.callbacks.transfer_progress = [](const git_indexer_progress* stats,
                                                              void*) {
        // Uncomment to get advancment
        // if (stats->received_objects % 500 == 0 || stats->received_objects == stats->total_objects)
        //     JAMI_DEBUG("{}/{} {}kb", stats->received_objects, stats->total_objects,
        //     stats->received_bytes/1024);
        // If a pack is more than 256Mb, it's anormal.
        if (stats->received_bytes > MAX_FETCH_SIZE) {
            JAMI_ERROR("Abort fetching repository, the fetch is too big: {} bytes ({}/{})",
                       stats->received_bytes,
                       stats->received_objects,
                       stats->total_objects);
            return -1;
        }
        return 0;
    };

    if (std::filesystem::is_directory(path)) {
        // If a crash occurs during a previous clone, just in case
        JAMI_WARNING("Removing existing directory {} (the dir exists and non empty)", path);
        if (dhtnet::fileutils::removeAll(path, true) != 0)
            return nullptr;
    }

    JAMI_DEBUG("Start clone of {:s} to {}", url, path);
    git_repository* rep = nullptr;
    if (auto err = git_clone(&rep, url.c_str(), path.c_str(), nullptr)) {
        if (const git_error* gerr = giterr_last())
            JAMI_ERROR("Error when retrieving remote conversation: {:s} {}", gerr->message, path);
        else
            JAMI_ERROR("Unknown error {:d} when retrieving remote conversation", err);
        return nullptr;
    }
    git_repository_free(rep);
    auto repo = std::make_unique<ConversationRepository>(account, conversationId);
    repo->pinCertificates(true); // need to load certificates to validate non known members
    if (!repo->validClone()) {
        repo->erase();
        JAMI_ERROR("Error when validating remote conversation");
        return nullptr;
    }
    JAMI_LOG("New conversation cloned in {}", path);
    return repo;
}

bool
ConversationRepository::Impl::validCommits(
    const std::vector<ConversationCommit>& commitsToValidate) const
{
    for (const auto& commit : commitsToValidate) {
        auto userDevice = commit.author.email;
        auto validUserAtCommit = commit.id;
        if (commit.parents.size() == 0) {
            if (!checkInitialCommit(userDevice, commit.id, commit.commit_msg)) {
                JAMI_WARNING("Malformed initial commit {}. Please check you use the latest "
                          "version of Jami, or that your contact is not doing unwanted stuff.",
                          commit.id);
                if (auto shared = account_.lock()) {
                    emitSignal<libjami::ConversationSignal::OnConversationError>(
                        shared->getAccountID(), id_, EVALIDFETCH, "Malformed initial commit");
                }
                return false;
            }
        } else if (commit.parents.size() == 1) {
            auto type = getCommitType(commit.commit_msg);
            if (type == "vote") {
                // Check that vote is valid
                if (!checkVote(userDevice, commit.id, commit.parents[0])) {
                    JAMI_WARNING("Malformed vote commit {}. Please check you use the latest version "
                              "of Jami, or that your contact is not doing unwanted stuff.",
                              commit.id.c_str());
                    if (auto shared = account_.lock()) {
                        emitSignal<libjami::ConversationSignal::OnConversationError>(
                            shared->getAccountID(), id_, EVALIDFETCH, "Malformed vote");
                    }
                    return false;
                }
            } else if (type == "member") {
                std::string err;
                Json::Value root;
                Json::CharReaderBuilder rbuilder;
                auto reader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
                if (!reader->parse(commit.commit_msg.data(),
                                   commit.commit_msg.data() + commit.commit_msg.size(),
                                   &root,
                                   &err)) {
                    JAMI_ERROR("Failed to parse: {}", err);
                    if (auto shared = account_.lock()) {
                        emitSignal<libjami::ConversationSignal::OnConversationError>(
                            shared->getAccountID(), id_, EVALIDFETCH, "Malformed member commit");
                    }
                    return false;
                }
                std::string action = root["action"].asString();
                std::string uriMember = root["uri"].asString();
                if (action == "add") {
                    if (!checkValidAdd(userDevice, uriMember, commit.id, commit.parents[0])) {
                        JAMI_WARNING(
                            "Malformed add commit {}. Please check you use the latest version "
                            "of Jami, or that your contact is not doing unwanted stuff.",
                            commit.id);
                        if (auto shared = account_.lock()) {
                            emitSignal<libjami::ConversationSignal::OnConversationError>(
                                shared->getAccountID(),
                                id_,
                                EVALIDFETCH,
                                "Malformed add member commit");
                        }
                        return false;
                    }
                } else if (action == "join") {
                    if (!checkValidJoins(userDevice, uriMember, commit.id, commit.parents[0])) {
                        JAMI_WARNING(
                            "Malformed joins commit {}. Please check you use the latest version "
                            "of Jami, or that your contact is not doing unwanted stuff.",
                            commit.id);
                        if (auto shared = account_.lock()) {
                            emitSignal<libjami::ConversationSignal::OnConversationError>(
                                shared->getAccountID(),
                                id_,
                                EVALIDFETCH,
                                "Malformed join member commit");
                        }
                        return false;
                    }
                } else if (action == "remove") {
                    // In this case, we remove the user. So if self, the user will not be
                    // valid for this commit. Check previous commit
                    validUserAtCommit = commit.parents[0];
                    if (!checkValidRemove(userDevice, uriMember, commit.id, commit.parents[0])) {
                        JAMI_WARNING(
                            "Malformed removes commit {}. Please check you use the latest version "
                            "of Jami, or that your contact is not doing unwanted stuff.",
                            commit.id);
                        if (auto shared = account_.lock()) {
                            emitSignal<libjami::ConversationSignal::OnConversationError>(
                                shared->getAccountID(),
                                id_,
                                EVALIDFETCH,
                                "Malformed remove member commit");
                        }
                        return false;
                    }
                } else if (action == "ban" || action == "unban") {
                    // Note device.size() == "member".size()
                    if (!checkValidVoteResolution(userDevice,
                                                  uriMember,
                                                  commit.id,
                                                  commit.parents[0],
                                                  action)) {
                        JAMI_WARNING(
                            "Malformed removes commit {}. Please check you use the latest version "
                            "of Jami, or that your contact is not doing unwanted stuff.",
                            commit.id);
                        if (auto shared = account_.lock()) {
                            emitSignal<libjami::ConversationSignal::OnConversationError>(
                                shared->getAccountID(),
                                id_,
                                EVALIDFETCH,
                                "Malformed ban member commit");
                        }
                        return false;
                    }
                } else {
                    JAMI_WARNING("Malformed member commit {} with action {}. Please check you use the "
                              "latest "
                              "version of Jami, or that your contact is not doing unwanted stuff.",
                              commit.id,
                              action);
                    if (auto shared = account_.lock()) {
                        emitSignal<libjami::ConversationSignal::OnConversationError>(
                            shared->getAccountID(), id_, EVALIDFETCH, "Malformed member commit");
                    }
                    return false;
                }
            } else if (type == "application/update-profile") {
                if (!checkValidProfileUpdate(userDevice, commit.id, commit.parents[0])) {
                    JAMI_WARNING("Malformed profile updates commit {}. Please check you use the "
                              "latest version "
                              "of Jami, or that your contact is not doing unwanted stuff.",
                              commit.id);
                    if (auto shared = account_.lock()) {
                        emitSignal<libjami::ConversationSignal::OnConversationError>(
                            shared->getAccountID(),
                            id_,
                            EVALIDFETCH,
                            "Malformed profile updates commit");
                    }
                    return false;
                }
            } else if (type == "application/edited-message") {
                if (!checkEdit(userDevice, commit)) {
                    JAMI_ERROR("Commit {:s} malformed", commit.id);
                    if (auto shared = account_.lock()) {
                        emitSignal<libjami::ConversationSignal::OnConversationError>(
                            shared->getAccountID(), id_, EVALIDFETCH, "Malformed edit commit");
                    }
                    return false;
                }
            } else {
                // Note: accept all mimetype here, as we can have new mimetypes
                // Just avoid to add weird files
                // Check that no weird file is added outside device cert nor removed
                if (!checkValidUserDiff(userDevice, commit.id, commit.parents[0])) {
                    JAMI_WARNING("Malformed {} commit {}. Please check you use the latest "
                              "version of Jami, or that your contact is not doing unwanted stuff.",
                              type,
                              commit.id);
                    if (auto shared = account_.lock()) {
                        emitSignal<libjami::ConversationSignal::OnConversationError>(
                            shared->getAccountID(), id_, EVALIDFETCH, "Malformed commit");
                    }
                    return false;
                }
            }

            // For all commit, check that user is valid,
            // So that user certificate MUST be in /members or /admins
            // and device cert MUST be in /devices
            if (!isValidUserAtCommit(userDevice, validUserAtCommit)) {
                JAMI_WARNING(
                    "Malformed commit {}. Please check you use the latest version of Jami, or "
                    "that your contact is not doing unwanted stuff. {}",
                    validUserAtCommit,
                    commit.commit_msg);
                if (auto shared = account_.lock()) {
                    emitSignal<libjami::ConversationSignal::OnConversationError>(
                        shared->getAccountID(), id_, EVALIDFETCH, "Malformed commit");
                }
                return false;
            }
        } else {
            // Merge commit, for now, check user
            if (!isValidUserAtCommit(userDevice, validUserAtCommit)) {
                JAMI_WARNING("Malformed merge commit {}. Please check you use the latest version of "
                          "Jami, or "
                          "that your contact is not doing unwanted stuff.",
                          validUserAtCommit);
                if (auto shared = account_.lock()) {
                    emitSignal<libjami::ConversationSignal::OnConversationError>(
                        shared->getAccountID(), id_, EVALIDFETCH, "Malformed commit");
                }
                return false;
            }
        }
        JAMI_DEBUG("Validate commit {}", commit.id);
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////////

ConversationRepository::ConversationRepository(const std::weak_ptr<JamiAccount>& account,
                                               const std::string& id)
    : pimpl_ {new Impl {account, id}}
{}

ConversationRepository::~ConversationRepository() = default;

const std::string&
ConversationRepository::id() const
{
    return pimpl_->id_;
}

std::string
ConversationRepository::addMember(const std::string& uri)
{
    auto account = pimpl_->account_.lock();
    auto repo = pimpl_->repository();
    if (!account or not repo)
        return {};
    auto deviceId = account->currentDeviceId();
    auto name = account->getUsername();
    if (name.empty())
        name = deviceId;

    // First, we need to add the member file to the repository if not present
    std::filesystem::path repoPath = git_repository_workdir(repo.get());

    std::filesystem::path invitedPath = repoPath / "invited";
    if (!dhtnet::fileutils::recursive_mkdir(invitedPath, 0700)) {
        JAMI_ERROR("Error when creating {}.", invitedPath);
        return {};
    }
    std::filesystem::path devicePath = invitedPath / uri;
    if (std::filesystem::is_regular_file(devicePath)) {
        JAMI_WARNING("Member {} already present!", uri);
        return {};
    }

    auto file = fileutils::ofstream(devicePath, std::ios::trunc | std::ios::binary);
    if (!file.is_open()) {
        JAMI_ERROR("Could not write data to {}", devicePath);
        return {};
    }
    std::string path = "invited/" + uri;
    if (!pimpl_->add(path))
        return {};

    {
        std::lock_guard<std::mutex> lk(pimpl_->membersMtx_);
        pimpl_->members_.emplace_back(ConversationMember {uri, MemberRole::INVITED});
    }

    Json::Value json;
    json["action"] = "add";
    json["uri"] = uri;
    json["type"] = "member";
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";
    return pimpl_->commit(Json::writeString(wbuilder, json));
}

std::string
ConversationRepository::amend(const std::string& id, const std::string& msg)
{
    GitSignature sig = pimpl_->signature();
    if (!sig)
        return {};
    auto account = pimpl_->account_.lock();

    git_oid tree_id, commit_id;
    git_commit* commit_ptr = nullptr;
    auto repo = pimpl_->repository();
    if (!repo || git_oid_fromstr(&tree_id, id.c_str()) < 0
        || git_commit_lookup(&commit_ptr, repo.get(), &tree_id) < 0) {
        GitCommit commit {commit_ptr, git_commit_free};
        JAMI_WARNING("Failed to look up commit {}", id);
        return {};
    }
    GitCommit commit {commit_ptr, git_commit_free};

    if (git_commit_amend(
            &commit_id, commit.get(), nullptr, sig.get(), sig.get(), nullptr, msg.c_str(), nullptr)
        < 0) {
        const git_error* err = giterr_last();
        if (err)
            JAMI_ERROR("Could not amend commit: {}", err->message);
        return {};
    }

    // Move commit to main branch
    git_reference* ref_ptr = nullptr;
    if (git_reference_create(&ref_ptr, repo.get(), "refs/heads/main", &commit_id, true, nullptr)
        < 0) {
        const git_error* err = giterr_last();
        if (err) {
            JAMI_ERROR("Could not move commit to main: {}", err->message);
            emitSignal<libjami::ConversationSignal::OnConversationError>(
                account->getAccountID(),
                pimpl_->id_,
                ECOMMIT,
                err->message);
        }
        return {};
    }
    git_reference_free(ref_ptr);

    auto commit_str = git_oid_tostr_s(&commit_id);
    if (commit_str) {
        JAMI_DEBUG("Commit {} amended (new id: {})", id, commit_str);
        return commit_str;
    }
    return {};
}

bool
ConversationRepository::fetch(const std::string& remoteDeviceId)
{
    // Fetch distant repository
    git_remote* remote_ptr = nullptr;
    git_fetch_options fetch_opts;
    git_fetch_options_init(&fetch_opts, GIT_FETCH_OPTIONS_VERSION);

    LogOptions options;
    options.nbOfCommits = 1;
    auto lastMsg = log(options);
    if (lastMsg.size() == 0)
        return false;
    auto lastCommit = lastMsg[0].id;

    // Assert that repository exists
    auto repo = pimpl_->repository();
    if (!repo)
        return false;
    auto res = git_remote_lookup(&remote_ptr, repo.get(), remoteDeviceId.c_str());
    if (res != 0) {
        if (res != GIT_ENOTFOUND) {
            JAMI_ERROR("Couldn't lookup for remote {}", remoteDeviceId);
            return false;
        }
        std::string channelName = fmt::format("git://{}/{}", remoteDeviceId, pimpl_->id_);
        if (git_remote_create(&remote_ptr, repo.get(), remoteDeviceId.c_str(), channelName.c_str())
            < 0) {
            JAMI_ERROR("Could not create remote for repository for conversation {}", pimpl_->id_);
            return false;
        }
    }
    GitRemote remote {remote_ptr, git_remote_free};

    fetch_opts.callbacks.transfer_progress = [](const git_indexer_progress* stats, void*) {
        // Uncomment to get advancment
        // if (stats->received_objects % 500 == 0 || stats->received_objects == stats->total_objects)
        //     JAMI_DEBUG("{}/{} {}kb", stats->received_objects, stats->total_objects,
        //     stats->received_bytes/1024);
        // If a pack is more than 256Mb, it's anormal.
        if (stats->received_bytes > MAX_FETCH_SIZE) {
            JAMI_ERROR("Abort fetching repository, the fetch is too big: {} bytes ({}/{})",
                       stats->received_bytes,
                       stats->received_objects,
                       stats->total_objects);
            return -1;
        }
        return 0;
    };
    if (git_remote_fetch(remote.get(), nullptr, &fetch_opts, "fetch") < 0) {
        const git_error* err = giterr_last();
        if (err) {
            JAMI_WARNING("Could not fetch remote repository for conversation {:s} {:s}",
                         pimpl_->id_,
                         err->message);
        }
        return false;
    }

    return true;
}

std::string
ConversationRepository::remoteHead(const std::string& remoteDeviceId,
                                   const std::string& branch) const
{
    git_remote* remote_ptr = nullptr;
    auto repo = pimpl_->repository();
    if (!repo || git_remote_lookup(&remote_ptr, repo.get(), remoteDeviceId.c_str()) < 0) {
        JAMI_WARNING("No remote found with id: {}", remoteDeviceId);
        return {};
    }
    GitRemote remote {remote_ptr, git_remote_free};

    git_reference* head_ref_ptr = nullptr;
    std::string remoteHead = "refs/remotes/" + remoteDeviceId + "/" + branch;
    git_oid commit_id;
    if (git_reference_name_to_id(&commit_id, repo.get(), remoteHead.c_str()) < 0) {
        const git_error* err = giterr_last();
        if (err)
            JAMI_ERROR("failed to lookup {} ref: {}", remoteHead, err->message);
        return {};
    }
    GitReference head_ref {head_ref_ptr, git_reference_free};

    auto commit_str = git_oid_tostr_s(&commit_id);
    if (!commit_str)
        return {};
    return commit_str;
}

void
ConversationRepository::Impl::addUserDevice()
{
    auto account = account_.lock();
    if (!account)
        return;

    // First, we need to add device file to the repository if not present
    auto repo = repository();
    if (!repo)
        return;
    // NOTE: libgit2 uses / for files
    std::string path = fmt::format("devices/{}.crt", account->currentDeviceId());
    std::string devicePath = git_repository_workdir(repo.get()) + path;
    if (!std::filesystem::is_regular_file(devicePath)) {
        auto file = fileutils::ofstream(devicePath, std::ios::trunc | std::ios::binary);
        if (!file.is_open()) {
            JAMI_ERROR("Could not write data to {}", devicePath);
            return;
        }
        auto cert = account->identity().second;
        auto deviceCert = cert->toString(false);
        file << deviceCert;
        file.close();

        if (!add(path))
            JAMI_WARNING("Couldn't add file {}", devicePath);
    }
}

std::string
ConversationRepository::commitMessage(const std::string& msg, bool verifyDevice)
{
    pimpl_->addUserDevice();
    return pimpl_->commit(msg, verifyDevice);
}

std::vector<std::string>
ConversationRepository::commitMessages(const std::vector<std::string>& msgs)
{
    pimpl_->addUserDevice();
    std::vector<std::string> ret;
    ret.reserve(msgs.size());
    for (const auto& msg : msgs)
        ret.emplace_back(pimpl_->commit(msg));
    return ret;
}

std::vector<ConversationCommit>
ConversationRepository::log(const LogOptions& options) const
{
    return pimpl_->log(options);
}

std::vector<std::map<std::string, std::string>>
ConversationRepository::search(const Filter& filter) const
{
    return pimpl_->search(filter);
}

std::optional<ConversationCommit>
ConversationRepository::getCommit(const std::string& commitId, bool logIfNotFound) const
{
    return pimpl_->getCommit(commitId, logIfNotFound);
}

std::pair<bool, std::string>
ConversationRepository::merge(const std::string& merge_id, bool force)
{
    // First, the repository must be in a clean state
    auto repo = pimpl_->repository();
    if (!repo) {
        JAMI_ERROR("Can't merge without repo");
        return {false, ""};
    }
    int state = git_repository_state(repo.get());
    if (state != GIT_REPOSITORY_STATE_NONE) {
        JAMI_ERROR("Merge operation aborted: repository is in unexpected state %d", state);
        return {false, ""};
    }
    // Checkout main (to do a `git_merge branch`)
    if (git_repository_set_head(repo.get(), "refs/heads/main") < 0) {
        JAMI_ERROR("Merge operation aborted: couldn't checkout main branch");
        return {false, ""};
    }

    // Then check that merge_id exists
    git_oid commit_id;
    if (git_oid_fromstr(&commit_id, merge_id.c_str()) < 0) {
        JAMI_ERROR("Merge operation aborted: couldn't lookup commit {}", merge_id);
        return {false, ""};
    }
    git_annotated_commit* annotated_ptr = nullptr;
    if (git_annotated_commit_lookup(&annotated_ptr, repo.get(), &commit_id) < 0) {
        JAMI_ERROR("Merge operation aborted: couldn't lookup commit {}", merge_id);
        return {false, ""};
    }
    GitAnnotatedCommit annotated {annotated_ptr, git_annotated_commit_free};

    // Now, we can analyze which type of merge do we need
    git_merge_analysis_t analysis;
    git_merge_preference_t preference;
    const git_annotated_commit* const_annotated = annotated.get();
    if (git_merge_analysis(&analysis, &preference, repo.get(), &const_annotated, 1) < 0) {
        JAMI_ERROR("Merge operation aborted: repository analysis failed");
        return {false, ""};
    }

    // Handle easy merges
    if (analysis & GIT_MERGE_ANALYSIS_UP_TO_DATE) {
        JAMI_LOG("Already up-to-date");
        return {true, ""};
    } else if (analysis & GIT_MERGE_ANALYSIS_UNBORN
               || (analysis & GIT_MERGE_ANALYSIS_FASTFORWARD
                   && !(preference & GIT_MERGE_PREFERENCE_NO_FASTFORWARD))) {
        if (analysis & GIT_MERGE_ANALYSIS_UNBORN)
            JAMI_LOG("Merge analysis result: Unborn");
        else
            JAMI_LOG("Merge analysis result: Fast-forward");
        const auto* target_oid = git_annotated_commit_id(annotated.get());

        if (!pimpl_->mergeFastforward(target_oid, (analysis & GIT_MERGE_ANALYSIS_UNBORN))) {
            const git_error* err = giterr_last();
            if (err)
                JAMI_ERROR("Fast forward merge failed: {}", err->message);
            return {false, ""};
        }
        return {true, ""}; // fast forward so no commit generated;
    }

    if (!pimpl_->validateDevice() && !force) {
        JAMI_ERROR("Invalid device. Not migrated?");
        return {false, ""};
    }

    // Else we want to check for conflicts
    git_oid head_commit_id;
    if (git_reference_name_to_id(&head_commit_id, repo.get(), "HEAD") < 0) {
        JAMI_ERROR("Cannot get reference for HEAD");
        return {false, ""};
    }

    git_commit* head_ptr = nullptr;
    if (git_commit_lookup(&head_ptr, repo.get(), &head_commit_id) < 0) {
        JAMI_ERROR("Could not look up HEAD commit");
        return {false, ""};
    }
    GitCommit head_commit {head_ptr, git_commit_free};

    git_commit* other__ptr = nullptr;
    if (git_commit_lookup(&other__ptr, repo.get(), &commit_id) < 0) {
        JAMI_ERROR("Could not look up HEAD commit");
        return {false, ""};
    }
    GitCommit other_commit {other__ptr, git_commit_free};

    git_merge_options merge_opts;
    git_merge_options_init(&merge_opts, GIT_MERGE_OPTIONS_VERSION);
    merge_opts.recursion_limit = 2;
    git_index* index_ptr = nullptr;
    if (git_merge_commits(&index_ptr, repo.get(), head_commit.get(), other_commit.get(), &merge_opts)
        < 0) {
        const git_error* err = giterr_last();
        if (err)
            JAMI_ERROR("Git merge failed: {}", err->message);
        return {false, ""};
    }
    GitIndex index {index_ptr, git_index_free};
    if (git_index_has_conflicts(index.get())) {
        JAMI_LOG("Some conflicts were detected during the merge operations. Resolution phase.");
        if (!pimpl_->resolveConflicts(index.get(), merge_id) or !git_add_all(repo.get())) {
            JAMI_ERROR("Merge operation aborted; Can't automatically resolve conflicts");
            return {false, ""};
        }
    }
    auto result = pimpl_->createMergeCommit(index.get(), merge_id);
    JAMI_LOG("Merge done between {} and main", merge_id);

    return {!result.empty(), result};
}

std::string
ConversationRepository::mergeBase(const std::string& from, const std::string& to) const
{
    if (auto repo = pimpl_->repository()) {
        git_oid oid, oidFrom, oidMerge;
        git_oid_fromstr(&oidFrom, from.c_str());
        git_oid_fromstr(&oid, to.c_str());
        git_merge_base(&oidMerge, repo.get(), &oid, &oidFrom);
        if (auto* commit_str = git_oid_tostr_s(&oidMerge))
            return commit_str;
    }
    return {};
}

std::string
ConversationRepository::diffStats(const std::string& newId, const std::string& oldId) const
{
    return pimpl_->diffStats(newId, oldId);
}

std::vector<std::string>
ConversationRepository::changedFiles(std::string_view diffStats)
{
    static const std::regex re(" +\\| +[0-9]+.*");
    std::vector<std::string> changedFiles;
    std::string_view line;
    while (jami::getline(diffStats, line)) {
        std::svmatch match;
        if (!std::regex_search(line, match, re) && match.size() == 0)
            continue;
        changedFiles.emplace_back(std::regex_replace(std::string {line}, re, "").substr(1));
    }
    return changedFiles;
}

std::string
ConversationRepository::join()
{
    // Check that not already member
    auto repo = pimpl_->repository();
    if (!repo)
        return {};
    std::string_view repoPath = git_repository_workdir(repo.get());
    auto account = pimpl_->account_.lock();
    if (!account)
        return {};
    auto cert = account->identity().second;
    auto parentCert = cert->issuer;
    if (!parentCert) {
        JAMI_ERROR("Parent cert is null!");
        return {};
    }
    auto uri = parentCert->getId().toString();
    std::string membersPath = repoPath + "members" + DIR_SEPARATOR_STR;
    std::string memberFile = membersPath + DIR_SEPARATOR_STR + uri + ".crt";
    std::string adminsPath = repoPath + "admins" + DIR_SEPARATOR_STR + uri + ".crt";
    if (std::filesystem::is_regular_file(memberFile) or std::filesystem::is_regular_file(adminsPath)) {
        // Already member, nothing to commit
        return {};
    }
    // Remove invited/uri.crt
    std::string invitedPath = repoPath + "invited";
    dhtnet::fileutils::remove(fileutils::getFullPath(invitedPath, uri));
    // Add members/uri.crt
    if (!dhtnet::fileutils::recursive_mkdir(membersPath, 0700)) {
        JAMI_ERROR("Error when creating {}. Abort create conversations", membersPath);
        return {};
    }
    auto file = fileutils::ofstream(memberFile, std::ios::trunc | std::ios::binary);
    if (!file.is_open()) {
        JAMI_ERROR("Could not write data to {}", memberFile);
        return {};
    }
    file << parentCert->toString(true);
    file.close();
    // git add -A
    if (!git_add_all(repo.get())) {
        return {};
    }
    Json::Value json;
    json["action"] = "join";
    json["uri"] = uri;
    json["type"] = "member";
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";

    {
        std::lock_guard<std::mutex> lk(pimpl_->membersMtx_);
        auto updated = false;

        for (auto& member : pimpl_->members_) {
            if (member.uri == uri) {
                updated = true;
                member.role = MemberRole::MEMBER;
                break;
            }
        }
        if (!updated)
            pimpl_->members_.emplace_back(ConversationMember {uri, MemberRole::MEMBER});
    }

    return commitMessage(Json::writeString(wbuilder, json));
}

std::string
ConversationRepository::leave()
{
    // TODO simplify
    auto account = pimpl_->account_.lock();
    auto repo = pimpl_->repository();
    if (!account || !repo)
        return {};
    auto details = account->getAccountDetails();
    auto deviceId = details[libjami::Account::ConfProperties::DEVICE_ID];
    auto uri = details[libjami::Account::ConfProperties::USERNAME];
    auto name = details[libjami::Account::ConfProperties::DISPLAYNAME];
    if (name.empty())
        name = account->getVolatileAccountDetails()
                   [libjami::Account::VolatileProperties::REGISTERED_NAME];
    if (name.empty())
        name = deviceId;

    // Remove related files
    std::string_view repoPath = git_repository_workdir(repo.get());
    std::string adminFile = fmt::format("{}admins/{}.crt", repoPath, uri);
    std::string memberFile = fmt::format("{}members/{}.crt", repoPath, uri);
    std::string crlsPath = fmt::format("{}CRLs", repoPath);

    if (std::filesystem::is_regular_file(adminFile)) {
        dhtnet::fileutils::removeAll(adminFile, true);
    }

    if (std::filesystem::is_regular_file(memberFile)) {
        dhtnet::fileutils::removeAll(memberFile, true);
    }

    // /CRLs
    for (const auto& crl : account->identity().second->getRevocationLists()) {
        if (!crl)
            continue;
        std::string crlPath = fmt::format("{}/{}/{}.crl", crlsPath, deviceId, dht::toHex(crl->getNumber()));
        if (std::filesystem::is_regular_file(crlPath)) {
            dhtnet::fileutils::removeAll(crlPath, true);
        }
    }

    // Devices
    for (const auto& d : account->getKnownDevices()) {
        std::string deviceFile = fmt::format("{}devices/{}.crt", repoPath, d.first);
        if (std::filesystem::is_regular_file(deviceFile)) {
            dhtnet::fileutils::removeAll(deviceFile, true);
        }
    }

    if (!git_add_all(repo.get())) {
        return {};
    }

    Json::Value json;
    json["action"] = "remove";
    json["uri"] = uri;
    json["type"] = "member";
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";

    {
        std::lock_guard<std::mutex> lk(pimpl_->membersMtx_);
        std::remove_if(pimpl_->members_.begin(), pimpl_->members_.end(), [&](auto& member) {
            return member.uri == account->getUsername();
        });
    }

    return pimpl_->commit(Json::writeString(wbuilder, json), false);
}

void
ConversationRepository::erase()
{
    // First, we need to add the member file to the repository if not present
    if (auto repo = pimpl_->repository()) {
        std::string repoPath = git_repository_workdir(repo.get());
        JAMI_LOG("Erasing {}", repoPath);
        dhtnet::fileutils::removeAll(repoPath, true);
    }
}

ConversationMode
ConversationRepository::mode() const
{
    return pimpl_->mode();
}

std::string
ConversationRepository::voteKick(const std::string& uri, const std::string& type)
{
    auto repo = pimpl_->repository();
    auto account = pimpl_->account_.lock();
    if (!account || !repo)
        return {};
    std::string_view repoPath = git_repository_workdir(repo.get());
    auto cert = account->identity().second;
    if (!cert || !cert->issuer)
        return {};
    auto adminUri = cert->issuer->getId().toString();
    if (adminUri == uri) {
        JAMI_WARNING("Admin tried to ban theirself");
        return {};
    }

    if (!std::filesystem::is_regular_file(
            fmt::format("{}/{}/{}{}", repoPath, type, uri, (type != "invited" ? ".crt" : "")))) {
        JAMI_WARNING("Didn't found file for {} with type {}", uri, type);
        return {};
    }

    auto relativeVotePath = fmt::format("votes/ban/{}/{}", type, uri);
    auto voteDirectory = repoPath + relativeVotePath;
    if (!dhtnet::fileutils::recursive_mkdir(voteDirectory, 0700)) {
        JAMI_ERROR("Error when creating {}. Abort vote", voteDirectory);
        return {};
    }
    auto votePath = fileutils::getFullPath(voteDirectory, adminUri);
    auto voteFile = fileutils::ofstream(votePath, std::ios::trunc | std::ios::binary);
    if (!voteFile.is_open()) {
        JAMI_ERROR("Could not write data to {}", votePath);
        return {};
    }
    voteFile.close();

    auto toAdd = fileutils::getFullPath(relativeVotePath, adminUri);
    if (!pimpl_->add(toAdd.c_str()))
        return {};

    Json::Value json;
    json["uri"] = uri;
    json["type"] = "vote";
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";
    return commitMessage(Json::writeString(wbuilder, json));
}

std::string
ConversationRepository::voteUnban(const std::string& uri, const std::string_view type)
{
    auto repo = pimpl_->repository();
    auto account = pimpl_->account_.lock();
    if (!account || !repo)
        return {};
    std::string repoPath = git_repository_workdir(repo.get());
    auto cert = account->identity().second;
    if (!cert || !cert->issuer)
        return {};
    auto adminUri = cert->issuer->getId().toString();

    auto relativeVotePath = fmt::format("votes/unban/{}/{}", type, uri);
    auto voteDirectory = repoPath + DIR_SEPARATOR_STR + relativeVotePath;
    if (!dhtnet::fileutils::recursive_mkdir(voteDirectory, 0700)) {
        JAMI_ERROR("Error when creating {}. Abort vote", voteDirectory);
        return {};
    }
    auto votePath = fileutils::getFullPath(voteDirectory, adminUri);
    auto voteFile = fileutils::ofstream(votePath, std::ios::trunc | std::ios::binary);
    if (!voteFile.is_open()) {
        JAMI_ERROR("Could not write data to {}", votePath);
        return {};
    }
    voteFile.close();

    auto toAdd = fileutils::getFullPath(relativeVotePath, adminUri);
    if (!pimpl_->add(toAdd.c_str()))
        return {};

    Json::Value json;
    json["uri"] = uri;
    json["type"] = "vote";
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";
    return commitMessage(Json::writeString(wbuilder, json));
}

bool
ConversationRepository::Impl::resolveBan(const std::string_view type, const std::string& uri)
{
    auto repo = repository();
    std::string repoPath = git_repository_workdir(repo.get());
    std::string bannedPath = repoPath + "banned";
    std::string devicesPath = repoPath + "devices";
    // Move from device or members file into banned
    auto crtStr = (type != "invited" ? ".crt" : "");
    std::string originFilePath = fmt::format("{}/{}/{}{}", repoPath, type, uri, crtStr);

    auto destPath = bannedPath + DIR_SEPARATOR_STR + type;
    auto destFilePath = fmt::format("{}/{}{}", destPath, uri, crtStr);
    if (!dhtnet::fileutils::recursive_mkdir(destPath, 0700)) {
        JAMI_ERROR("Error when creating {}. Abort resolving vote", destPath);
        return false;
    }

    if (std::rename(originFilePath.c_str(), destFilePath.c_str())) {
        JAMI_ERROR("Error when moving {} to {}. Abort resolving vote",
                 originFilePath,
                 destFilePath);
        return false;
    }

    // If members, remove related devices and mark as banned
    if (type != "devices") {
        for (const auto& certificate : dhtnet::fileutils::readDirectory(devicesPath)) {
            auto certPath = fileutils::getFullPath(devicesPath, certificate);
            try {
                crypto::Certificate cert(fileutils::loadFile(certPath));
                if (auto issuer = cert.issuer)
                    if (issuer->toString() == uri)
                        dhtnet::fileutils::remove(certPath, true);
            } catch (...) {
                continue;
            }
        }
        std::lock_guard<std::mutex> lk(membersMtx_);
        auto updated = false;

        for (auto& member : members_) {
            if (member.uri == uri) {
                updated = true;
                member.role = MemberRole::BANNED;
                break;
            }
        }
        if (!updated)
            members_.emplace_back(ConversationMember {uri, MemberRole::BANNED});
    }
    return true;
}

bool
ConversationRepository::Impl::resolveUnban(const std::string_view type, const std::string& uri)
{
    auto repo = repository();
    std::string repoPath = git_repository_workdir(repo.get());
    std::string bannedPath = repoPath + "banned";
    auto crtStr = (type != "invited" ? ".crt" : "");
    auto originFilePath = fmt::format("{}/{}/{}{}", bannedPath, type, uri, crtStr);
    auto destPath = repoPath + DIR_SEPARATOR_STR + type;
    auto destFilePath = fmt::format("{}/{}{}", destPath, uri, crtStr);
    if (!dhtnet::fileutils::recursive_mkdir(destPath, 0700)) {
        JAMI_ERROR("Error when creating {}. Abort resolving vote", destPath);
        return false;
    }
    if (std::rename(originFilePath.c_str(), destFilePath.c_str())) {
        JAMI_ERROR("Error when moving {} to {}. Abort resolving vote",
                 originFilePath,
                 destFilePath);
        return false;
    }

    std::lock_guard<std::mutex> lk(membersMtx_);
    auto updated = false;

    auto role = MemberRole::MEMBER;
    if (type == "invited")
        role = MemberRole::INVITED;
    else if (type == "admins")
        role = MemberRole::ADMIN;

    for (auto& member : members_) {
        if (member.uri == uri) {
            updated = true;
            member.role = role;
            break;
        }
    }
    if (!updated)
        members_.emplace_back(ConversationMember {uri, role});
    return true;
}

std::string
ConversationRepository::resolveVote(const std::string& uri,
                                    const std::string_view type,
                                    const std::string& voteType)
{
    // Count ratio admin/votes
    auto nbAdmins = 0, nbVotes = 0;
    // For each admin, check if voted
    auto repo = pimpl_->repository();
    if (!repo)
        return {};
    std::string repoPath = git_repository_workdir(repo.get());
    std::string adminsPath = repoPath + "admins";
    auto votePath = fmt::format("{}/{}", voteType, type);

    auto voteDirectory = fmt::format("{}/votes/{}/{}", repoPath, votePath, uri);
    for (const auto& certificate : dhtnet::fileutils::readDirectory(adminsPath)) {
        if (certificate.find(".crt") == std::string::npos) {
            JAMI_WARNING("Incorrect file found: {}/{}", adminsPath, certificate);
            continue;
        }
        auto adminUri = certificate.substr(0, certificate.size() - std::string(".crt").size());
        nbAdmins += 1;
        if (std::filesystem::is_regular_file(fileutils::getFullPath(voteDirectory, adminUri)))
            nbVotes += 1;
    }

    if (nbAdmins > 0 && (static_cast<double>(nbVotes) / static_cast<double>(nbAdmins)) > .5) {
        JAMI_WARNING("More than half of the admins voted to ban {}, apply the ban", uri);

        // Remove vote directory
        dhtnet::fileutils::removeAll(voteDirectory, true);

        if (voteType == "ban") {
            if (!pimpl_->resolveBan(type, uri))
                return {};
        } else if (voteType == "unban") {
            if (!pimpl_->resolveUnban(type, uri))
                return {};
        }

        // Commit
        if (!git_add_all(repo.get()))
            return {};

        Json::Value json;
        json["action"] = voteType;
        json["uri"] = uri;
        json["type"] = "member";
        Json::StreamWriterBuilder wbuilder;
        wbuilder["commentStyle"] = "None";
        wbuilder["indentation"] = "";
        return commitMessage(Json::writeString(wbuilder, json));
    }

    // If vote nok
    return {};
}

std::pair<std::vector<ConversationCommit>, bool>
ConversationRepository::validFetch(const std::string& remoteDevice) const
{
    auto newCommit = remoteHead(remoteDevice);
    if (not pimpl_ or newCommit.empty())
        return {{}, false};
    auto commitsToValidate = pimpl_->behind(newCommit);
    std::reverse(std::begin(commitsToValidate), std::end(commitsToValidate));
    auto isValid = pimpl_->validCommits(commitsToValidate);
    if (isValid)
        return {commitsToValidate, false};
    return {{}, true};
}

bool
ConversationRepository::validClone() const
{
    return pimpl_->validCommits(log({}));
}

void
ConversationRepository::removeBranchWith(const std::string& remoteDevice)
{
    git_remote* remote_ptr = nullptr;
    auto repo = pimpl_->repository();
    if (!repo || git_remote_lookup(&remote_ptr, repo.get(), remoteDevice.c_str()) < 0) {
        JAMI_WARNING("No remote found with id: {}", remoteDevice);
        return;
    }
    GitRemote remote {remote_ptr, git_remote_free};

    git_remote_prune(remote.get(), nullptr);
}

std::vector<std::string>
ConversationRepository::getInitialMembers() const
{
    return pimpl_->getInitialMembers();
}

std::vector<ConversationMember>
ConversationRepository::members() const
{
    return pimpl_->members();
}

std::vector<std::string>
ConversationRepository::memberUris(std::string_view filter,
                                   const std::set<MemberRole>& filteredRoles) const
{
    return pimpl_->memberUris(filter, filteredRoles);
}

std::map<std::string, std::vector<DeviceId>>
ConversationRepository::devices(bool ignoreExpired) const
{
    return pimpl_->devices(ignoreExpired);
}

void
ConversationRepository::refreshMembers() const
{
    try {
        pimpl_->initMembers();
    } catch (...) {
    }
}

void
ConversationRepository::pinCertificates(bool blocking)
{
    auto acc = pimpl_->account_.lock();
    auto repo = pimpl_->repository();
    if (!repo or !acc)
        return;

    std::string repoPath = git_repository_workdir(repo.get());
    std::vector<std::string> paths = {repoPath + "admins",
                                      repoPath + "members",
                                      repoPath + "devices"};

    for (const auto& path : paths) {
        if (blocking) {
            std::promise<bool> p;
            std::future<bool> f = p.get_future();
            acc->certStore().pinCertificatePath(path, [&](auto /* certs */) { p.set_value(true); });
            f.wait();
        } else {
            acc->certStore().pinCertificatePath(path, {});
        }
    }
}

std::string
ConversationRepository::uriFromDevice(const std::string& deviceId) const
{
    return pimpl_->uriFromDevice(deviceId);
}

std::string
ConversationRepository::updateInfos(const std::map<std::string, std::string>& profile)
{
    auto account = pimpl_->account_.lock();
    if (!account)
        return {};
    auto uri = std::string(account->getUsername());
    auto valid = false;
    {
        std::lock_guard<std::mutex> lk(pimpl_->membersMtx_);
        for (const auto& member : pimpl_->members_) {
            if (member.uri == uri) {
                valid = member.role <= pimpl_->updateProfilePermLvl_;
                break;
            }
        }
    }
    if (!valid) {
        JAMI_ERROR("Not enough authorization for updating infos");
        emitSignal<libjami::ConversationSignal::OnConversationError>(
            account->getAccountID(),
            pimpl_->id_,
            EUNAUTHORIZED,
            "Not enough authorization for updating infos");
        return {};
    }

    auto infosMap = infos();
    for (const auto& [k, v] : profile) {
        infosMap[k] = v;
    }
    auto repo = pimpl_->repository();
    if (!repo)
        return {};
    std::string repoPath = git_repository_workdir(repo.get());
    auto profilePath = repoPath + "profile.vcf";
    auto file = fileutils::ofstream(profilePath, std::ios::trunc | std::ios::binary);
    if (!file.is_open()) {
        JAMI_ERROR("Could not write data to {}", profilePath);
        return {};
    }

    auto addKey = [&](auto property, auto key) {
        auto it = infosMap.find(key);
        if (it != infosMap.end()) {
            file << property;
            file << ":";
            file << it->second;
            file << vCard::Delimiter::END_LINE_TOKEN;
        }
    };

    file << vCard::Delimiter::BEGIN_TOKEN;
    file << vCard::Delimiter::END_LINE_TOKEN;
    file << vCard::Property::VCARD_VERSION;
    file << ":2.1";
    file << vCard::Delimiter::END_LINE_TOKEN;
    addKey(vCard::Property::FORMATTED_NAME, vCard::Value::TITLE);
    addKey(vCard::Property::DESCRIPTION, vCard::Value::DESCRIPTION);
    file << vCard::Property::PHOTO;
    file << vCard::Delimiter::SEPARATOR_TOKEN;
    file << vCard::Property::BASE64;
    auto avatarIt = infosMap.find(vCard::Value::AVATAR);
    if (avatarIt != infosMap.end()) {
        // TODO type=png? store another way?
        file << ":";
        file << avatarIt->second;
    }
    file << vCard::Delimiter::END_LINE_TOKEN;
    addKey(vCard::Property::RDV_ACCOUNT, vCard::Value::RDV_ACCOUNT);
    file << vCard::Delimiter::END_LINE_TOKEN;
    addKey(vCard::Property::RDV_DEVICE, vCard::Value::RDV_DEVICE);
    file << vCard::Delimiter::END_LINE_TOKEN;
    file << vCard::Delimiter::END_TOKEN;
    file.close();

    if (!pimpl_->add("profile.vcf"))
        return {};
    Json::Value json;
    json["type"] = "application/update-profile";
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";

    return pimpl_->commit(Json::writeString(wbuilder, json));
}

std::map<std::string, std::string>
ConversationRepository::infos() const
{
    if (auto repo = pimpl_->repository()) {
        try {
            std::string repoPath = git_repository_workdir(repo.get());
            auto profilePath = repoPath + "profile.vcf";
            std::map<std::string, std::string> result;
            if (std::filesystem::is_regular_file(profilePath)) {
                auto content = fileutils::loadFile(profilePath);
                result = ConversationRepository::infosFromVCard(vCard::utils::toMap(
                    std::string_view {(const char*) content.data(), content.size()}));
            }
            result["mode"] = std::to_string(static_cast<int>(mode()));
            return result;
        } catch (...) {
        }
    }
    return {};
}

std::map<std::string, std::string>
ConversationRepository::infosFromVCard(std::map<std::string, std::string>&& details)
{
    std::map<std::string, std::string> result;
    for (auto&& [k, v] : details) {
        if (k == vCard::Property::FORMATTED_NAME) {
            result["title"] = std::move(v);
        } else if (k == vCard::Property::DESCRIPTION) {
            result["description"] = std::move(v);
        } else if (k.find(vCard::Property::PHOTO) == 0) {
            result["avatar"] = std::move(v);
        } else if (k.find(vCard::Property::RDV_ACCOUNT) == 0) {
            result["rdvAccount"] = std::move(v);
        } else if (k.find(vCard::Property::RDV_DEVICE) == 0) {
            result["rdvDevice"] = std::move(v);
        }
    }
    return result;
}

std::string
ConversationRepository::getHead() const
{
    if (auto repo = pimpl_->repository()) {
        git_oid commit_id;
        if (git_reference_name_to_id(&commit_id, repo.get(), "HEAD") < 0) {
            JAMI_ERROR("Cannot get reference for HEAD");
            return {};
        }
        if (auto commit_str = git_oid_tostr_s(&commit_id))
            return commit_str;
    }
    return {};
}

std::optional<std::map<std::string, std::string>>
ConversationRepository::convCommitToMap(const ConversationCommit& commit) const
{
    return pimpl_->convCommitToMap(commit);
}

std::vector<std::map<std::string, std::string>>
ConversationRepository::convCommitToMap(const std::vector<ConversationCommit>& commits) const
{
    std::vector<std::map<std::string, std::string>> result = {};
    result.reserve(commits.size());
    for (const auto& commit : commits) {
        auto message = pimpl_->convCommitToMap(commit);
        if (message == std::nullopt)
            continue;
        result.emplace_back(*message);
    }
    return result;
}

} // namespace jami
