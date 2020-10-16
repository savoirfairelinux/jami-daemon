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
#include "manager.h"
#include "multiplexed_socket.h"
#include "opendht/thread_pool.h"

#include <ctime>
#include <fstream>
#include <git2.h>
#include <iomanip>

constexpr std::string_view const FLUSH_PKT = "0000";
constexpr std::string_view const NAK_PKT = "0008NAK\n";
constexpr std::string_view const DONE_PKT = "0009done\n";

constexpr std::string_view const WANT_CMD = "want";
constexpr std::string_view const HAVE_CMD = "have";

using namespace std::string_view_literals;

namespace jami {

class GitServer::Impl
{
public:
    Impl(const std::string& repositoryId,
         const std::string& repository,
         const std::shared_ptr<ChannelSocket>& socket)
        : repositoryId_(repositoryId)
        , repository_(repository)
        , socket_(socket)
    {
        socket_->setOnRecv([this](const uint8_t* buf, std::size_t len) {
            if (isDestroying_)
                return len;
            auto needMoreParsing = parseOrder(buf, len);
            while (needMoreParsing) {
                needMoreParsing = parseOrder();
            };
            return len;
        });
    }
    ~Impl() = default;

    bool parseOrder(const uint8_t* buf = nullptr, std::size_t len = 0);

    void sendReferenceCapabilities(bool sendVersion = false);
    void answerToWantOrder();
    void sendPackData();
    std::map<std::string, std::string> getParameters(const std::string& pkt_line);

    std::string repositoryId_ {};
    std::string repository_ {};
    std::shared_ptr<ChannelSocket> socket_ {};
    std::string wantedReference_ {};
    std::vector<std::string> haveRefs_ {};
    std::string cachedPkt_ {};
    std::atomic_bool isDestroying_ {false};
};

bool
GitServer::Impl::parseOrder(const uint8_t* buf, std::size_t len)
{
    std::string pkt = cachedPkt_;
    if (buf)
        pkt += std::string({buf, buf + len});
    cachedPkt_.clear();

    // Parse pkt len
    // Reference: https://github.com/git/git/blob/master/Documentation/technical/protocol-common.txt#L51
    // The first four bytes define the length of the packet and 0000 is a FLUSH pkt

    unsigned int pkt_len;
    std::stringstream stream;
    stream << std::hex << pkt.substr(0, 4);
    stream >> pkt_len;
    if (pkt_len != pkt.size()) {
        // Store next packet part
        if (pkt_len == 0) {
            // FLUSH_PKT
            pkt_len = 4;
        }
        cachedPkt_ = pkt.substr(pkt_len, pkt.size() - pkt_len);
    }
    // NOTE: do not remove the size to detect the 0009done packet
    pkt = pkt.substr(0, pkt_len);

    if (pkt.find(UPLOAD_PACK_CMD) == 4) {
        // Cf: https://github.com/git/git/blob/master/Documentation/technical/pack-protocol.txt#L166
        // References discovery
        JAMI_INFO("Upload pack command detected.");
        auto version = 1;
        auto parameters = getParameters(pkt);
        auto versionIt = parameters.find("version");
        bool sendVersion = false;
        if (versionIt != parameters.end()) {
            try {
                version = std::stoi(versionIt->second);
                sendVersion = true;
            } catch (...) {
                JAMI_WARN("Invalid version detected: %s", versionIt->second.c_str());
            }
        }
        if (version == 1) {
            sendReferenceCapabilities(sendVersion);
        } else {
            JAMI_ERR("That protocol version is not yet supported (version: %u)", version);
        }
    } else if (pkt.find(WANT_CMD) == 4) {
        // Reference:
        // https://github.com/git/git/blob/master/Documentation/technical/pack-protocol.txt#L229
        // TODO can have more want
        auto content = pkt.substr(5, pkt_len - 5);
        auto commit = content.substr(4, 40);
        wantedReference_ = commit;
        JAMI_INFO("Peer want ref: %s", wantedReference_.c_str());
    } else if (pkt.find(HAVE_CMD) == 4) {
        auto content = pkt.substr(5, pkt_len - 5);
        auto commit = content.substr(4, 40);
        haveRefs_.emplace_back(commit);
    } else if (pkt == DONE_PKT) {
        // Reference:
        // https://github.com/git/git/blob/master/Documentation/technical/pack-protocol.txt#L390 Do
        // not do multi-ack, just send ACK + pack file
        // TODO: in case of no common base, send NAK
        JAMI_INFO("Peer negotiation is done. Answering to want order");
        answerToWantOrder();
    } else if (pkt == FLUSH_PKT) {
        // Nothing to do for now
    } else {
        JAMI_WARN("Unwanted packet received: %s", pkt.c_str());
    }
    return !cachedPkt_.empty();
}

void
GitServer::Impl::sendReferenceCapabilities(bool sendVersion)
{
    // Get references
    // First, get the HEAD reference
    // https://github.com/git/git/blob/master/Documentation/technical/pack-protocol.txt#L166
    git_repository* repo;
    if (git_repository_open(&repo, repository_.c_str()) != 0) {
        JAMI_WARN("Couldn't open %s", repository_.c_str());
        return;
    }

    // Answer with the version number
    // **** When the client initially connects the server will immediately respond
    // **** with a version number (if "version=1" is sent as an Extra Parameter),
    std::string currentHead;
    std::error_code ec;
    std::stringstream packet;
    if (sendVersion) {
        packet << "000eversion 1\0";
        socket_->write(reinterpret_cast<const unsigned char*>(packet.str().c_str()),
                       packet.str().size(),
                       ec);
        if (ec) {
            JAMI_WARN("Couldn't send data for %s: %s", repository_.c_str(), ec.message().c_str());
            return;
        }
    }

    git_oid commit_id;
    if (git_reference_name_to_id(&commit_id, repo, "HEAD") < 0) {
        JAMI_ERR("Cannot get reference for HEAD");
        git_repository_free(repo);
        return;
    }
    currentHead = git_oid_tostr_s(&commit_id);

    // Send references
    std::stringstream capabilities;
    capabilities << currentHead
                 << " HEAD\0side-band side-band-64k shallow no-progress include-tag"sv;
    std::string capStr = capabilities.str();

    packet.clear();
    packet << std::setw(4) << std::setfill('0') << std::hex << ((5 + capStr.size()) & 0x0FFFF);
    packet << capStr << "\n";
    auto toSend = packet.str();

    // Now, add other references
    git_strarray refs;
    if (git_reference_list(&refs, repo) == 0) {
        for (int i = 0; i < refs.count; ++i) {
            std::string ref = refs.strings[i];
            if (git_reference_name_to_id(&commit_id, repo, ref.c_str()) < 0) {
                JAMI_WARN("Cannot get reference for %s");
                continue;
            }
            currentHead = git_oid_tostr_s(&commit_id);

            packet.clear();
            packet << std::setw(4) << std::setfill('0') << std::hex
                   << ((6 /* size + space + \n */ + currentHead.size() + ref.size()) & 0x0FFFF);
            packet << currentHead << " " << ref << "\n";
            toSend += packet.str();
        }
    }
    git_repository_free(repo);

    // And add FLUSH
    packet.clear();
    packet << FLUSH_PKT;
    toSend += packet.str();

    // auto toSend = packet.str();
    for (auto i = 0; i < toSend.size(); ++i) {
        if (toSend[i] == '\0') {
            printf("-");
        } else {
            printf("%c", toSend[i]);
        }
    }
    printf("\n");
    socket_->write(reinterpret_cast<const unsigned char*>(toSend.c_str()), toSend.size(), ec);
    if (ec) {
        JAMI_WARN("Couldn't send data for %s: %s", repository_.c_str(), ec.message().c_str());
    }
}

void
GitServer::Impl::answerToWantOrder()
{
    std::error_code ec;
    socket_->write(reinterpret_cast<const unsigned char*>(NAK_PKT.data()), NAK_PKT.size(), ec);
    if (ec) {
        JAMI_WARN("Couldn't send data for %s: %s", repository_.c_str(), ec.message().c_str());
        return;
    }
    sendPackData();
}

void
GitServer::Impl::sendPackData()
{
    git_repository* repo;

    if (git_repository_open(&repo, repository_.c_str()) != 0) {
        JAMI_WARN("Couldn't open %s", repository_.c_str());
        return;
    }

    git_packbuilder* pb;
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
            JAMI_WARN("Couldn't open reference %s for %s",
                      wantedReference_.c_str(),
                      repository_.c_str());
            git_repository_free(repo);
            git_packbuilder_free(pb);
            return;
        }
        if (git_packbuilder_insert_commit(pb, &commit_id) != 0) {
            JAMI_WARN("Couldn't open insert commit %s for %s",
                      git_oid_tostr_s(&commit_id),
                      repository_.c_str());
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

    socket_->write(reinterpret_cast<const unsigned char*>(FLUSH_PKT.data()), FLUSH_PKT.size(), ec);
    if (ec) {
        JAMI_WARN("Couldn't send data for %s: %s", repository_.c_str(), ec.message().c_str());
    }

    // Clear sent data
    haveRefs_.clear();
    wantedReference_.clear();
}

std::map<std::string, std::string>
GitServer::Impl::getParameters(const std::string& pkt_line)
{
    std::map<std::string, std::string> parameters;
    std::string key, value;
    auto isKey = true;
    auto nullChar = 0;
    for (auto i = 0; i < pkt_line.size(); ++i) {
        auto letter = pkt_line[i];
        if (letter == '\0') {
            // parameters such as host or version are after the first \0
            if (nullChar != 0 && !key.empty()) {
                parameters[key] = value;
            }
            nullChar += 1;
            isKey = true;
            key.clear();
            value.clear();
        } else if (letter == '=') {
            isKey = false;
        } else if (nullChar != 0) {
            if (isKey) {
                key += letter;
            } else {
                value += letter;
            }
        }
    }
    return parameters;
}

GitServer::GitServer(const std::string& accountId,
                     const std::string& conversationId,
                     const std::shared_ptr<ChannelSocket>& client)
{
    auto path = fileutils::get_data_dir() + DIR_SEPARATOR_STR + accountId + DIR_SEPARATOR_STR
                + "conversations" + DIR_SEPARATOR_STR + conversationId;
    pimpl_ = std::make_unique<GitServer::Impl>(conversationId, path, client);
}

GitServer::~GitServer()
{
    stop();
    pimpl_.reset();
    JAMI_INFO("GitServer destroyed");
}

void
GitServer::stop()
{
    pimpl_->isDestroying_.exchange(true);
    pimpl_->socket_->setOnRecv({});
    pimpl_->socket_->shutdown();
}

} // namespace jami