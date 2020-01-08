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
#include "gittransport.h"

#include "manager.h"
#include "jamidht/multiplexed_socket.h"
#include "jamidht/connectionmanager.h"

using namespace std::string_literals;

// NOTE: THIS MUST BE IN THE ROOT NAMESPACE FOR LIBGIT2

int
generateRequest(git_buf *request, const std::string& cmd, const std::string& url)
{
    if (cmd.empty()) {
		giterr_set_str(GITERR_NET, "empty command");
		return -1;
	}
    // url format = deviceId/conversationId
	auto delim = url.find('/');
	if (delim == std::string::npos) {
		giterr_set_str(GITERR_NET, "malformed URL");
		return -1;
	}

    auto deviceId = url.substr(0,delim);
	auto conversationId = url.substr(delim, url.size());

	auto total = 4 /* 4 bytes for the len len */
               + cmd.size() /* followed by the command */
               + 1 /* space */
               + conversationId.size() /* conversation */
               + 1 /* \0 */
               + strlen(HOST_TAG)
               + deviceId.size() /* device */
               + 1 /* \0 */;

    std::stringstream streamed;
    streamed << std::setw(4) << std::setfill('0') << std::hex << (total & 0x0FFFF) << cmd;
    streamed << " " << conversationId;
    streamed << "\0"s << HOST_TAG << deviceId << "\0"s;
    git_buf_set(request, streamed.str().c_str(), total);
	return 0;
}

int
sendCmd(P2PStream *s)
{
    auto res = 0;
	git_buf request = {};
	if ((res = generateRequest(&request, s->cmd, s->url)) < 0) {
        git_buf_free(&request);
        return res;
    }

    std::error_code ec;
	if ((res = s->socket->write(reinterpret_cast<const unsigned char*>(request.ptr), request.size, ec))) {
        s->sent_command = 1;
        git_buf_free(&request);
        return res;
    }

	s->sent_command = 1;
	return res;
}

int
P2PStreamRead(git_smart_subtransport_stream *stream, char *buffer, size_t buflen, size_t *read)
{
	auto* fs = reinterpret_cast<P2PStream*>(stream);
    if (!fs->socket) {
		giterr_set_str(GITERR_NET, "unavailable socket");
        return -1;
    }

	int res;
    // If it's the first read, we need to send
    // the upload-pack command
	if (!fs->sent_command && (res = sendCmd(fs)) < 0)
		return res;

    std::error_code ec;
    // TODO ChannelSocket needs a blocking read operation
    size_t datalen = fs->socket->waitForData(std::chrono::milliseconds(3600 * 1000 * 24), ec);
    if (datalen > 0)
        *read = fs->socket->read(reinterpret_cast<unsigned char*>(buffer), std::min(datalen, buflen), ec);

	return res;
}

int
P2PStreamWrite(git_smart_subtransport_stream *stream, const char *buffer, size_t len)
{
	auto* fs = reinterpret_cast<P2PStream*>(stream);
    if (!fs->socket) {
		giterr_set_str(GITERR_NET, "unavailable socket");
        return -1;
    }
    std::error_code ec;
    auto written = fs->socket->write(reinterpret_cast<const unsigned char*>(buffer), len, ec);
    if (written < 0) {
		giterr_set_str(GITERR_NET, ec.message().c_str());
        return -1;
    }
	return 0;
}

void
P2PStreamFree(git_smart_subtransport_stream *stream)
{
	delete stream;
}

int
P2PSubTransportAction(git_smart_subtransport_stream **out, git_smart_subtransport *transport,
	const char *url, git_smart_service_t action)
{
	auto* sub = reinterpret_cast<P2PSubTransport*>(transport);
    if (!sub || !sub->remote) {
        JAMI_ERR("Invalid subtransport");
        return -1;
    }

    auto repo = git_remote_owner(sub->remote);
    if (!repo) {
        JAMI_ERR("No repository linked to the transport");
        return -1;
    }

    auto* workdir = git_repository_workdir(repo);
    if (!workdir) {
        JAMI_ERR("No working linked to the repository");
        return -1;
    }
    std::string path = workdir;
    auto delimConv = path.rfind("/conversations");
    if (delimConv == std::string::npos) {
        JAMI_ERR("No conversation id found");
        return -1;
    }
    auto delimAccount = path.rfind('/', delimConv-1);
    if (delimAccount == std::string::npos && delimConv-1-delimAccount == 16) {
        JAMI_ERR("No account id found");
        return -1;
    }
    auto accountId = path.substr(delimAccount+1, delimConv-1-delimAccount);
    std::string gitUrl = url + std::string("git://").size();
    auto delim = gitUrl.find('/');
    if (delim == std::string::npos) {
        JAMI_ERR("Incorrect url %s", gitUrl.c_str());
        return -1;
    }
    auto deviceId = gitUrl.substr(0, delim);
    auto conversationId = gitUrl.substr(delim+1, gitUrl.size());

    if (action == GIT_SERVICE_UPLOADPACK_LS) {
        auto gitSocket = jami::Manager::instance().gitSocket(accountId, deviceId, conversationId);
        if (!gitSocket) {
            JAMI_ERR("Can't find related socket for %s, %s, %s",
                accountId.c_str(), deviceId.c_str(), conversationId.c_str());
            return -1;
        }
        auto* stream = new P2PStream();
        stream->socket = gitSocket;
        stream->base.read = P2PStreamRead;
        stream->base.write = P2PStreamWrite;
        stream->base.free = P2PStreamFree;
        stream->cmd = UPLOAD_PACK_CMD;
        stream->url = url + std::string("git://").size();
        sub->stream = stream;
        *out = &sub->stream->base;
        return 0;
    } else if (action == GIT_SERVICE_UPLOADPACK) {
        if (sub->stream) {
            *out = &sub->stream->base;
            return 0;
        }
        return -1;
    }
    return 0;
}

int
P2PSubTransportClose(git_smart_subtransport*)
{
	return 0;
}

void
P2PSubTransportFree(git_smart_subtransport* transport)
{
	delete transport;
}

int
P2PSubTransportNew(P2PSubTransport **out, git_transport *owner, void *payload)
{
	P2PSubTransport *sub = new P2PSubTransport();
    sub->remote = reinterpret_cast<git_remote*>(payload);
	sub->base.action = P2PSubTransportAction;
	sub->base.close = P2PSubTransportClose;
	sub->base.free = P2PSubTransportFree;

	*out = sub;
	return 0;
}

int
p2p_subtransport_cb(git_smart_subtransport **out, git_transport *owner, void *payload)
{
	P2PSubTransport *sub;

	if (P2PSubTransportNew(&sub, owner, payload) < 0)
		return -1;

	*out = &sub->base;
	return 0;
}

int
p2p_transport_cb(git_transport **out, git_remote *owner, void *param)
{
	git_smart_subtransport_definition def = {
		p2p_subtransport_cb,
		0, /* Because we use an already existing channel socket, we use a permanent transport */
		reinterpret_cast<void*>(owner)
	};
	return git_transport_smart(out, owner, &def);
}