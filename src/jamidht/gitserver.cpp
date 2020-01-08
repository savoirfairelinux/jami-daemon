/*
 *  Copyright (C) 2019 Savoir-faire Linux Inc.
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
#include "gitserver.h"

#include "fileutils.h"
#include "logger.h"
#include "gittransport.h"
#include "multiplexed_socket.h"

#include <ctime>
#include <fstream>
#include <git2.h>
#include <iomanip>

constexpr const char* const FLUSH_PKT = "0000";
constexpr const char* const NAK_PKT = "0008NAK\n";
constexpr const char* const DONE_PKT = "0009done\n";

constexpr const char* const WANT_CMD = "want";
constexpr const char* const HAVE_CMD = "have";

using namespace std::string_literals;

namespace jami {

enum class ServerState
{
    WAIT_ORDER,
    SEND_REFERENCES_CAPABILITIES,
    ANSWER_TO_WANT_ORDER,
    SEND_PACKDATA,
    TERMINATE
};

class GitServer::Impl
{
public:
    Impl(const std::string& repositoryId, const std::string& repository,
        const std::shared_ptr<ChannelSocket>& socket)
    : repositoryId_(repositoryId), repository_(repository), socket_(socket) {}
    ~Impl() = default;

    void run();
    void waitOrder();
    void sendReferenceCapabilities();
    void answerToWantOrder();
    void sendPackData();

    std::string repositoryId_ {};
    std::string repository_ {};
    std::shared_ptr<ChannelSocket> socket_ {};
    ServerState state_ {ServerState::WAIT_ORDER};
    std::string wantedReference_ {};
    std::vector<std::string> haveRefs_ {};
    std::string currentHead_ {};
    std::string cachedPkt_ {};
    std::atomic_bool isDestroying_ {false};
};

void
GitServer::Impl::run()
{
    while (!isDestroying_.load()) {
        switch (state_) {
            case ServerState::WAIT_ORDER:
            waitOrder(); // TODO don't close at the end
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
            break;
        }
    }
}

void
GitServer::Impl::waitOrder()
{
    // TODO blocking read
    std::string pkt = cachedPkt_;
    if (pkt.empty()) {
        std::error_code ec;
        auto res = socket_->waitForData(std::chrono::milliseconds(5000), ec);
        if (ec) {
            if (!isDestroying_.load()) JAMI_WARN("Error when reading socket for %s: %s", repository_.c_str(), ec.message().c_str());
            state_ = ServerState::TERMINATE;
            return;
        }
        if (res <= 4) return;
        uint8_t buf[UINT16_MAX];
        socket_->read(buf, res, ec);
        if (ec) {
            if (!isDestroying_.load()) JAMI_WARN("Error when reading socket for %s: %s", repository_.c_str(), ec.message().c_str());
            state_ = ServerState::TERMINATE;
            return;
        }
        pkt = {buf, buf + res};
    } else {
        cachedPkt_ = "";
    }

    unsigned int pkt_len;
    std::stringstream stream;
    stream << std::hex << pkt.substr(0,4);
    stream >> pkt_len;
    if (pkt_len != pkt.size()) {
        if (pkt_len == 0) {
            // FLUSH_PKT
            pkt_len = 4;
        }
        cachedPkt_ = pkt.substr(pkt_len, pkt.size() - pkt_len);
        pkt = pkt.substr(0, pkt_len);
    }

    if (pkt.find(UPLOAD_PACK_CMD) == 4) {
        // NOTE: We do not retrieve version or repo for now
        JAMI_INFO("Upload pack command detected.");
        state_ = ServerState::SEND_REFERENCES_CAPABILITIES;
    } else if (pkt.find(WANT_CMD) == 4) {
        auto content = pkt.substr(5, pkt_len-5);
        auto commit = content.substr(4, 40);
        wantedReference_ = commit;
        JAMI_INFO("Peer want ref: %s", wantedReference_.c_str());
    } else if (pkt.find(HAVE_CMD) == 4) {
        auto content = pkt.substr(5, pkt_len-5);
        auto commit = content.substr(4, 40);
        haveRefs_.emplace_back(commit);
    } else if (pkt == DONE_PKT) {
        JAMI_INFO("Peer negotiation is done. Answering to want order");
        state_ = ServerState::ANSWER_TO_WANT_ORDER;
    } else if (pkt == FLUSH_PKT) {
        // Nothing to do for now
    } else {
        JAMI_WARN("Unkown packet received: %s", pkt.c_str());
    }
}

void
GitServer::Impl::sendReferenceCapabilities()
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
    currentHead_ = git_oid_tostr_s(&commit_id);
    git_repository_free(repo);

    std::stringstream capabilities;
    capabilities << currentHead_ << " HEAD\0side-band side-band-64k shallow no-progress include-tag"s;
    std::string capStr = capabilities.str();

    std::stringstream packet;
    packet << std::setw(4) << std::setfill('0') << std::hex << ((5 + capStr.size()) & 0x0FFFF);
    packet << capStr << "\n";
    packet << "003f" << currentHead_ << " refs/heads/master\n";
    packet << FLUSH_PKT;

    std::error_code ec;
    auto res = socket_->write(reinterpret_cast<const unsigned char*>(packet.str().c_str()), packet.str().size(), ec);
    if (ec) {
        JAMI_WARN("Couldn't send data for %s: %s", repository_.c_str(), ec.message().c_str());
    }
}

void
GitServer::Impl::answerToWantOrder()
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
GitServer::Impl::sendPackData()
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

    while (true) {
        if (std::find(haveRefs_.begin(), haveRefs_.end(), wantedReference_) != haveRefs_.end()) {
            // The peer already have the reference
            break;
        }
        git_oid commit_id;
        if (git_oid_fromstr(&commit_id, wantedReference_.c_str()) < 0) {
            JAMI_WARN("Couldn't open reference %s for %s", wantedReference_.c_str(), repository_.c_str());
            git_repository_free(repo);
            git_packbuilder_free(pb);
            return;
        }
        if (git_packbuilder_insert_commit(pb, &commit_id) != 0) {
            JAMI_WARN("Couldn't open insert commit %s for %s", git_oid_tostr_s(&commit_id), repository_.c_str());
            git_repository_free(repo);
            git_packbuilder_free(pb);
            return;
        }

        // Get next commit to pack
        git_commit* current_commit;
        if (git_commit_lookup(&current_commit, repo, &commit_id) < 0) {
            JAMI_ERR("Could not look up current commit");
            git_packbuilder_free(pb);
            return;
        }
        git_commit* parent_commit;
        if (git_commit_parent(&parent_commit, current_commit, 0) < 0) {
            git_commit_free(current_commit);
            break;
        }
        auto* oid_str = git_oid_tostr_s(git_commit_id(parent_commit));
        if (!oid_str) {
            git_packbuilder_free(pb);
            git_commit_free(parent_commit);
            git_commit_free(current_commit);
            JAMI_ERR("Could not look up current commit");
            return;
        }
        wantedReference_ = oid_str;
        git_commit_free(current_commit);
        git_commit_free(parent_commit);
    }

    git_buf data = {};
    if (git_packbuilder_write_buf(&data, pb) != 0) {
        JAMI_WARN("Couldn't write pack data for %s", repository_.c_str());
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

    git_packbuilder_free(pb);
    git_repository_free(repo);

    socket_->write(reinterpret_cast<const unsigned char*>(FLUSH_PKT), std::strlen(FLUSH_PKT), ec);
    if (ec) {
        JAMI_WARN("Couldn't send data for %s: %s", repository_.c_str(), ec.message().c_str());
    }

    // Clear sent data
    haveRefs_.clear();
    wantedReference_.clear();
}

GitServer::GitServer(const std::string& accountId, const std::string& conversationId, const std::shared_ptr<ChannelSocket>& client)
{
    auto path = fileutils::get_data_dir()+DIR_SEPARATOR_STR+accountId+DIR_SEPARATOR_STR+"conversations"+DIR_SEPARATOR_STR+conversationId;
    pimpl_ = std::make_unique<GitServer::Impl>(conversationId, path, client);
}

GitServer::~GitServer()
{
    stop();
    pimpl_.reset();
    JAMI_INFO("GitServer destroyed");
}

void
GitServer::run()
{
    JAMI_INFO("Running GitServer for %s", pimpl_->repository_.c_str());
    pimpl_->run();
}

void
GitServer::stop()
{
    pimpl_->isDestroying_.exchange(true);
    pimpl_->socket_->shutdown();
}

}