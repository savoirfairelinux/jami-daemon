/*
 *  Copyright (C) 2017-2019 Savoir-faire Linux Inc.
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

#include <opendht/rng.h>
using random_device = dht::crypto::random_device;

#include <git2.h>

#include <ctime>
#include <fstream>

using GitRepository = std::unique_ptr<git_repository, decltype(&git_repository_free)>;

constexpr const char* const FLUSH_PKT = "0000";
constexpr const char* const NAK_PKT = "0008NAK\n";
constexpr const char* const DONE_PKT = "0009done\n";

constexpr const char* const WANT_CMD = "want";
constexpr const char* const HAVE_CMD = "have";

using namespace std::string_literals;

namespace jami {

class ConversationRepository::Impl
{
public:
    Impl(const std::weak_ptr<JamiAccount>& account, const std::string& id) : account_(account), id_(id) {
        auto shared = account.lock();
        if (!shared) return;
        auto path = fileutils::get_data_dir()+DIR_SEPARATOR_STR+shared->getAccountID()+DIR_SEPARATOR_STR+"conversations"+DIR_SEPARATOR_STR+id_;
        git_repository *repo = nullptr;
        // TODO share this repo with GitServer
        if (git_repository_open(&repo, path.c_str()) != 0) {
            JAMI_WARN("Couldn't open %s", path.c_str());
            return;
        }
        repository_ = {std::move(repo), git_repository_free};
    }
    ~Impl() = default;

    std::weak_ptr<JamiAccount> account_;
    const std::string id_;
    GitRepository repository_ {nullptr, git_repository_free};
};

/////////////////////////////////////////////////////////////////////////////////

/**
 * Creates an empty repository
 * @param path       Path of the new repository
 * @return The libgit2's managed repository
 */
GitRepository
create_empty_repository(const std::string path) {
    git_repository *repo = nullptr;
    if (git_repository_init(&repo, path.c_str(), false /* we want a non-bare repo to work on it */) < 0) {
        JAMI_ERR("Couldn't create a git repository in %s", path.c_str());
    }
    return {std::move(repo), git_repository_free};
}

/**
 * Adds initial files. This adds the certificate of the account in the /admins directory
 * the device's key in /devices and the CRLs in /CRLs.
 * @param repo      The repository
 * @return if files were added successfully
 */
bool
add_initial_files(GitRepository& repo, const std::shared_ptr<JamiAccount>& account)
{
    auto deviceId = account->getAccountDetails()[DRing::Account::ConfProperties::RING_DEVICE_ID];
    std::string repoPath = git_repository_workdir(repo.get());
    std::string adminsPath = repoPath + "admins";
    std::string devicesPath = repoPath + "devices";
    std::string crlsPath = repoPath + "CRLs" + DIR_SEPARATOR_STR + deviceId;

    if (!fileutils::recursive_mkdir(adminsPath, 0700)) {
        JAMI_ERR("Error when creating %s. Abort create conversations", adminsPath.c_str());
        return false;
    }

    auto cert = account->identity().second;
    auto deviceCert = cert->toString(false);
    auto parentCert = cert->issuer;
    if (!parentCert) {
        JAMI_ERR("Parent cert is null!");
        return false;
    }

    // /admins
    std::string adminPath = adminsPath + DIR_SEPARATOR_STR + parentCert->getId().toString() + ".crt";
    auto file = fileutils::ofstream(adminPath, std::ios::trunc | std::ios::binary);
    if (!file.is_open()) {
        JAMI_ERR("Could not write data to %s", adminPath.c_str());
        return false;
    }
    file << parentCert->toString(true);
    file.close();

    if (!fileutils::recursive_mkdir(devicesPath, 0700)) {
        JAMI_ERR("Error when creating %s. Abort create conversations", devicesPath.c_str());
        return false;
    }

    // /devices
    std::string devicePath = devicesPath + DIR_SEPARATOR_STR + cert->getId().toString() + ".crt";
    file = fileutils::ofstream(devicePath, std::ios::trunc | std::ios::binary);
    if (!file.is_open()) {
        JAMI_ERR("Could not write data to %s", devicePath.c_str());
        return false;
    }
    file << deviceCert;
    file.close();

    if (!fileutils::recursive_mkdir(crlsPath, 0700)) {
        JAMI_ERR("Error when creating %s. Abort create conversations", crlsPath.c_str());
        return false;
    }

    // /CRLs
    for (const auto& crl : account->identity().second->getRevocationLists()) {
        if (!crl) continue;
        auto v = crl->getNumber();
        std::stringstream ss;
        ss << std::hex;
        for (const auto& b : v)
            ss << (unsigned)b;
        std::string crlPath = crlsPath + DIR_SEPARATOR_STR + deviceId + DIR_SEPARATOR_STR + ss.str() + ".crl";
        file = fileutils::ofstream(crlPath, std::ios::trunc | std::ios::binary);
        if (!file.is_open()) {
            JAMI_ERR("Could not write data to %s", crlPath.c_str());
            return false;
        }
        file << crl->toString();
        file.close();
    }

    // git add -A
    git_index *index;
    git_strarray array = {0};

    if (git_repository_index(&index, repo.get()) < 0) {
        JAMI_ERR("Could not open repository index");
        return false;
    }

    git_index_add_all(index, &array, 0, nullptr, nullptr);
    git_index_write(index);
    git_index_free(index);

    JAMI_INFO("Initial files added in %s", repoPath.c_str());
    return true;
}

/**
 * Sign and create the initial commit
 * @param repo      The git repository
 * @param account   The account who signs
 * @return          The first commit hash or empty if failed
 */
std::string
initial_commit(GitRepository& repo, const std::shared_ptr<JamiAccount>& account)
{
    auto deviceId = account->getAccountDetails()[DRing::Account::ConfProperties::RING_DEVICE_ID];
    auto name = account->getAccountDetails()[DRing::Account::ConfProperties::DISPLAYNAME];
    if (name.empty())
        name = account->getVolatileAccountDetails()[DRing::Account::VolatileProperties::REGISTERED_NAME];
    if (name.empty())
        name = deviceId;

    git_signature *sig;
	git_index *index;
	git_oid tree_id, commit_id;
	git_tree *tree;
    git_buf to_sign = {};

    // Sign commit's buffer
    if (git_signature_new(&sig, name.c_str(), deviceId.c_str(), std::time(nullptr), 0) < 0) {
		JAMI_ERR("Unable to create a commit signature.");
        return {};
    }

    if (git_repository_index(&index, repo.get()) < 0) {
		JAMI_ERR("Could not open repository index");
        return {};
    }

	if (git_index_write_tree(&tree_id, index) < 0) {
		JAMI_ERR("Unable to write initial tree from index");
        return {};
    }

	git_index_free(index);

	if (git_tree_lookup(&tree, repo.get(), &tree_id) < 0) {
		JAMI_ERR("Could not look up initial tree");
        git_tree_free(tree);
        return {};
    }

    if (git_commit_create_buffer(&to_sign, repo.get(), sig, sig, nullptr, "Initial commit", tree, 0, nullptr) < 0) {
        JAMI_ERR("Could not create initial buffer");
        git_tree_free(tree);
        return {};
    }

    auto to_sign_vec = std::vector<uint8_t>(to_sign.ptr, to_sign.ptr + to_sign.size);
    auto signed_buf = account->identity().first->sign(to_sign_vec);
    std::string signed_str = base64::encode(signed_buf.begin(), signed_buf.end());

    // git commit -S
    if (git_commit_create_with_signature(&commit_id, repo.get(), to_sign.ptr, signed_str.c_str(), "signature") < 0) {
        JAMI_ERR("Could not sign initial commit");
        git_tree_free(tree);
        return {};
    }

    // Move commit to master branch
    git_commit* commit;
    if (git_commit_lookup(&commit, repo.get(), &commit_id) == 0) {
        git_reference* ref;
        git_branch_create(&ref, repo.get(), "master", commit, true);
        git_reference_free(ref);
        git_commit_free(commit);
    }

	git_tree_free(tree);
        git_signature_free(sig);

    auto commit_str = git_oid_tostr_s(&commit_id);
    if (commit_str) return commit_str;
    return {};
}

std::unique_ptr<ConversationRepository>
ConversationRepository::createConversation(const std::weak_ptr<JamiAccount>& account)
{
    auto shared = account.lock();
    if (!shared) return {};
    // Create temporary directory because we can't know the first hash for now
    std::uniform_int_distribution<uint64_t> dist{ 0, std::numeric_limits<uint64_t>::max() };
    random_device rdev;
    auto tmpPath = fileutils::get_data_dir()+DIR_SEPARATOR_STR+shared->getAccountID()+DIR_SEPARATOR_STR+"conversations"+DIR_SEPARATOR_STR+std::to_string(dist(rdev));
    if (fileutils::isDirectory(tmpPath)) {
        JAMI_ERR("%s already exists. Abort create conversations", tmpPath.c_str());
        return {};
    }
    if (!fileutils::recursive_mkdir(tmpPath, 0700)) {
        JAMI_ERR("Error when creating %s. Abort create conversations", tmpPath.c_str());
        return {};
    }
    auto repo = create_empty_repository(tmpPath);
    if (!repo) {
        return {};
    }

    // Add initial files
    if (!add_initial_files(repo, shared)) {
        JAMI_ERR("Error when adding initial files");
        fileutils::removeAll(tmpPath, true);
        return {};
    }

    // Commit changes
    auto id = initial_commit(repo, shared);
    if (id.empty()) {
        JAMI_ERR("Couldn't create initial commit in %s", tmpPath.c_str());
        fileutils::removeAll(tmpPath, true);
        return {};
    }

    // Move to wanted directory
    auto newPath = fileutils::get_data_dir()+DIR_SEPARATOR_STR+shared->getAccountID()+DIR_SEPARATOR_STR+"conversations"+DIR_SEPARATOR_STR+id;
    if (std::rename(tmpPath.c_str(), newPath.c_str())) {
        JAMI_ERR("Couldn't move %s in %s", tmpPath.c_str(), newPath.c_str());
        fileutils::removeAll(tmpPath, true);
        return {};
    }

    JAMI_INFO("New conversation initialized in %s", newPath.c_str());

    return std::make_unique<ConversationRepository>(account, id);
}

std::unique_ptr<ConversationRepository>
ConversationRepository::cloneConversation(
    const std::weak_ptr<JamiAccount>& account,
    const std::string& deviceId,
    const std::string& conversationId
)
{
    auto shared = account.lock();
    if (!shared) return {};
    auto path = fileutils::get_data_dir()+DIR_SEPARATOR_STR+shared->getAccountID()+DIR_SEPARATOR_STR+"conversations"+DIR_SEPARATOR_STR+conversationId;
    git_repository *rep = nullptr;
    std::stringstream url;
    url << "git://" << deviceId << '/' << conversationId;
    if (git_clone(&rep, url.str().c_str(), path.c_str(), nullptr) < 0) {
        const git_error *err = giterr_last();
        if (err) JAMI_ERR("Error when retrieving remote conversation: %s", err->message);
        return nullptr;
    }
    JAMI_INFO("New conversation cloned in %s", path.c_str());
    git_repository_free(rep);
    return std::make_unique<ConversationRepository>(account, conversationId);
}

/////////////////////////////////////////////////////////////////////////////////

enum class ServerState
{
    WAIT_ORDER,
    SEND_REFERENCES_CAPABILITIES,
    ANSWER_TO_WANT_ORDER,
    SEND_PACKDATA,
    TERMINATE
};

class GitServer
{
public:
    GitServer(const std::string& repositoryId, const std::string& repository,
        const std::shared_ptr<ChannelSocket>& socket)
    : repositoryId_(repositoryId), repository_(repository), socket_(socket) {
        execute();
    }

    void execute();
    void waitOrder();
    void sendReferenceCapabilities();
    void answerToWantOrder();
    void sendPackData();

    std::string repositoryId_ {};
    std::string repository_ {};
    std::shared_ptr<ChannelSocket> socket_ {};
    ServerState state_ {ServerState::WAIT_ORDER};
    std::string wantedReference_ {};
};

void
GitServer::execute()
{
    auto stop = false;
    while (!stop) {
        switch (state_) {
            case ServerState::WAIT_ORDER:
            waitOrder();
            break;
            case ServerState::SEND_REFERENCES_CAPABILITIES:
            sendReferenceCapabilities();
            break;
            case ServerState::ANSWER_TO_WANT_ORDER:
            answerToWantOrder();
            break;
            case ServerState::SEND_PACKDATA:
            sendPackData();
            break;
            case ServerState::TERMINATE:
            stop = true;
            break;
        }
    }
}

void
GitServer::waitOrder()
{
    // TODO blocking read
    std::error_code ec;
    auto res = socket_->waitForData(std::chrono::milliseconds(5000), ec);
    if (ec) {
        JAMI_WARN("Error when reading socket for %s: %s", repository_.c_str(), ec.message().c_str());
        state_ = ServerState::TERMINATE;
        return;
    }
    if (res <= 4) return;
    uint8_t buf[UINT16_MAX];
    socket_->read(buf, res, ec);
    if (ec) {
        JAMI_WARN("Error when reading socket for %s: %s", repository_.c_str(), ec.message().c_str());
        state_ = ServerState::TERMINATE;
        return;
    }
    std::string pkt = {buf, buf + res};

    if (pkt.find(UPLOAD_PACK_CMD) == 4) {
        // NOTE: We do not retrieve version or repo for now
        JAMI_INFO("Upload pack command detected.");
        state_ = ServerState::SEND_REFERENCES_CAPABILITIES;
    } else if (pkt.find(WANT_CMD) == 4) {
        // TODO HAVE_CMD
        // TODO more than one pkt
        auto done = false;
        std::string stopFlag = std::string(FLUSH_PKT) + DONE_PKT;
        while (pkt != stopFlag) {
            // Get pktlen
            unsigned int pktlen;
            std::stringstream ss;
            ss << std::hex << pkt.substr(0,4);
            ss >> pktlen;
            auto content = pkt.substr(5, pktlen-5);
            auto commit = content.substr(4, 40);
            if (commit == repositoryId_) {
                // TODO
                wantedReference_ = "HEAD";
            }
            pkt = pkt.substr(pktlen, pkt.size() - pktlen);
        }
        state_ = ServerState::ANSWER_TO_WANT_ORDER;
    } else {
        JAMI_WARN("Unkown packet received: %s", pkt.c_str());
    }
}

void
GitServer::sendReferenceCapabilities()
{
    // TODO store more references?
    state_ = ServerState::WAIT_ORDER;

    git_repository* repo;
    if (git_repository_open(&repo, repository_.c_str()) != 0) {
        JAMI_WARN("Couldn't open %s", repository_.c_str());
        return;
    }
    git_oid commit_id;
    if (git_reference_name_to_id(&commit_id, repo, "HEAD") < 0) {
        JAMI_ERR("Cannot get reference for HEAD");
        git_repository_free(repo);
        return;
    }
    std::string headRef = git_oid_tostr_s(&commit_id);
    git_repository_free(repo);

    std::stringstream capabilities;
    capabilities << headRef << " HEAD\0side-band side-band-64k shallow no-progress include-tag"s;
    std::string capStr = capabilities.str();

    std::stringstream packet;
    packet << std::setw(4) << std::setfill('0') << std::hex << ((5 + capStr.size()) & 0x0FFFF);
    packet << capStr << "\n";
    // TODO get last commit and store references for want!
    packet << "003f" << headRef << " refs/heads/master\n";
    packet << FLUSH_PKT;

    std::error_code ec;
    socket_->write(reinterpret_cast<const unsigned char*>(packet.str().c_str()), packet.str().size(), ec);
    if (ec) {
        JAMI_WARN("Couldn't send data for %s: %s", repository_.c_str(), ec.message().c_str());
    }
}

void
GitServer::answerToWantOrder()
{
    state_ = ServerState::WAIT_ORDER;
    std::error_code ec;
    socket_->write(reinterpret_cast<const unsigned char*>(NAK_PKT), std::strlen(NAK_PKT), ec);
    if (ec) {
        JAMI_WARN("Couldn't send data for %s: %s", repository_.c_str(), ec.message().c_str());
        return;
    }
    state_ = ServerState::SEND_PACKDATA;
}

void
GitServer::sendPackData()
{
    state_ = ServerState::WAIT_ORDER;
    git_repository* repo;

    if (git_repository_open(&repo, repository_.c_str()) != 0) {
        JAMI_WARN("Couldn't open %s", repository_.c_str());
        return;
    }

    git_packbuilder *pb;
    if (git_packbuilder_new(&pb, repo) != 0) {
        JAMI_WARN("Couldn't open packbuilder for %s", repository_.c_str());
        git_repository_free(repo);
        return;
    }

    // TODO: here only one commit is written!
    git_oid commit_id;
    if (git_reference_name_to_id(&commit_id, repo, wantedReference_.c_str()) != 0) {
        JAMI_WARN("Couldn't open reference %s for %s", wantedReference_.c_str(), repository_.c_str());
        git_repository_free(repo);
        return;
    }
    if (git_packbuilder_insert_commit(pb, &commit_id) != 0) {
        JAMI_WARN("Couldn't open insert commit %s for %s", git_oid_tostr_s(&commit_id), repository_.c_str());
        git_repository_free(repo);
        return;
    }

    git_buf data = {};
    if (git_packbuilder_write_buf(&data, pb) != 0) {
        JAMI_WARN("Couldn't write data commit %s for %s", git_oid_tostr_s(&commit_id), repository_.c_str());
        git_repository_free(repo);
        return;
    }

    std::stringstream toSend;
    toSend << std::setw(4) << std::setfill('0') << std::hex << ((data.size + 5) & 0x0FFFF);
    toSend << "\x1";
    std::string toSendStr = toSend.str();

    std::error_code ec;
    socket_->write(reinterpret_cast<const unsigned char*>(toSendStr.c_str()), toSendStr.size(), ec);
    if (ec) {
        JAMI_WARN("Couldn't send data for %s: %s", repository_.c_str(), ec.message().c_str());
        git_packbuilder_free(pb);
        git_repository_free(repo);
        return;
    }

    socket_->write(reinterpret_cast<const unsigned char*>(data.ptr), data.size, ec);
    if (ec) {
        JAMI_WARN("Couldn't send data for %s: %s", repository_.c_str(), ec.message().c_str());
        git_packbuilder_free(pb);
        git_repository_free(repo);
        return;
    }

    git_repository_free(repo);
    git_packbuilder_free(pb);

    socket_->write(reinterpret_cast<const unsigned char*>(FLUSH_PKT), std::strlen(FLUSH_PKT), ec);
    if (ec) {
        JAMI_WARN("Couldn't send data for %s: %s", repository_.c_str(), ec.message().c_str());
    }
}

/////////////////////////////////////////////////////////////////////////////////

ConversationRepository::ConversationRepository(const std::weak_ptr<JamiAccount>& account, const std::string& id)
: pimpl_ { new Impl { account, id } }
{}

ConversationRepository::~ConversationRepository() = default;

void
ConversationRepository::serve(const std::shared_ptr<ChannelSocket>& client)
{
    auto shared = pimpl_->account_.lock();
    if (!shared) return;
    auto path = fileutils::get_data_dir()+DIR_SEPARATOR_STR+shared->getAccountID()+DIR_SEPARATOR_STR+"conversations"+DIR_SEPARATOR_STR+pimpl_->id_;
    GitServer server(pimpl_->id_, path, client);
}

std::string
ConversationRepository::id() const
{
    return pimpl_->id_;
}

std::string
ConversationRepository::sendMessage(const std::string& msg)
{
    auto account = pimpl_->account_.lock();
    if (!account) return {};
    auto deviceId = account->getAccountDetails()[DRing::Account::ConfProperties::RING_DEVICE_ID];
    auto name = account->getAccountDetails()[DRing::Account::ConfProperties::DISPLAYNAME];
    if (name.empty())
        name = account->getVolatileAccountDetails()[DRing::Account::VolatileProperties::REGISTERED_NAME];
    if (name.empty())
        name = deviceId;

    // First, we need to add device file to the repository if not present
    std::string repoPath = git_repository_workdir(pimpl_->repository_.get());
    std::string devicePath = repoPath + DIR_SEPARATOR_STR + "devices" + DIR_SEPARATOR_STR + deviceId + ".crt";
    if (!fileutils::isFile(devicePath)) {
        auto file = fileutils::ofstream(devicePath, std::ios::trunc | std::ios::binary);
        if (!file.is_open()) {
            JAMI_ERR("Could not write data to %s", devicePath.c_str());
            return {};
        }
        auto cert = account->identity().second;
        auto deviceCert = cert->toString(false);
        file << deviceCert;
        file.close();

        // git add
        git_index *index;

        if (git_repository_index(&index, pimpl_->repository_.get()) < 0) {
            JAMI_ERR("Could not open repository index");
            return {};
        }

        git_index_add_bypath(index, devicePath.c_str());
        git_index_write(index);
        git_index_free(index);
    }

    git_signature *sig;
    // Sign commit's buffer
    if (git_signature_new(&sig, name.c_str(), deviceId.c_str(), std::time(nullptr), 0) < 0) {
		JAMI_ERR("Unable to create a commit signature.");
        return {};
    }

    // Retrieve current HEAD
    git_oid commit_id;
    if (git_reference_name_to_id(&commit_id, pimpl_->repository_.get(), "HEAD") < 0) {
        JAMI_ERR("Cannot get reference for HEAD");
        git_signature_free(sig);
        return {};
    }

    git_commit* head_commit;
    if (git_commit_lookup(&head_commit, pimpl_->repository_.get(), &commit_id) < 0) {
		JAMI_ERR("Could not look up HEAD commit");
        git_signature_free(sig);
        return {};
    }

    git_tree *tree;
	if (git_commit_tree(&tree, head_commit) < 0) {
		JAMI_ERR("Could not look up initial tree");
        git_signature_free(sig);
        return {};
    }

    git_buf to_sign = {};
    const git_commit* head_ref[1] = { head_commit };
    if (git_commit_create_buffer(&to_sign, pimpl_->repository_.get(),
            sig, sig, nullptr, msg.c_str(), tree, 1, &head_ref[0]) < 0) {
        JAMI_ERR("Could not create commit buffer");
        git_commit_free(head_commit);
        git_tree_free(tree);
        git_signature_free(sig);
        return {};
    }

    git_tree_free(tree);
    git_commit_free(head_commit);

    // git commit -S
    auto to_sign_vec = std::vector<uint8_t>(to_sign.ptr, to_sign.ptr + to_sign.size);
    auto signed_buf = account->identity().first->sign(to_sign_vec);
    std::string signed_str = base64::encode(signed_buf.begin(), signed_buf.end());
    if (git_commit_create_with_signature(&commit_id, pimpl_->repository_.get(), to_sign.ptr, signed_str.c_str(), "signature") < 0) {
        JAMI_ERR("Could not sign commit");
        return {};
    }

    git_signature_free(sig);

    // Move commit to master branch
    git_reference *ref;
    if (git_reference_create(&ref, pimpl_->repository_.get(), "refs/heads/master", &commit_id, true, nullptr) < 0) {
        JAMI_WARN("Could not move commit to master");
    }
    git_reference_free(ref);

    auto commit_str = git_oid_tostr_s(&commit_id);
    if (commit_str) {
        JAMI_INFO("New message added with id: %s", commit_str);
    }
    return commit_str ? commit_str : "";
}

}