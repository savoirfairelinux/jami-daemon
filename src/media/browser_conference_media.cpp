#include "browser_conference_media.h"

#include "conference.h"
#include "jamidht/jamiaccount.h"
#include "manager.h"
#include "media/audio/audio_rtp_session.h"
#include "media/audio/ringbufferpool.h"
#include "media/dtls_srtp.h"
#include "media/socket_pair.h"
#include "sip/sipvoiplink.h"
#ifdef ENABLE_VIDEO
#include "media/video/video_rtp_session.h"
#endif
#include "connectivity/sip_utils.h"

#include <dhtnet/ice_socket.h>
#include <dhtnet/ice_transport_factory.h>
#include <dhtnet/ip_utils.h>
#include <opendht/thread_pool.h>

namespace jami {

namespace {
constexpr auto ICE_TIMEOUT = std::chrono::seconds(35);
constexpr auto MEDIA_TIMEOUT = std::chrono::seconds(45);

std::string
serializeBrowserAnswer(Sdp& sdp)
{
    auto* negotiated = sdp.getActiveLocalSdpSession();
    auto* offered = sdp.getLocalSdpSession();
    if (!negotiated || !offered || negotiated->media_count != offered->media_count)
        return {};

    sip_utils::PoolPtr pool(pj_pool_create(&Manager::instance().sipVoIPLink().getCachingPool()->factory,
                                           "browserAnswer",
                                           4096,
                                           4096,
                                           nullptr));
    if (!pool)
        return {};
    auto* answer = pjmedia_sdp_session_clone(pool.get(), offered);
    if (!answer)
        return {};

    for (unsigned i = 0; i < answer->media_count; ++i) {
        auto* media = answer->media[i];
        const auto* active = negotiated->media[i];
        media->desc.fmt_count = active->desc.fmt_count;
        for (unsigned j = 0; j < active->desc.fmt_count; ++j)
            pj_strdup(pool.get(), &media->desc.fmt[j], &active->desc.fmt[j]);

        unsigned retained = 0;
        for (unsigned j = 0; j < media->attr_count; ++j) {
            auto* attr = media->attr[j];
            if (pj_stricmp2(&attr->name, "sendrecv") == 0 || pj_stricmp2(&attr->name, "sendonly") == 0
                || pj_stricmp2(&attr->name, "recvonly") == 0 || pj_stricmp2(&attr->name, "inactive") == 0)
                continue;
            if (pj_stricmp2(&attr->name, "rtpmap") == 0 || pj_stricmp2(&attr->name, "fmtp") == 0
                || pj_stricmp2(&attr->name, "rtcp-fb") == 0) {
                auto payload = std::string_view(attr->value.ptr, attr->value.slen);
                payload = payload.substr(0, payload.find(' '));
                if (payload != "*") {
                    bool selected = false;
                    for (unsigned k = 0; k < media->desc.fmt_count; ++k) {
                        if (payload == sip_utils::as_view(media->desc.fmt[k])) {
                            selected = true;
                            break;
                        }
                    }
                    if (!selected)
                        continue;
                }
            }
            media->attr[retained++] = attr;
        }
        media->attr_count = retained;
        for (const char* direction : {"sendonly", "recvonly", "inactive"}) {
            if (pjmedia_sdp_attr_find2(active->attr_count, active->attr, direction, nullptr)) {
                if (pjmedia_sdp_media_add_attr(media, pjmedia_sdp_attr_create(pool.get(), direction, nullptr))
                    != PJ_SUCCESS)
                    return {};
                break;
            }
        }
    }
    return pjmedia_sdp_validate(answer) == PJ_SUCCESS ? Sdp::toString(answer) : std::string {};
}
} // namespace

BrowserConferenceMedia::BrowserConferenceMedia(std::shared_ptr<JamiAccount> account,
                                               std::shared_ptr<Conference> conference,
                                               std::optional<dhtnet::IceTransportOptions> iceOptions)
    : account_(std::move(account))
    , conference_(std::move(conference))
    , iceOptions_(std::move(iceOptions))
    , timeout_(*Manager::instance().ioContext())
{}

BrowserConferenceMedia::~BrowserConferenceMedia()
{
    stop();
}

void
BrowserConferenceMedia::start(std::string offer, AnswerCb onAnswer, FailureCb onFailure, ReadyCb onReady)
{
    {
        std::lock_guard lock(mutex_);
        if (state_ != State::IDLE) {
            if (onFailure)
                runOnMainThread([cb = std::move(onFailure)] { cb("Browser conference media already started"); });
            return;
        }
        state_ = State::GATHERING;
        onAnswer_ = std::move(onAnswer);
        onFailure_ = std::move(onFailure);
        onReady_ = std::move(onReady);
    }
    runOnMainThread([w = weak_from_this(), offer = std::move(offer)]() mutable {
        if (auto self = w.lock())
            self->initialize(std::move(offer));
    });
}

void
BrowserConferenceMedia::initialize(std::string offer)
{
    {
        std::lock_guard lock(mutex_);
        if (state_ != State::GATHERING)
            return;
    }
    if (!account_ || !conference_ || conference_->getAccount() != account_) {
        fail("Conference and Jami account must belong to the same account");
        return;
    }

    try {
        sdp_ = std::make_unique<Sdp>(conference_->getConfId() + "-browser");
        if (!sdp_->setReceivedOfferFromExternalSdp(offer))
            throw std::runtime_error("Invalid browser SDP offer");
        const auto* remote = sdp_->getRemoteSdpSession();
        if (!remote || remote->media_count < 1 || remote->media_count > 2)
            throw std::runtime_error("Expected browser audio and optional video");
        if (pj_stricmp2(&remote->media[0]->desc.media, "audio"))
            throw std::runtime_error(
                "Browser conference offer requires an audio m-line first; video-first offers are unsupported");
        for (unsigned i = 0; i < remote->media_count; ++i) {
            const auto* line = remote->media[i];
            const char* expected = i == 0 ? "audio" : "video";
            if (pj_stricmp2(&line->desc.media, expected) || line->desc.port == 0
                || !pjmedia_sdp_attr_find2(line->attr_count, line->attr, "rtcp-mux", nullptr))
                throw std::runtime_error("Browser offer requires enabled audio/video with rtcp-mux");
#ifndef ENABLE_VIDEO
            if (i == 1)
                throw std::runtime_error("Video is unavailable in this daemon");
#endif
            media_.emplace_back(i == 0 ? MEDIA_AUDIO : MEDIA_VIDEO,
                                false,
                                true,
                                true,
                                "",
                                i == 0 ? sip_utils::DEFAULT_AUDIO_STREAMID : sip_utils::DEFAULT_VIDEO_STREAMID);
        }
        if (remote->media_count > 1) {
            const auto* group = pjmedia_sdp_attr_find2(remote->attr_count, remote->attr, "group", nullptr);
            if (!group || std::string_view(group->value.ptr, group->value.slen).substr(0, 7) != "BUNDLE ")
                throw std::runtime_error("Audio and video require BUNDLE");
            const std::string_view groupValue(group->value.ptr, group->value.slen);
            std::string firstMid;
            for (unsigned i = 0; i < remote->media_count; ++i) {
                const auto* line = remote->media[i];
                const auto* mid = pjmedia_sdp_attr_find2(line->attr_count, line->attr, "mid", nullptr);
                if (!mid || mid->value.slen == 0)
                    throw std::runtime_error("Bundled browser media requires a MID on every stream");
                std::string_view value(mid->value.ptr, mid->value.slen);
                if (i == 0)
                    firstMid = value;
                else if (value == firstMid)
                    throw std::runtime_error("Bundled browser streams need distinct MIDs");
                if ((" " + std::string(groupValue.substr(7)) + " ").find(" " + std::string(value) + " ")
                    == std::string::npos)
                    throw std::runtime_error("Browser BUNDLE group does not include all media");
            }
        }
        const auto remoteIce = sdp_->getIceAttributes();
        const auto* audioLine = remote->media[0];
        bool hasAudioCandidate = false;
        for (unsigned i = 0; i < audioLine->attr_count; ++i) {
            if (pj_stricmp2(&audioLine->attr[i]->name, "candidate") == 0) {
                hasAudioCandidate = true;
                break;
            }
        }
        if (remoteIce.ufrag.empty() || remoteIce.pwd.empty() || !hasAudioCandidate)
            throw std::runtime_error("Browser offer must contain gathered ICE candidates and credentials");

        auto identity = generateDtlsSrtpIdentity();
        privateKey_ = std::move(identity.first);
        certificate_ = std::move(identity.second);
        if (!privateKey_ || !certificate_)
            throw std::runtime_error("Cannot create ephemeral DTLS-SRTP identity");
        sdp_->setSecureMediaKeyExchange(KeyExchangeProtocol::DTLS);
        sdp_->setLocalDtlsFingerprint("SHA-256", getDtlsFingerprint(*certificate_));
        sdp_->enableRtcpMux(true);
        sdp_->enableBundle(true);
        sdp_->setLocalMediaCapabilities(MEDIA_AUDIO, account_->getActiveAccountCodecInfoList(MEDIA_AUDIO));
#ifdef ENABLE_VIDEO
        if (media_.size() > 1) {
            if (!conference_->getVideoMixer())
                throw std::runtime_error("Conference video mixer is unavailable");
            sdp_->setLocalMediaCapabilities(MEDIA_VIDEO, account_->getActiveAccountCodecInfoList(MEDIA_VIDEO));
        }
#endif
        auto options = iceOptions_ ? *iceOptions_ : account_->getIceOptions();
        options.accountPublicAddr = account_->getPublishedIpAddress();
        options.accountLocalAddr = dhtnet::ip_utils::getInterfaceAddr(account_->getLocalInterface(),
                                                                      options.accountPublicAddr
                                                                          ? options.accountPublicAddr.getFamily()
                                                                          : AF_INET);
        if (!options.accountLocalAddr)
            options.accountLocalAddr = dhtnet::ip_utils::getInterfaceAddr(account_->getLocalInterface(), AF_INET6);
        if (!options.accountLocalAddr)
            throw std::runtime_error("No usable local address for browser ICE");
        if (!options.accountPublicAddr)
            options.accountPublicAddr = options.accountLocalAddr;
        options.master = false;
        options.streamsCount = 1;
        options.compCountPerStream = 1;
        options.qosType = {media_.size() > 1 ? dhtnet::QosType::VIDEO : dhtnet::QosType::VOICE};
        auto onInitDone = std::move(options.onInitDone);
        auto onNegoDone = std::move(options.onNegoDone);
        options.onInitDone = [w = weak_from_this(), cb = std::move(onInitDone)](bool ok) {
            runOnMainThread([w, cb, ok] {
                if (cb)
                    cb(ok);
                if (auto self = w.lock())
                    self->onIceInitialized(ok);
            });
        };
        options.onNegoDone = [w = weak_from_this(), cb = std::move(onNegoDone)](bool ok) {
            runOnMainThread([w, cb, ok] {
                if (cb)
                    cb(ok);
                if (auto self = w.lock())
                    self->onIceNegotiated(ok);
            });
        };
        ice_ = Manager::instance().getIceTransportFactory()->createTransport(conference_->getConfId() + "-browser");
        if (!ice_)
            throw std::runtime_error("Cannot create browser ICE transport");
        ice_->setOnShutdown([w = weak_from_this()] {
            runOnMainThread([w] {
                if (auto self = w.lock())
                    self->fail("Browser ICE transport disconnected");
            });
        });
        armTimeout(ICE_TIMEOUT, "Browser ICE gathering timed out");
        ice_->initIceInstance(options);
    } catch (const std::exception& e) {
        fail(e.what());
    }
}

void
BrowserConferenceMedia::armTimeout(std::chrono::seconds duration, std::string reason)
{
    std::lock_guard lock(mutex_);
    timeout_.expires_after(duration);
    timeout_.async_wait([w = weak_from_this(), reason = std::move(reason)](const std::error_code& ec) {
        if (!ec)
            if (auto self = w.lock())
                self->fail(reason);
    });
}

void
BrowserConferenceMedia::onIceInitialized(bool ok)
{
    {
        std::lock_guard lock(mutex_);
        if (state_ != State::GATHERING)
            return;
    }
    if (!ok || !ice_ || !ice_->isInitialized()) {
        fail("Browser ICE gathering failed");
        return;
    }
    try {
        const auto candidates = ice_->getLocalCandidates(0, 1);
        dhtnet::IpAddr address;
        // getLocalAddress(1) is the selected ICE pair and is empty until
        // negotiation; the SDP answer needs a gathered host address now.
        for (const auto& line : candidates) {
            dhtnet::IceCandidate candidate;
            if (ice_->parseIceAttributeLine(0, line, candidate) && candidate.comp_id == 1
                && candidate.type == PJ_ICE_CAND_TYPE_HOST && candidate.transport == PJ_CAND_UDP) {
                address = dhtnet::IpAddr(candidate.addr);
                break;
            }
        }
        if (!address || candidates.empty())
            throw std::runtime_error("ICE gathering produced no usable RTP candidate");
        sdp_->setPublishedIP(address);
        sdp_->setLocalPublishedAudioPorts(address.getPort(), 0);
        if (media_.size() > 1)
            sdp_->setLocalPublishedVideoPorts(address.getPort(), 0);
        // processIncomingOffer creates the local session and m-lines required
        // by addIceAttributes/addIceCandidates.
        if (!sdp_->processIncomingOffer(media_))
            throw std::runtime_error("Browser SDP offer has no compatible codecs");
        sdp_->addIceAttributes(ice_->getLocalAttributes());
        for (unsigned i = 0; i < media_.size(); ++i)
            sdp_->addIceCandidates(i, candidates);
        if (!sdp_->startNegotiation())
            throw std::runtime_error("Browser SDP negotiation failed");
        const auto slots = sdp_->getMediaSlots();
        if (slots.size() != media_.size())
            throw std::runtime_error("Browser SDP negotiation changed media slots");
        for (unsigned i = 0; i < slots.size(); ++i) {
            const auto& [local, remote] = slots[i];
            const auto localSends = local.direction_ == MediaDirection::SENDRECV
                                    || local.direction_ == MediaDirection::SENDONLY;
            const auto localReceives = local.direction_ == MediaDirection::SENDRECV
                                       || local.direction_ == MediaDirection::RECVONLY;
            const auto remoteSends = remote.direction_ == MediaDirection::SENDRECV
                                     || remote.direction_ == MediaDirection::SENDONLY;
            const auto remoteReceives = remote.direction_ == MediaDirection::SENDRECV
                                        || remote.direction_ == MediaDirection::RECVONLY;
            if (!local.enabled || !remote.enabled || !local.codec || !remote.codec || local.type != media_[i].type_
                || remote.type != local.type || local.codec->name != remote.codec->name
                || (!localSends && !localReceives) || localSends != remoteReceives || localReceives != remoteSends
                || !local.rtcp_mux || !remote.rtcp_mux || local.key_exchange != KeyExchangeProtocol::DTLS
                || remote.key_exchange != KeyExchangeProtocol::DTLS || local.dtls_fingerprint.empty()
                || remote.dtls_fingerprint.empty())
                throw std::runtime_error("Browser media requires a compatible DTLS-SRTP codec and rtcp-mux");
            if (i
                && (remote.dtls_fingerprint_type != slots[0].second.dtls_fingerprint_type
                    || remote.dtls_fingerprint != slots[0].second.dtls_fingerprint
                    || local.dtls_setup != slots[0].first.dtls_setup))
                throw std::runtime_error("Bundled browser media must share one DTLS identity and role");
        }
        auto answer = serializeBrowserAnswer(*sdp_);
        if (answer.empty())
            throw std::runtime_error("Cannot serialize browser SDP answer");

        AnswerCb onAnswer;
        {
            std::lock_guard lock(mutex_);
            if (state_ != State::GATHERING)
                return;
            state_ = State::ANSWERED;
            onAnswer = std::move(onAnswer_);
        }
        armTimeout(ICE_TIMEOUT, "Browser ICE negotiation timed out");
        if (onAnswer)
            onAnswer(answer);

        {
            std::lock_guard lock(mutex_);
            if (state_ != State::ANSWERED)
                return;
        }
        auto attributes = sdp_->getIceAttributes();
        std::vector<dhtnet::IceCandidate> remoteCandidates;
        for (const auto& line : sdp_->getIceCandidates(0)) {
            dhtnet::IceCandidate candidate;
            if (ice_->parseIceAttributeLine(0, line, candidate))
                remoteCandidates.emplace_back(std::move(candidate));
        }
        if (remoteCandidates.empty() || !ice_->startIce(attributes, std::move(remoteCandidates)))
            throw std::runtime_error("Cannot start browser ICE negotiation");
    } catch (const std::exception& e) {
        fail(e.what());
    }
}

void
BrowserConferenceMedia::onIceNegotiated(bool ok)
{
    {
        std::lock_guard lock(mutex_);
        if (state_ != State::ANSWERED)
            return;
        state_ = State::STARTING;
    }
    if (!ok || !ice_ || !ice_->isRunning()) {
        fail("Browser ICE negotiation failed");
        return;
    }
    try {
        const auto slots = sdp_->getMediaSlots();
        const auto streamId = conference_->getConfId() + "-browser-audio";
        audio_ = std::make_shared<AudioRtpSession>(conference_->getConfId(),
                                                   streamId,
                                                   nullptr,
                                                   AudioRtpSession::Mode::CONFERENCE_HOST);
        audioBuffer_ = Manager::instance().getRingBufferPool().getRingBuffer(streamId);
        if (!audioBuffer_ || !conference_->attachExternalHostAudio(streamId, slots.size() > 1))
            throw std::runtime_error("Cannot attach browser audio to conference");
        attached_ = true;
        audio_->setDtlsSrtpIdentity(certificate_, privateKey_);
        audio_->setMtu(1200);
        audio_->updateMedia(slots[0].second, slots[0].first);
        audio_->setStartupFailureCb([w = weak_from_this()](MediaType, const std::string& reason) {
            runOnMainThread([w, reason] {
                if (auto self = w.lock())
                    self->fail(reason);
            });
        });
#ifdef ENABLE_VIDEO
        if (slots.size() > 1) {
            video_ = std::make_shared<video::VideoRtpSession>(conference_->getConfId(),
                                                              conference_->getConfId() + "-browser-video",
                                                              DeviceParams {},
                                                              nullptr);
            video_->setDtlsSrtpIdentity(certificate_, privateKey_);
            video_->setMtu(1200);
            video_->enterConference(*conference_);
            video_->updateMedia(slots[1].second, slots[1].first);
            video_->setStartupFailureCb([w = weak_from_this()](MediaType, const std::string& reason) {
                runOnMainThread([w, reason] {
                    if (auto self = w.lock())
                        self->fail(reason);
                });
            });
        }
#endif
        armTimeout(MEDIA_TIMEOUT, "Browser RTP startup could not be positively verified");
        dht::ThreadPool::io().run([w = weak_from_this()] {
            if (auto self = w.lock())
                self->startMedia();
        });
    } catch (const std::exception& e) {
        fail(e.what());
    }
}

void
BrowserConferenceMedia::startMedia()
{
    std::lock_guard startLock(mediaStartMtx_);
    std::shared_ptr<AudioRtpSession> audio;
    std::shared_ptr<video::VideoRtpSession> video;
    std::shared_ptr<dhtnet::IceTransport> ice;
    {
        std::lock_guard lock(mutex_);
        if (state_ != State::STARTING)
            return;
        audio = audio_;
        video = video_;
        ice = ice_;
    }
    try {
        auto bundle = SocketPair::createBundleContext(std::make_unique<dhtnet::IceSocket>(ice, 1), nullptr, true, false);
        const auto slots = sdp_->getMediaSlots();
        if (slots.empty())
            throw std::runtime_error("No negotiated browser media for DTLS-SRTP");
        SocketPair::ensureBundleDtlsContext(bundle,
                                            slots[0].first.dtls_setup,
                                            slots[0].second.dtls_fingerprint_type,
                                            slots[0].second.dtls_fingerprint,
                                            certificate_,
                                            privateKey_,
                                            dtlsAbort_);
        {
            std::lock_guard lock(mutex_);
            if (state_ != State::STARTING)
                return;
        }
        audio->setBundleSocketContext(bundle, audio->getSendPayloadType());
        audio->start(nullptr, nullptr);
        if (!audio->isRtpReady(slots[0].second.enabled && !slots[0].second.hold,
                               slots[0].first.enabled && !slots[0].first.hold))
            throw std::runtime_error("Browser audio RTP startup failed");
        {
            std::lock_guard lock(mutex_);
            if (state_ != State::STARTING)
                return;
        }
        if (video) {
#ifdef ENABLE_VIDEO
            video->setBundleSocketContext(bundle, video->getSendPayloadType());
            video->start(nullptr, nullptr);
            if (!video->isRtpReady(slots[1].second.enabled && !slots[1].second.hold,
                                   slots[1].first.enabled && !slots[1].first.hold))
                throw std::runtime_error("Browser video RTP startup failed");
#endif
        }
        runOnMainThread([w = weak_from_this()] {
            if (auto self = w.lock()) {
                ReadyCb onReady;
                {
                    std::lock_guard lock(self->mutex_);
                    if (self->state_ != State::STARTING || self->stopRequested_.load())
                        return;
                    if (self->onReady_) {
                        self->state_ = State::RUNNING;
                        self->timeout_.cancel();
                        onReady = std::move(self->onReady_);
                    }
                }
                if (!onReady) {
                    self->fail("Browser RTP started without a readiness callback");
                    return;
                }
                onReady();
            }
        });
    } catch (const std::exception& e) {
        runOnMainThread([w = weak_from_this(), reason = std::string(e.what())] {
            if (auto self = w.lock())
                self->fail(reason);
        });
    }
}

void
BrowserConferenceMedia::fail(std::string reason)
{
    close(!stopRequested_.load(), std::move(reason));
}

void
BrowserConferenceMedia::stop()
{
    stopRequested_.store(true);
    if (Manager::instance().ioContext()->get_executor().running_in_this_thread()) {
        close(false);
    } else if (auto self = weak_from_this().lock()) {
        runOnMainThread([self = std::move(self)] { self->close(false); });
    } else {
        close(false);
    }
}

void
BrowserConferenceMedia::close(bool notify, std::string reason)
{
    std::shared_ptr<AudioRtpSession> audio;
    std::shared_ptr<video::VideoRtpSession> video;
    std::shared_ptr<dhtnet::IceTransport> ice;
    std::shared_ptr<Conference> conference;
    FailureCb onFailure;
    bool attached = false;
    {
        std::lock_guard lock(mutex_);
        if (state_ == State::CLOSED)
            return;
        state_ = State::CLOSED;
        dtlsAbort_->store(true);
        timeout_.cancel();
        audio = std::move(audio_);
        video = std::move(video_);
        ice = std::move(ice_);
        conference = std::move(conference_);
        attached = attached_;
        attached_ = false;
        if (notify && !stopRequested_.load())
            onFailure = std::move(onFailure_);
        onAnswer_ = {};
        onFailure_ = {};
        onReady_ = {};
    }
    if (ice)
        ice->cancelOperations();
    if (video) {
#ifdef ENABLE_VIDEO
        video->stop();
#endif
    }
    if (audio)
        audio->stop();
    {
        std::lock_guard startLock(mediaStartMtx_);
        if (video) {
#ifdef ENABLE_VIDEO
            video->stop();
            video->exitConference();
#endif
        }
        if (audio)
            audio->stop();
    }
    if (attached && conference)
        conference->detachHost();
    audioBuffer_.reset();
    if (onFailure)
        onFailure(reason);
}

} // namespace jami
