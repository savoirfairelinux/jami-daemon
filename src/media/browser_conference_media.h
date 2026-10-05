#pragma once

#include "media/media_attribute.h"
#include "sip/sdp.h"

#include <dhtnet/ice_options.h>
#include <asio/steady_timer.hpp>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <functional>
#include <string>
#include <vector>

namespace dhtnet {
class IceTransport;
}

namespace dht {
namespace crypto {
struct Certificate;
struct PrivateKey;
} // namespace crypto
} // namespace dht

namespace jami {

class JamiAccount;
class Conference;
class AudioRtpSession;
class RingBuffer;
namespace video {
class VideoRtpSession;
}

class BrowserConferenceMedia final : public std::enable_shared_from_this<BrowserConferenceMedia>
{
public:
    using AnswerCb = std::function<void(const std::string&)>;
    using FailureCb = std::function<void(const std::string&)>;
    using ReadyCb = std::function<void()>;

    // Optional ICE options allow a host-only transport in integration tests;
    // production uses the account's own ICE configuration.
    BrowserConferenceMedia(std::shared_ptr<JamiAccount> account,
                           std::shared_ptr<Conference> conference,
                           std::optional<dhtnet::IceTransportOptions> iceOptions = std::nullopt);
    ~BrowserConferenceMedia();

    // AnswerCb reports a fully gathered SDP answer, not ICE/media readiness.
    // The caller must retain this owner until stop() or a failure callback.
    // ReadyCb reports completed ICE, DTLS-SRTP, and negotiated RTP startup.
    void start(std::string offer, AnswerCb onAnswer, FailureCb onFailure, ReadyCb onReady = {});
    void stop();

private:
    enum class State { IDLE, GATHERING, ANSWERED, STARTING, RUNNING, CLOSED };
    void initialize(std::string offer);
    void onIceInitialized(bool ok);
    void onIceNegotiated(bool ok);
    void startMedia();
    void fail(std::string reason);
    void close(bool notify, std::string reason = {});
    void armTimeout(std::chrono::seconds duration, std::string reason);

    std::mutex mutex_;
    std::mutex mediaStartMtx_;
    const std::shared_ptr<std::atomic_bool> dtlsAbort_ {std::make_shared<std::atomic_bool>(false)};
    std::atomic_bool stopRequested_ {false};
    State state_ {State::IDLE};
    std::shared_ptr<JamiAccount> account_;
    std::shared_ptr<Conference> conference_;
    std::optional<dhtnet::IceTransportOptions> iceOptions_;
    std::unique_ptr<Sdp> sdp_;
    std::shared_ptr<dhtnet::IceTransport> ice_;
    std::shared_ptr<dht::crypto::Certificate> certificate_;
    std::shared_ptr<dht::crypto::PrivateKey> privateKey_;
    std::shared_ptr<AudioRtpSession> audio_;
    std::shared_ptr<video::VideoRtpSession> video_;
    std::shared_ptr<RingBuffer> audioBuffer_;
    std::vector<MediaAttribute> media_;
    AnswerCb onAnswer_;
    FailureCb onFailure_;
    ReadyCb onReady_;
    asio::steady_timer timeout_;
    bool attached_ {false};
};

} // namespace jami
