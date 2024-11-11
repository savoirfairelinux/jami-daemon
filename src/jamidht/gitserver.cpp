/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
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
#include "gitserver.h"

#include "fileutils.h"
#include "logger.h"
#include "gittransport.h"
#include "manager.h"
#include <opendht/thread_pool.h>
#include <dhtnet/multiplexed_socket.h>
#include <fmt/compile.h>

#include <charconv>
#include <ctime>
#include <fstream>
#include <git2.h>
#include <iomanip>

using namespace std::string_view_literals;
constexpr auto FLUSH_PKT = "0000"sv;
constexpr auto NAK_PKT = "0008NAK\n"sv;
constexpr auto DONE_CMD = "done\n"sv;
constexpr auto WANT_CMD = "want"sv;
constexpr auto HAVE_CMD = "have"sv;
constexpr auto SERVER_CAPABILITIES
    = " HEAD\0side-band side-band-64k shallow no-progress include-tag"sv;

namespace jami {

class GitServer::Impl
{
public:
    Impl(const std::string& repositoryId,
         const std::string& repository,
         const std::shared_ptr<dhtnet::ChannelSocket>& socket)
        : repositoryId_(repositoryId)
        , repository_(repository)
        , socket_(socket)
    {
        // Check at least if repository is correct
        git_repository* repo;
        if (git_repository_open(&repo, repository_.c_str()) != 0) {
            socket_->shutdown();
            return;
        }
        git_repository_free(repo);

        socket_->setOnRecv([this](const uint8_t* buf, std::size_t len) {
            std::lock_guard lk(destroyMtx_);
            if (isDestroying_)
                return len;
            if (parseOrder(std::string_view((const char*)buf, len)))
                while(parseOrder());
            return len;
        });
    }
    ~Impl() { stop(); }
    void stop()
    {
        std::lock_guard lk(destroyMtx_);
        if (isDestroying_.exchange(true)) {
            socket_->setOnRecv({});
            socket_->shutdown();
        }
    }
    bool parseOrder(std::string_view buf = {});

    void sendReferenceCapabilities(bool sendVersion = false);
    bool NAK();
    void ACKCommon();
    bool ACKFirst();
    void sendPackData();
    std::map<std::string, std::string> getParameters(std::string_view pkt_line);

    std::string repositoryId_ {};
    std::string repository_ {};
    std::shared_ptr<dhtnet::ChannelSocket> socket_ {};
    std::string wantedReference_ {};
    std::string common_ {};
    std::vector<std::string> haveRefs_ {};
    std::string cachedPkt_ {};
    std::mutex destroyMtx_ {};
    std::atomic_bool isDestroying_ {false};
    onFetchedCb onFetchedCb_ {};
};

bool
GitServer::Impl::parseOrder(std::string_view buf)
{
    std::string pkt = std::move(cachedPkt_);
    if (!buf.empty())
        pkt += buf;

    // Parse pkt len
    // Reference: https://github.com/git/git/blob/master/Documentation/technical/protocol-common.txt#L51
    // The first four bytes define the length of the packet and 0000 is a FLUSH pkt

    unsigned int pkt_len = 0;
    auto [p, ec] = std::from_chars(pkt.data(), pkt.data() + 4, pkt_len, 16);
    if (ec != std::errc()) {
        JAMI_ERROR("Unable to parse packet size");
    }
    if (pkt_len != pkt.size()) {
        // Store next packet part
        if (pkt_len == 0) {
            // FLUSH_PKT
            pkt_len = 4;
        }
        cachedPkt_ = pkt.substr(pkt_len);
    }

    auto pack = std::string_view(pkt).substr(4, pkt_len - 4);
    if (pack == DONE_CMD) {
        // Reference:
        // https://github.com/git/git/blob/master/Documentation/technical/pack-protocol.txt#L390 Do
        // not do multi-ack, just send ACK + pack file
        // In case of no common base, send NAK
        JAMI_INFO("Peer negotiation is done. Answering to want order");
        bool sendData;
        if (common_.empty())
            sendData = NAK();
        else
            sendData = ACKFirst();
        if (sendData)
            sendPackData();
        return !cachedPkt_.empty();
    } else if (pack.empty()) {
        if (!haveRefs_.empty()) {
            // Reference:
            // https://github.com/git/git/blob/master/Documentation/technical/pack-protocol.txt#L390
            // Do not do multi-ack, just send ACK + pack file In case of no common base ACK
            ACKCommon();
            NAK();
        }
        return !cachedPkt_.empty();
    }

    auto lim = pack.find(' ');
    auto cmd = pack.substr(0, lim);
    auto dat = (lim < pack.size()) ? pack.substr(lim+1) : std::string_view{};
    if (cmd == UPLOAD_PACK_CMD) {
        // Cf: https://github.com/git/git/blob/master/Documentation/technical/pack-protocol.txt#L166
        // References discovery
        JAMI_INFO("Upload pack command detected.");
        auto version = 1;
        auto parameters = getParameters(dat);
        auto versionIt = parameters.find("version");
        bool sendVersion = false;
        if (versionIt != parameters.end()) {
            try {
                version = std::stoi(versionIt->second);
                sendVersion = true;
            } catch (...) {
                JAMI_WARNING("Invalid version detected: {}", versionIt->second);
            }
        }
        if (version == 1) {
            sendReferenceCapabilities(sendVersion);
        } else {
            JAMI_ERR("That protocol version is not yet supported (version: %u)", version);
        }
    } else if (cmd == WANT_CMD) {
        // Reference:
        // https://github.com/git/git/blob/master/Documentation/technical/pack-protocol.txt#L229
        // TODO can have more want
        wantedReference_ = dat.substr(0, 40);
        JAMI_INFO("Peer want ref: %s", wantedReference_.c_str());
    } else if (cmd == HAVE_CMD) {
        const auto& commit = haveRefs_.emplace_back(dat.substr(0, 40));
        if (common_.empty()) {
            // Detect first common commit
            // Reference:
            // https://github.com/git/git/blob/master/Documentation/technical/pack-protocol.txt#L390
            // TODO do not open repository every time
            git_repository* repo;
            if (git_repository_open(&repo, repository_.c_str()) != 0) {
                JAMI_WARN("Unable to open %s", repository_.c_str());
                return !cachedPkt_.empty();
            }
            GitRepository rep {repo, git_repository_free};
            git_oid commit_id;
            if (git_oid_fromstr(&commit_id, commit.c_str()) == 0) {
                // Reference found
                common_ = commit;
            }
        }
    } else {
        JAMI_WARNING("Unwanted packet received: {}", pkt);
    }
    return !cachedPkt_.empty();
}

std::string
toGitHex(size_t value) {
    return fmt::format(FMT_COMPILE("{:04x}"), value & 0x0FFFF);
}

void
GitServer::Impl::sendReferenceCapabilities(bool sendVersion)
{
    // Get references
    // First, get the HEAD reference
    // https://github.com/git/git/blob/master/Documentation/technical/pack-protocol.txt#L166
    git_repository* repo;
    if (git_repository_open(&repo, repository_.c_str()) != 0) {
        JAMI_WARNING("Unable to open {}", repository_);
        socket_->shutdown();
        return;
    }
    GitRepository rep {repo, git_repository_free};

    // Answer with the version number
    // **** When the client initially connects the server will immediately respond
    // **** with a version number (if "version=1" is sent as an Extra Parameter),
    std::error_code ec;
    if (sendVersion) {
        auto toSend = "000eversion 1\0"sv;
        socket_->write(reinterpret_cast<const unsigned char*>(toSend.data()),
                       toSend.size(),
                       ec);
        if (ec) {
            JAMI_WARNING("Unable to send data for {}: {}", repository_, ec.message());
            socket_->shutdown();
            return;
        }
    }

    git_oid commit_id;
    if (git_reference_name_to_id(&commit_id, rep.get(), "HEAD") < 0) {
        JAMI_ERROR("Unable to get reference for HEAD");
        socket_->shutdown();
        return;
    }
    std::string currentHead = git_oid_tostr_s(&commit_id);

    // Send references
    std::ostringstream packet;
    packet << toGitHex(5 + currentHead.size() + SERVER_CAPABILITIES.size());
    packet << currentHead << SERVER_CAPABILITIES << "\n";

    // Now, add other references
    git_strarray refs;
    if (git_reference_list(&refs, rep.get()) == 0) {
        for (std::size_t i = 0; i < refs.count; ++i) {
            std::string_view ref = refs.strings[i];
            if (git_reference_name_to_id(&commit_id, rep.get(), ref.data()) < 0) {
                JAMI_WARNING("Unable to get reference for {}", ref);
                continue;
            }
            currentHead = git_oid_tostr_s(&commit_id);

            packet << toGitHex(6 /* size + space + \n */ + currentHead.size() + ref.size());
            packet << currentHead << " " << ref << "\n";
        }
    }
    git_strarray_dispose(&refs);

    // And add FLUSH
    packet << FLUSH_PKT;
    auto toSend = packet.str();
    socket_->write(reinterpret_cast<const unsigned char*>(toSend.data()), toSend.size(), ec);
    if (ec) {
        JAMI_WARNING("Unable to send data for {}: {}", repository_, ec.message());
        socket_->shutdown();
    }
}

void
GitServer::Impl::ACKCommon()
{
    std::error_code ec;
    // Ack common base
    if (!common_.empty()) {
        auto toSend = fmt::format(FMT_COMPILE("{:04x}ACK {} continue\n"),
            18 + common_.size() /* size + ACK + space * 2 + continue + \n */, common_);
        socket_->write(reinterpret_cast<const unsigned char*>(toSend.c_str()), toSend.size(), ec);
        if (ec) {
            JAMI_WARNING("Unable to send data for {}: {}", repository_, ec.message());
            socket_->shutdown();
        }
    }
}

bool
GitServer::Impl::ACKFirst()
{
    std::error_code ec;
    // Ack common base
    if (!common_.empty()) {
        auto toSend = fmt::format(FMT_COMPILE("{:04x}ACK {}\n"),
            9 + common_.size() /* size + ACK + space + \n */, common_);
        socket_->write(reinterpret_cast<const unsigned char*>(toSend.c_str()), toSend.size(), ec);
        if (ec) {
            JAMI_WARNING("Unable to send data for {}: {}", repository_, ec.message());
            socket_->shutdown();
            return false;
        }
    }
    return true;
}

bool
GitServer::Impl::NAK()
{
    std::error_code ec;
    // NAK
    socket_->write(reinterpret_cast<const unsigned char*>(NAK_PKT.data()), NAK_PKT.size(), ec);
    if (ec) {
        JAMI_WARNING("Unable to send data for {}: {}", repository_, ec.message());
        socket_->shutdown();
        return false;
    }
    return true;
}

void
GitServer::Impl::sendPackData()
{
    git_repository* repo_ptr;
    if (git_repository_open(&repo_ptr, repository_.c_str()) != 0) {
        JAMI_WARN("Unable to open %s", repository_.c_str());
        return;
    }
    GitRepository repo {repo_ptr, git_repository_free};

    git_packbuilder* pb_ptr;
    if (git_packbuilder_new(&pb_ptr, repo.get()) != 0) {
        JAMI_WARNING("Unable to open packbuilder for {}", repository_);
        return;
    }
    GitPackBuilder pb {pb_ptr, git_packbuilder_free};

    std::string fetched = wantedReference_;
    git_oid oid;
    if (git_oid_fromstr(&oid, fetched.c_str()) < 0) {
        JAMI_ERROR("Unable to get reference for commit {}", fetched);
        return;
    }

    git_revwalk* walker_ptr = nullptr;
    if (git_revwalk_new(&walker_ptr, repo.get()) < 0 || git_revwalk_push(walker_ptr, &oid) < 0) {
        if (walker_ptr)
            git_revwalk_free(walker_ptr);
        return;
    }
    GitRevWalker walker {walker_ptr, git_revwalk_free};
    git_revwalk_sorting(walker.get(), GIT_SORT_TOPOLOGICAL);
    // Add first commit
    std::set<std::string> parents;
    auto haveCommit = false;

    while (!git_revwalk_next(&oid, walker.get())) {
        // log until have refs
        std::string id = git_oid_tostr_s(&oid);
        haveCommit |= std::find(haveRefs_.begin(), haveRefs_.end(), id) != haveRefs_.end();
        auto itParents = std::find(parents.begin(), parents.end(), id);
        if (itParents != parents.end())
            parents.erase(itParents);
        if (haveCommit && parents.size() == 0 /* We are sure that all commits are there */)
            break;
        if (git_packbuilder_insert_commit(pb.get(), &oid) != 0) {
            JAMI_WARN("Unable to open insert commit %s for %s",
                      git_oid_tostr_s(&oid),
                      repository_.c_str());
            return;
        }

        // Get next commit to pack
        git_commit* commit_ptr;
        if (git_commit_lookup(&commit_ptr, repo.get(), &oid) < 0) {
            JAMI_ERR("Unable to look up current commit");
            return;
        }
        GitCommit commit {commit_ptr, git_commit_free};
        auto parentsCount = git_commit_parentcount(commit.get());
        for (unsigned int p = 0; p < parentsCount; ++p) {
            // make sure to explore all branches
            const git_oid* pid = git_commit_parent_id(commit.get(), p);
            if (pid)
                parents.emplace(git_oid_tostr_s(pid));
        }
    }

    git_buf data = {};
    if (git_packbuilder_write_buf(&data, pb.get()) != 0) {
        JAMI_WARN("Unable to write pack data for %s", repository_.c_str());
        return;
    }

    std::size_t sent = 0;
    std::size_t len = data.size;
    std::error_code ec;
    std::vector<uint8_t> toSendData;
    do {
        // cf https://github.com/git/git/blob/master/Documentation/technical/pack-protocol.txt#L166
        // In 'side-band-64k' mode it will send up to 65519 data bytes plus 1 control code, for a
        // total of up to 65520 bytes in a pkt-line.
        std::size_t pkt_size = std::min(static_cast<std::size_t>(65515), len - sent);
        std::string toSendHeader = toGitHex(pkt_size + 5);
        toSendData.clear();
        toSendData.reserve(pkt_size + 5);
        toSendData.insert(toSendData.end(), toSendHeader.begin(), toSendHeader.end());
        toSendData.push_back(0x1);
        toSendData.insert(toSendData.end(), data.ptr + sent, data.ptr + sent + pkt_size);

        socket_->write(reinterpret_cast<const unsigned char*>(toSendData.data()),
                       toSendData.size(),
                       ec);
        if (ec) {
            JAMI_WARNING("Unable to send data for {}: {}", repository_, ec.message());
            git_buf_dispose(&data);
            return;
        }
        sent += pkt_size;
    } while (sent < len);
    git_buf_dispose(&data);
    toSendData = {};

    // And finish by a little FLUSH
    socket_->write(reinterpret_cast<const uint8_t*>(FLUSH_PKT.data()), FLUSH_PKT.size(), ec);
    if (ec) {
        JAMI_WARNING("Unable to send data for {}: {}", repository_, ec.message());
    }

    // Clear sent data
    haveRefs_.clear();
    wantedReference_.clear();
    common_.clear();
    if (onFetchedCb_)
        onFetchedCb_(fetched);
}

std::map<std::string, std::string>
GitServer::Impl::getParameters(std::string_view pkt_line)
{
    std::map<std::string, std::string> parameters;
    std::string key, value;
    auto isKey = true;
    auto nullChar = 0;
    for (auto letter: pkt_line) {
        if (letter == '\0') {
            // parameters such as host or version are after the first \0
            if (nullChar != 0 && !key.empty()) {
                parameters[std::move(key)] = std::move(value);
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
                     const std::shared_ptr<dhtnet::ChannelSocket>& client)
{
    auto path = (fileutils::get_data_dir() / accountId / "conversations" / conversationId).string();
    pimpl_ = std::make_unique<GitServer::Impl>(conversationId, path, client);
}

GitServer::~GitServer()
{
    stop();
    pimpl_.reset();
    JAMI_INFO("GitServer destroyed");
}

void
GitServer::setOnFetched(const onFetchedCb& cb)
{
    if (!pimpl_)
        return;
    pimpl_->onFetchedCb_ = cb;
}

void
GitServer::stop()
{
    pimpl_->stop();
}

} // namespace jami