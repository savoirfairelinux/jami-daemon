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
#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <algorithm>
#include <git2/remote.h>
#include <git2/sys/transport.h>
#include <git2/errors.h>
#include <git2.h>

namespace jami {
class Manager;
class ChannelSocket;
} // namespace jami

// NOTE: THIS MUST BE IN THE ROOT NAMESPACE FOR LIBGIT2

struct P2PStream
{
    git_smart_subtransport_stream base;
    std::weak_ptr<jami::ChannelSocket> socket;

    std::string cmd {};
    std::string url {};
    unsigned sent_command : 1;
};

struct P2PSubTransport
{
    git_smart_subtransport base;
    std::unique_ptr<P2PStream> stream;
    git_remote* remote;
};

using namespace std::string_view_literals;
constexpr auto UPLOAD_PACK_CMD = "git-upload-pack"sv;
constexpr auto HOST_TAG = "host="sv;

/*
 * Create a git protocol request.
 *
 * For example: 0029git-upload-pack conversation\0host=device\0
 * @param buf       The buffer to fill
 * @param cmd       The wanted command
 * @param url       The repository's URL
 * @return 0 on success, - 1 on error
 */
int generateRequest(git_buf* request, const std::string& cmd, const std::string_view& url);

/**
 * Send a git command on the linked socket
 * @param s     Related stream
 * @return 0 on success
 */
int sendCmd(P2PStream* s);

/**
 * Read on a channel socket
 * @param stream        Related stream
 * @param buffer        Buffer to fill
 * @param buflen        Maximum buffer size
 * @param read          Number of bytes read
 * @return 0 on success
 */
int P2PStreamRead(git_smart_subtransport_stream* stream, char* buffer, size_t buflen, size_t* read);

int P2PStreamWrite(git_smart_subtransport_stream* stream, const char* buffer, size_t len);

/**
 * Free resources used by the stream
 */
void P2PStreamFree(git_smart_subtransport_stream* stream);

/**
 * Handles git actions
 * @param out       Subtransport's stream created or used by the action
 * @param transport Subtransport created or used by the action
 * @param url       'deviceId/conversationId'
 * @param action    Action to perform
 * @return 0 on success
 */
int P2PSubTransportAction(git_smart_subtransport_stream** out,
                          git_smart_subtransport* transport,
                          const char* url,
                          git_smart_service_t action);

/**
 * Close a subtransport
 * Because we use a channel socket, we need to do nothing here.
 * Will be shutdown by the rest of the code
 */
int P2PSubTransportClose(git_smart_subtransport*);

/**
 * Free resources used by a transport
 * @param transport     Transport to free
 */
void P2PSubTransportFree(git_smart_subtransport* transport);

/**
 * Create a new subtransport
 * @param out       The new subtransport
 * @param owner     The transport owning this subtransport
 * @param payload   The remote
 * @return 0 on success
 */
int P2PSubTransportNew(P2PSubTransport** out, git_transport* owner, void* payload);

/**
 * Setup the subtransport callback
 * @param out       Subtransport created
 * @param owner     Transport owning the sub transport
 * @param param     The remote
 * @param 0 on success
 */
int p2p_subtransport_cb(git_smart_subtransport** out, git_transport* owner, void* payload);

/**
 * Setup the transport callback
 * @param out       Transport created
 * @param owner     Remote wanted
 * @param 0 on success
 */
int p2p_transport_cb(git_transport** out, git_remote* owner, void* param);