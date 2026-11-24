#include "circle_fetch.h"

#include "git_def.h"
#include "git_utils.h"
#include "gittransport.h"

#include <dhtnet/channel_socket.h>
#include <fmt/format.h>
#include <git2/sys/errors.h>
#include <git2/sys/transport.h>

#include <chrono>

namespace jami {
namespace {

constexpr auto READ_TIMEOUT = std::chrono::minutes(1);

struct Context
{
    // Keeps the channel alive for the stream, which only holds a weak reference.
    std::shared_ptr<dhtnet::ChannelSocketInterface> socket;
    std::string url;
};

struct SubTransport
{
    git_smart_subtransport base {};
    Context* context {nullptr};
    std::unique_ptr<P2PStream> stream;
};

int
action(git_smart_subtransport_stream** out, git_smart_subtransport* transport, const char*, git_smart_service_t service)
{
    auto* self = reinterpret_cast<SubTransport*>(transport);
    if (service == GIT_SERVICE_UPLOADPACK_LS) {
        self->stream = std::make_unique<P2PStream>();
        self->stream->base.subtransport = transport;
        self->stream->base.read = P2PStreamRead;
        self->stream->base.write = P2PStreamWrite;
        self->stream->base.free = P2PStreamFree;
        self->stream->socket = self->context->socket;
        self->stream->cmd = UPLOAD_PACK_CMD;
        self->stream->url = self->context->url;
        self->stream->timeout = READ_TIMEOUT;
    } else if (service != GIT_SERVICE_UPLOADPACK || !self->stream) {
        git_error_set_str(GIT_ERROR_NET, "unsupported circle transport service");
        return -1;
    }
    *out = &self->stream->base;
    return 0;
}

int
close(git_smart_subtransport*)
{
    return 0;
}

void
release(git_smart_subtransport* transport)
{
    delete reinterpret_cast<SubTransport*>(transport);
}

int
subtransport(git_smart_subtransport** out, git_transport*, void* param)
{
    auto* self = new SubTransport;
    self->base.action = action;
    self->base.close = close;
    self->base.free = release;
    self->context = static_cast<Context*>(param);
    *out = &self->base;
    return 0;
}

int
transport(git_transport** out, git_remote* owner, void* payload)
{
    // A permanent (non-RPC) transport, since the channel is already established.
    git_smart_subtransport_definition definition {subtransport, 0, payload};
    return git_transport_smart(out, owner, &definition);
}

} // namespace

std::optional<std::string>
downloadCircleRoute(const std::filesystem::path& repository,
                    std::string_view device,
                    std::string_view route,
                    const std::shared_ptr<dhtnet::ChannelSocketInterface>& socket,
                    std::string_view ref)
{
    if (!socket)
        return std::nullopt;
    Context context {socket, fmt::format("{}/{}", device, route)};
    git_repository* raw = nullptr;
    if (git_repository_open_bare(&raw, repository.string().c_str()) < 0)
        return std::nullopt;
    GitRepository repo(raw);
    git_remote* remoteRaw = nullptr;
    const auto url = fmt::format("git://{}", context.url);
    if (git_remote_create_anonymous(&remoteRaw, repo.get(), url.c_str()) < 0)
        return std::nullopt;
    GitRemote remote(remoteRaw);
    auto spec = fmt::format("+refs/heads/main:{}", ref);
    char* specs[] {spec.data()};
    const git_strarray refspecs {specs, 1};
    git_fetch_options options;
    git_fetch_options_init(&options, GIT_FETCH_OPTIONS_VERSION);
    options.callbacks.transport = transport;
    options.callbacks.transfer_progress = limitFetchSize;
    options.callbacks.payload = &context;
    options.update_fetchhead = 0;
    options.download_tags = GIT_REMOTE_DOWNLOAD_TAGS_NONE;
    options.follow_redirects = GIT_REMOTE_REDIRECT_NONE;
    git_oid head {};
    if (git_remote_fetch(remote.get(), &refspecs, &options, "Fetch circle") < 0
        || git_reference_name_to_id(&head, repo.get(), std::string(ref).c_str()) < 0)
        return std::nullopt;
    return git::oidString(&head);
}

} // namespace jami
