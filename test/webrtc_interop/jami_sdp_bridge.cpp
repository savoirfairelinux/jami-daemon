#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

#include <dhtnet/ice_transport.h>
#include <dhtnet/ice_transport_factory.h>
#include <dhtnet/ip_utils.h>

#include "media/libav_deps.h"

#include "account_const.h"
#include "base64.h"
#include "jami.h"
#include "jami/media_const.h"
#include "manager.h"
#include "media/dtls_srtp.h"
#include "media/media_io_handle.h"
#include "media/socket_pair.h"
#include "media/transport_cc_controller.h"
#include "sip/sdp.h"
#include "sip/sipaccount.h"
#include "sip/sipvoiplink.h"

#include <opendht/crypto.h>

extern "C" {
#include <pjmedia/sdp.h>
}

using namespace libjami::Account;
using namespace jami;

namespace {

constexpr char TEST_ACCOUNT_ALIAS[] = "WERIFT_INTEROP";
constexpr std::chrono::seconds ICE_INIT_TIMEOUT {5};
constexpr std::chrono::seconds ICE_NEGO_TIMEOUT {15};
constexpr std::chrono::seconds REMOTE_PACKET_TIMEOUT {3};
constexpr uint16_t SDP_ONLY_PORT = 4000;
constexpr uint32_t TEST_AUDIO_SSRC = 0x12345678;
constexpr uint16_t TEST_AUDIO_PAYLOAD_TYPE = 111;
constexpr uint16_t TEST_AUDIO_SEQUENCE = 32000;
constexpr uint32_t TEST_AUDIO_TIMESTAMP = 96000;
constexpr uint64_t TEST_INITIAL_BITRATE_BPS = 1'000'000;

struct Command
{
    std::string name {};
    std::string type {};
    std::string payload {};
};

struct BridgeEvent
{
    std::string name {};
    std::map<std::string, std::string> fields {};
};

struct ReturnTrafficStats
{
    size_t packetBytes {0};
    size_t packetCount {0};
    size_t rtpBytes {0};
    size_t rtcpBytes {0};
};

struct LiveRtpProfile
{
    std::string name {"default"};
    size_t packetCount {1};
    size_t packetSize {16};
    std::chrono::milliseconds packetSpacing {0};
    std::chrono::milliseconds feedbackTimeout {0};
    uint64_t initialBitrateBps {TEST_INITIAL_BITRATE_BPS};
    uint64_t expectedMaxSentBitrateBps {0};
    bool expectTransportCc {false};
};

struct LiveRtpStats
{
    size_t packetCount {0};
    size_t payloadBytes {0};
};

struct TransportCcBridgeStats
{
    size_t feedbackCount {0};
    size_t reportCount {0};
    uint64_t targetBitrateBps {0};
    uint64_t acknowledgedBitrateBps {0};
    uint64_t sentBitrateBps {0};
    double packetLossRatio {0.0};
    double confidence {0.0};
};

struct LiveSession
{
    std::string mode {};
    std::string accountId {};
    std::shared_ptr<SIPAccount> account {};
    std::unique_ptr<Sdp> sdp {};
    std::shared_ptr<dhtnet::IceTransport> iceTransport {};
    std::shared_ptr<dht::crypto::Certificate> certificate {};
    std::shared_ptr<dht::crypto::PrivateKey> privateKey {};
    std::unique_ptr<pj_pool_t, std::function<void(pj_pool_t*)>> pool {};
};

std::string
printSdp(const pjmedia_sdp_session* session)
{
    std::array<char, 16384> buffer {};
    const auto size = pjmedia_sdp_print(session, buffer.data(), buffer.size());
    if (size <= 0)
        return {};
    return std::string(buffer.data(), static_cast<size_t>(size));
}

std::unique_ptr<pj_pool_t, std::function<void(pj_pool_t*)>>
makePool(const char* name)
{
    return {pj_pool_create(&Manager::instance().sipVoIPLink().getCachingPool()->factory, name, 4096, 4096, nullptr),
            [](pj_pool_t* pool) { pj_pool_release(pool); }};
}

pjmedia_sdp_session*
parseSdp(pj_pool_t* pool, const std::string& rawSdp)
{
    pjmedia_sdp_session* session = nullptr;
    auto* buffer = const_cast<char*>(rawSdp.c_str());
    if (pjmedia_sdp_parse(pool, buffer, rawSdp.size(), &session) != PJ_SUCCESS)
        return nullptr;
    return session;
}

std::shared_ptr<SIPAccount>
createSipAccount(std::string& accountId)
{
    libjami::init(libjami::InitFlag(0));
    if (!Manager::instance().initialized && !libjami::start("jami-sample.yml"))
        return {};

    std::map<std::string, std::string> details = libjami::getAccountTemplate("SIP");
    details[ConfProperties::TYPE] = "SIP";
    details[ConfProperties::DISPLAYNAME] = TEST_ACCOUNT_ALIAS;
    details[ConfProperties::ALIAS] = TEST_ACCOUNT_ALIAS;
    details[ConfProperties::UPNP_ENABLED] = "false";

    accountId = Manager::instance().addAccount(details);
    auto account = Manager::instance().getAccount<SIPAccount>(accountId);
    if (account)
        account->enableVideo(true);
    return account;
}

dht::crypto::Identity
createDtlsIdentity()
{
    return dht::crypto::generateIdentity("jami-webrtc-interop", {}, 2048, false);
}

std::vector<std::shared_ptr<SystemCodecInfo>>
filterCodecs(const std::vector<std::shared_ptr<SystemCodecInfo>>& codecs,
             std::initializer_list<std::string_view> allowedNames,
             std::string_view mediaName)
{
    std::vector<std::shared_ptr<SystemCodecInfo>> filtered;
    for (const auto& codec : codecs) {
        if (!codec)
            continue;
        const auto codecName = std::string_view(codec->name);
        if (std::find(allowedNames.begin(), allowedNames.end(), codecName) != allowedNames.end())
            filtered.emplace_back(codec);
    }

    if (filtered.empty())
        throw std::runtime_error(std::string("missing WebRTC-compatible ") + std::string(mediaName) + " codec");
    return filtered;
}

std::unique_ptr<Sdp>
makeConfiguredSdp(const std::shared_ptr<SIPAccount>& account, std::string_view id, const dht::crypto::Identity& identity)
{
    auto sdp = std::make_unique<Sdp>(std::string(id));
    sdp->setPublishedIP("127.0.0.1", pj_AF_INET());
    sdp->setSecureMediaKeyExchange(KeyExchangeProtocol::DTLS);
    if (!identity.second)
        throw std::runtime_error("missing DTLS certificate");
    sdp->setLocalDtlsFingerprint("SHA-256", getDtlsFingerprint(*identity.second));
    sdp->setLocalMediaCapabilities(MediaType::MEDIA_AUDIO,
                                   filterCodecs(account->getActiveAccountCodecInfoList(MEDIA_AUDIO),
                                                {"opus", "G722", "PCMA", "PCMU"},
                                                "audio"));
    sdp->setLocalMediaCapabilities(MediaType::MEDIA_VIDEO,
                                   filterCodecs(account->getActiveAccountCodecInfoList(MEDIA_VIDEO), {"VP8"}, "video"));
    sdp->enableRtcpMux(true);
    sdp->enableBundle(true);
    return sdp;
}

std::vector<MediaAttribute>
defaultMedia()
{
    MediaAttribute audio(MediaType::MEDIA_AUDIO);
    audio.enabled_ = true;
    audio.label_ = "audio_0";

    MediaAttribute video(MediaType::MEDIA_VIDEO);
    video.enabled_ = true;
    video.label_ = "video_0";

    return {audio, video};
}

void
configurePublishedAddress(Sdp& sdp, const dhtnet::IceTransport& iceTransport)
{
    const auto candidates = iceTransport.getLocalCandidates(1);
    if (candidates.empty())
        throw std::runtime_error("missing local ICE candidate");

    std::string publishedIp;
    uint16_t publishedPort = 0;
    for (const auto& candidate : candidates) {
        std::istringstream iss(candidate);
        std::vector<std::string> tokens;
        for (std::string token; iss >> token;)
            tokens.push_back(std::move(token));
        if (tokens.size() < 6)
            continue;
        if (tokens[4].find(':') != std::string::npos)
            continue;
        publishedIp = tokens[4];
        publishedPort = static_cast<uint16_t>(std::stoi(tokens[5]));
        break;
    }

    if (publishedIp.empty() || publishedPort == 0)
        throw std::runtime_error("failed to derive published IP/port from ICE candidates");

    sdp.setPublishedIP(publishedIp, pj_AF_INET());
    sdp.setLocalPublishedAudioPorts(publishedPort, 0);
    sdp.setLocalPublishedVideoPorts(publishedPort, 0);
}

void
addIceData(Sdp& sdp, const dhtnet::IceTransport& iceTransport)
{
    sdp.addIceAttributes(iceTransport.getLocalAttributes());

    const auto candidates = iceTransport.getLocalCandidates(1);
    if (candidates.empty())
        throw std::runtime_error("missing local ICE candidate");
    sdp.addIceCandidates(0, candidates);
    sdp.addIceCandidates(1, candidates);
}

void
addStaticIceData(Sdp& sdp)
{
    dhtnet::IceTransport::Attribute attrs;
    attrs.ufrag = "werift0";
    attrs.pwd = "werift-password-0001";
    sdp.addIceAttributes(std::move(attrs));

    sdp.setPublishedIP("127.0.0.1", pj_AF_INET());
    sdp.setLocalPublishedAudioPorts(SDP_ONLY_PORT, 0);
    sdp.setLocalPublishedVideoPorts(SDP_ONLY_PORT, 0);

    const std::vector<std::string> candidates {"1 1 udp 2130706431 127.0.0.1 4000 typ host"};
    sdp.addIceCandidates(0, candidates);
    sdp.addIceCandidates(1, candidates);
}

std::string
readStdin()
{
    return {std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>()};
}

std::optional<Command>
readCommand()
{
    std::string line;
    if (!std::getline(std::cin, line))
        return std::nullopt;

    std::istringstream iss(line);
    Command command;
    if (!(iss >> command.name))
        return std::nullopt;
    iss >> command.type;
    if (!command.type.empty())
        iss >> command.payload;
    return command;
}

void
emitEvent(const BridgeEvent& event)
{
    std::cout << event.name;
    for (const auto& [key, value] : event.fields) {
        std::cout << ' ' << key << '=' << value;
    }
    std::cout << std::endl;
}

std::string
decodePayload(const std::string& payload)
{
    const auto decoded = base64::decode(payload);
    return {decoded.begin(), decoded.end()};
}

std::string
encodePayload(std::string_view payload)
{
    return base64::encode(payload);
}

std::shared_ptr<dhtnet::IceTransport>
createLiveIceTransport(const std::shared_ptr<SIPAccount>& account, std::string_view id, bool master)
{
    auto transport = Manager::instance().getIceTransportFactory()->createTransport(std::string(id));
    if (!transport)
        throw std::runtime_error("failed to create ICE transport");

    auto options = account->getIceOptions();
    options.factory = Manager::instance().getIceTransportFactory();
    options.master = master;
    options.streamsCount = 1;
    options.compCountPerStream = 1;
    options.qosType = {dhtnet::QosType::VOICE};

    options.accountPublicAddr = account->getPublishedIpAddress();
    if (options.accountPublicAddr) {
        options.accountLocalAddr = dhtnet::ip_utils::getInterfaceAddr(account->getLocalInterface(),
                                                                      options.accountPublicAddr.getFamily());
    } else {
        options.accountLocalAddr = dhtnet::ip_utils::getInterfaceAddr(account->getLocalInterface(), AF_INET);
        options.accountPublicAddr = options.accountLocalAddr;
    }

    if (!options.accountLocalAddr)
        throw std::runtime_error("missing local interface address for ICE");

    transport->initIceInstance(options);
    if (!transport->waitForInitialization(std::chrono::duration_cast<std::chrono::milliseconds>(ICE_INIT_TIMEOUT)))
        throw std::runtime_error("ICE initialization timeout");
    if (transport->isFailed())
        throw std::runtime_error("ICE initialization failed");
    return transport;
}

LiveSession
createLiveSession(std::string mode)
{
    LiveSession session;
    session.mode = std::move(mode);
    session.account = createSipAccount(session.accountId);
    if (!session.account)
        throw std::runtime_error("failed to create SIP account");

    const auto identity = createDtlsIdentity();
    session.privateKey = identity.first;
    session.certificate = identity.second;
    if (!session.privateKey || !session.certificate)
        throw std::runtime_error("failed to generate DTLS identity");

    session.iceTransport = createLiveIceTransport(session.account, session.mode, session.mode == "live-offer");
    session.sdp = makeConfiguredSdp(session.account, session.mode, identity);
    configurePublishedAddress(*session.sdp, *session.iceTransport);
    return session;
}

std::vector<dhtnet::IceCandidate>
collectBundledRemoteCandidates(dhtnet::IceTransport& iceTransport, const pjmedia_sdp_session* remoteSession)
{
    if (!remoteSession)
        throw std::runtime_error("missing remote SDP session");

    std::vector<dhtnet::IceCandidate> candidates;
    for (unsigned mediaIdx = 0; mediaIdx < remoteSession->media_count; ++mediaIdx) {
        const auto* media = remoteSession->media[mediaIdx];
        if (!media || media->desc.port == 0)
            continue;

        dhtnet::IceCandidate candidate;
        for (unsigned attrIdx = 0; attrIdx < media->attr_count; ++attrIdx) {
            auto* attr = media->attr[attrIdx];
            if (pj_stricmp2(&attr->name, "candidate") != 0)
                continue;

            const std::string line(attr->value.ptr, attr->value.slen);
            if (iceTransport.parseIceAttributeLine(0, line, candidate))
                candidates.emplace_back(std::move(candidate));
        }

        if (!candidates.empty())
            break;
    }

    return candidates;
}

std::vector<MediaDescription>
getLocalSessionDescriptions(Sdp& sdp)
{
    return sdp.getMediaDescriptions(sdp.getLocalSdpSession(), false);
}

std::vector<MediaDescription>
getRemoteSessionDescriptions(const Sdp& sdp, const pjmedia_sdp_session* remoteSession)
{
    return sdp.getMediaDescriptions(remoteSession, true);
}

void
forceSessionDtlsSetup(pjmedia_sdp_session* session, DtlsSetup setup)
{
    if (!session)
        return;

    const char* setupName = nullptr;
    switch (setup) {
    case DtlsSetup::ACTIVE:
        setupName = "active";
        break;
    case DtlsSetup::PASSIVE:
        setupName = "passive";
        break;
    case DtlsSetup::ACTPASS:
        setupName = "actpass";
        break;
    default:
        return;
    }

    pj_str_t value {const_cast<char*>(setupName), static_cast<pj_ssize_t>(std::strlen(setupName))};
    for (unsigned i = 0; i < session->media_count; ++i) {
        auto* media = session->media[i];
        if (!media || media->desc.port == 0)
            continue;
        for (unsigned attrIdx = 0; attrIdx < media->attr_count; ++attrIdx) {
            auto* attr = media->attr[attrIdx];
            if (pj_stricmp2(&attr->name, "setup") == 0)
                attr->value = value;
        }
    }
}

void
applyNegotiatedDtlsRoles(std::vector<MediaDescription>& localDescriptions,
                         const std::vector<MediaDescription>& remoteDescriptions)
{
    const auto count = std::min(localDescriptions.size(), remoteDescriptions.size());
    for (size_t i = 0; i < count; ++i) {
        switch (remoteDescriptions[i].dtls_setup) {
        case DtlsSetup::ACTIVE:
            localDescriptions[i].dtls_setup = DtlsSetup::PASSIVE;
            break;
        case DtlsSetup::PASSIVE:
            localDescriptions[i].dtls_setup = DtlsSetup::ACTIVE;
            break;
        default:
            throw std::runtime_error("invalid remote DTLS setup in negotiated answer");
        }
    }
}

void
startLiveIce(const std::shared_ptr<dhtnet::IceTransport>& iceTransport, const pjmedia_sdp_session* remoteSession)
{
    const auto remoteAttrs = Sdp::getIceAttributes(remoteSession);
    if (remoteAttrs.ufrag.empty() || remoteAttrs.pwd.empty())
        throw std::runtime_error("missing remote ICE credentials");

    auto remoteCandidates = collectBundledRemoteCandidates(*iceTransport, remoteSession);
    if (remoteCandidates.empty())
        throw std::runtime_error("missing remote ICE candidates");

    if (!iceTransport->startIce(remoteAttrs, std::move(remoteCandidates)))
        throw std::runtime_error("failed to start ICE negotiation");

    const auto deadline = std::chrono::steady_clock::now() + ICE_NEGO_TIMEOUT;
    while (std::chrono::steady_clock::now() < deadline) {
        if (iceTransport->isRunning())
            return;
        if (iceTransport->isFailed())
            throw std::runtime_error("ICE negotiation failed");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    throw std::runtime_error("ICE negotiation timeout");
}

std::unique_ptr<SocketPair>
createLiveSocketPair(std::shared_ptr<dhtnet::IceTransport> iceTransport,
                     const MediaDescription& localDescription,
                     const MediaDescription& remoteDescription,
                     const std::shared_ptr<dht::crypto::Certificate>& certificate,
                     const std::shared_ptr<dht::crypto::PrivateKey>& privateKey,
                     bool acceptAnyRemoteMid = false)
{
    auto rtpSocket = std::make_unique<dhtnet::IceSocket>(iceTransport, 1);
    auto dtlsSrtp = negotiateDtlsSrtp(*rtpSocket,
                                      localDescription.dtls_setup,
                                      remoteDescription.dtls_fingerprint_type,
                                      remoteDescription.dtls_fingerprint,
                                      certificate,
                                      privateKey);

    auto socketPair = std::make_unique<SocketPair>(std::move(rtpSocket), nullptr, true);
    socketPair->createSRTP(dtlsSrtp.suite.c_str(),
                           dtlsSrtp.outboundKeyInfo.c_str(),
                           dtlsSrtp.suite.c_str(),
                           dtlsSrtp.inboundKeyInfo.c_str());
    socketPair->setBundleMidExtension(localDescription.mid,
                                      localDescription.mid_rtp_ext_id
                                          ? std::optional<unsigned> {localDescription.mid_rtp_ext_id}
                                          : std::nullopt,
                                      acceptAnyRemoteMid ? std::string {} : remoteDescription.mid,
                                      remoteDescription.mid_rtp_ext_id
                                          ? std::optional<unsigned> {remoteDescription.mid_rtp_ext_id}
                                          : std::nullopt);
    socketPair->setTransportCcExtension(localDescription.rtcp_fb_transport_cc && localDescription.transport_cc_rtp_ext_id
                                            ? std::optional<unsigned> {localDescription.transport_cc_rtp_ext_id}
                                            : std::nullopt,
                                        remoteDescription.rtcp_fb_transport_cc
                                                && remoteDescription.transport_cc_rtp_ext_id
                                            ? std::optional<unsigned> {remoteDescription.transport_cc_rtp_ext_id}
                                            : std::nullopt);
    return socketPair;
}

size_t
selectMediaIndex(const std::vector<MediaDescription>& descriptions, MediaType preferredType)
{
    if (descriptions.empty())
        throw std::runtime_error("missing negotiated media descriptions");

    for (size_t index = 0; index < descriptions.size(); ++index) {
        if (descriptions[index].enabled && descriptions[index].type == preferredType)
            return index;
    }
    for (size_t index = 0; index < descriptions.size(); ++index) {
        if (descriptions[index].type == preferredType)
            return index;
    }
    return 0;
}

std::vector<uint8_t>
makeTestRtpPacket(uint16_t payloadType = TEST_AUDIO_PAYLOAD_TYPE,
                  uint16_t sequence = TEST_AUDIO_SEQUENCE,
                  uint32_t timestamp = TEST_AUDIO_TIMESTAMP,
                  uint32_t ssrc = TEST_AUDIO_SSRC,
                  size_t packetSize = 16)
{
    std::vector<uint8_t> packet(std::max<size_t>(16, packetSize), 0);
    packet[0] = 0x80;
    packet[1] = static_cast<uint8_t>(payloadType & 0x7f);
    packet[2] = static_cast<uint8_t>((sequence >> 8) & 0xff);
    packet[3] = static_cast<uint8_t>(sequence & 0xff);
    packet[4] = static_cast<uint8_t>((timestamp >> 24) & 0xff);
    packet[5] = static_cast<uint8_t>((timestamp >> 16) & 0xff);
    packet[6] = static_cast<uint8_t>((timestamp >> 8) & 0xff);
    packet[7] = static_cast<uint8_t>(timestamp & 0xff);
    packet[8] = static_cast<uint8_t>((ssrc >> 24) & 0xff);
    packet[9] = static_cast<uint8_t>((ssrc >> 16) & 0xff);
    packet[10] = static_cast<uint8_t>((ssrc >> 8) & 0xff);
    packet[11] = static_cast<uint8_t>(ssrc & 0xff);
    packet[12] = 0xde;
    packet[13] = 0xad;
    packet[14] = 0xbe;
    packet[15] = 0xef;
    for (size_t index = 16; index < packet.size(); ++index)
        packet[index] = static_cast<uint8_t>(index & 0xff);
    return packet;
}

LiveRtpStats
sendEncryptedRtpPackets(SocketPair& socketPair, const MediaDescription& media, const LiveRtpProfile& profile)
{
    std::unique_ptr<MediaIOHandle> ioContext(socketPair.createIOContext(1500));
    auto* context = ioContext->getContext();
    LiveRtpStats stats;
    const auto payloadType = media.payload_type ?: TEST_AUDIO_PAYLOAD_TYPE;
    for (size_t packetIndex = 0; packetIndex < profile.packetCount; ++packetIndex) {
        auto packet = makeTestRtpPacket(payloadType,
                                        static_cast<uint16_t>(TEST_AUDIO_SEQUENCE + packetIndex),
                                        TEST_AUDIO_TIMESTAMP + static_cast<uint32_t>(packetIndex * 3000),
                                        TEST_AUDIO_SSRC,
                                        profile.packetSize);
        if (media.type == MediaType::MEDIA_VIDEO && packet.size() > 12) {
            packet[1] |= 0x80;
            constexpr std::array<uint8_t, 11> vp8KeyFrameHeader {{
                0x10, // VP8 payload descriptor: start of partition 0.
                0x50,
                0x01,
                0x00, // Key frame, show frame, first partition size 10 bytes.
                0x9d,
                0x01,
                0x2a, // VP8 key frame sync code.
                0x10,
                0x00, // Width: 16.
                0x10,
                0x00, // Height: 16.
            }};
            std::copy(vp8KeyFrameHeader.begin(), vp8KeyFrameHeader.end(), packet.begin() + 12);
        }
        avio_write(context, packet.data(), static_cast<int>(packet.size()));
        avio_flush(context);
        if (context->error < 0)
            throw std::runtime_error("failed to send SRTP packet");
        ++stats.packetCount;
        stats.payloadBytes += packet.size();
        if (profile.packetSpacing.count() > 0 && packetIndex + 1 < profile.packetCount)
            std::this_thread::sleep_for(profile.packetSpacing);
    }
    return stats;
}

bool
isRtcpPacket(const unsigned char* packet, size_t len)
{
    if (len < 2)
        return false;
    const auto packetType = packet[1];
    return (packetType >= 192 && packetType <= 195) || (packetType >= 200 && packetType <= 210);
}

ReturnTrafficStats
waitForEncryptedReturnTraffic(const std::shared_ptr<dhtnet::IceTransport>& iceTransport)
{
    ReturnTrafficStats stats;
    const auto deadline = std::chrono::steady_clock::now() + REMOTE_PACKET_TIMEOUT;
    std::error_code ec;

    while (std::chrono::steady_clock::now() < deadline) {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
        auto ready = iceTransport->waitForData(1, remaining, ec);
        if (ready <= 0 || ec)
            break;

        std::array<char, 2048> buffer {};
        const auto received = iceTransport->recvfrom(1, buffer.data(), buffer.size(), ec);
        if (ec || received <= 0)
            break;

        const auto bytes = static_cast<size_t>(received);
        stats.packetBytes += bytes;
        stats.packetCount += 1;
        const auto* packet = reinterpret_cast<const unsigned char*>(buffer.data());
        if (isRtcpPacket(packet, bytes))
            stats.rtcpBytes += bytes;
        else
            stats.rtpBytes += bytes;

        if (stats.rtpBytes > 0 && stats.rtcpBytes > 0)
            break;
    }

    return stats;
}

BridgeEvent
makeSdpEvent(std::string_view type, const std::string& sdp)
{
    return {"SDP", {{"type", std::string(type)}, {"payload", encodePayload(sdp)}}};
}

LiveRtpProfile
waitForStartRtpCommand()
{
    const auto command = readCommand();
    if (!command || command->name != "START_RTP")
        throw std::runtime_error("expected START_RTP command");

    if (command->type == "bandwidth") {
        return {"bandwidth",
                64,
                1000,
                std::chrono::milliseconds(20),
                std::chrono::milliseconds(3000),
                TEST_INITIAL_BITRATE_BPS,
                800'000,
                true};
    }

    if (!command->type.empty() && command->type != "default")
        throw std::runtime_error("unknown START_RTP profile: " + command->type);

    return {};
}

std::list<TransportCcReport>
collectTransportCcReports(SocketPair& socketPair, std::chrono::milliseconds timeout)
{
    std::list<TransportCcReport> reports;
    if (timeout.count() <= 0)
        return reports;

    std::unique_ptr<MediaIOHandle> ioContext(socketPair.createIOContext(1500));
    auto* context = ioContext->getContext();
    std::array<uint8_t, 2048> buffer {};
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    socketPair.setReadBlockingMode(false);
    while (std::chrono::steady_clock::now() < deadline) {
        const auto bytesRead = avio_read(context, buffer.data(), static_cast<int>(buffer.size()));
        if (bytesRead < 0)
            break;

        auto nextReports = socketPair.getRtcpTransportCcReports();
        reports.splice(reports.end(), nextReports);
        if (!reports.empty())
            break;

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    socketPair.setReadBlockingMode(true);
    return reports;
}

TransportCcBridgeStats
estimateTransportCc(SocketPair& socketPair, const LiveRtpProfile& profile)
{
    TransportCcBridgeStats stats;
    auto reports = collectTransportCcReports(socketPair, profile.feedbackTimeout);
    auto feedback = socketPair.getRtcpTransportCc();
    stats.feedbackCount = feedback.size();
    stats.reportCount = reports.size();
    if (reports.empty())
        return stats;

    TransportCcController controller;
    auto estimate = controller.update(profile.initialBitrateBps, reports);
    if (!estimate)
        return stats;

    stats.targetBitrateBps = estimate->targetBitrateBps;
    stats.acknowledgedBitrateBps = estimate->acknowledgedBitrateBps;
    stats.sentBitrateBps = estimate->sentBitrateBps;
    stats.packetLossRatio = estimate->packetLossRatio;
    stats.confidence = estimate->confidence;
    return stats;
}

void
runLiveMediaExchange(LiveSession& session,
                     const std::vector<MediaDescription>& localDescriptions,
                     const std::vector<MediaDescription>& remoteDescriptions)
{
    const auto profile = waitForStartRtpCommand();
    const auto mediaIndex = selectMediaIndex(localDescriptions, MediaType::MEDIA_AUDIO);
    if (mediaIndex >= remoteDescriptions.size())
        throw std::runtime_error("missing remote media description for selected RTP profile");

    auto socketPair = createLiveSocketPair(session.iceTransport,
                                           localDescriptions[mediaIndex],
                                           remoteDescriptions[mediaIndex],
                                           session.certificate,
                                           session.privateKey,
                                           profile.expectTransportCc);
    emitEvent({"CONNECTED", {{"ice", "true"}, {"dtls", "true"}}});

    const auto rtpStats = sendEncryptedRtpPackets(*socketPair, localDescriptions[mediaIndex], profile);
    const auto transportCcSentPackets = socketPair->getTransportCcSentPacketCount();
    uint64_t profileSentBitrateBps = 0;
    if (profile.packetSpacing.count() > 0 && rtpStats.packetCount > 1) {
        const auto durationMs = profile.packetSpacing.count() * (rtpStats.packetCount - 1);
        profileSentBitrateBps = durationMs > 0 ? (rtpStats.payloadBytes * 8 * 1000) / durationMs : 0;
    }
    emitEvent({"RTP_SENT",
               {{"ssrc", std::to_string(TEST_AUDIO_SSRC)},
                {"profile", profile.name},
                {"packets", std::to_string(rtpStats.packetCount)}}});

    const auto transportCc = estimateTransportCc(*socketPair, profile);
    // Release the media callback so the raw ICE queue can capture return-path diagnostics.
    socketPair.reset();

    const auto returnTraffic = waitForEncryptedReturnTraffic(session.iceTransport);
    emitEvent({"RESULT",
               {{"ice", "true"},
                {"dtls", "true"},
                {"rtp_sent", "true"},
                {"profile", profile.name},
                {"local_mid", localDescriptions[mediaIndex].mid},
                {"remote_mid", remoteDescriptions[mediaIndex].mid},
                {"local_payload_type", std::to_string(localDescriptions[mediaIndex].payload_type)},
                {"local_transport_cc_ext_id", std::to_string(localDescriptions[mediaIndex].transport_cc_rtp_ext_id)},
                {"remote_transport_cc_ext_id", std::to_string(remoteDescriptions[mediaIndex].transport_cc_rtp_ext_id)},
                {"local_transport_cc_feedback", localDescriptions[mediaIndex].rtcp_fb_transport_cc ? "true" : "false"},
                {"remote_transport_cc_feedback", remoteDescriptions[mediaIndex].rtcp_fb_transport_cc ? "true" : "false"},
                {"transport_cc_sent_packets", std::to_string(transportCcSentPackets)},
                {"rtp_packet_count", std::to_string(rtpStats.packetCount)},
                {"rtp_payload_bytes", std::to_string(rtpStats.payloadBytes)},
                {"profile_sent_bitrate_bps", std::to_string(profileSentBitrateBps)},
                {"expected_max_sent_bitrate_bps", std::to_string(profile.expectedMaxSentBitrateBps)},
                {"transport_cc_feedback", std::to_string(transportCc.feedbackCount)},
                {"transport_cc_reports", std::to_string(transportCc.reportCount)},
                {"target_bitrate_bps", std::to_string(transportCc.targetBitrateBps)},
                {"acknowledged_bitrate_bps", std::to_string(transportCc.acknowledgedBitrateBps)},
                {"sent_bitrate_bps", std::to_string(transportCc.sentBitrateBps)},
                {"packet_loss_ratio", std::to_string(transportCc.packetLossRatio)},
                {"confidence", std::to_string(transportCc.confidence)},
                {"remote_packet_bytes", std::to_string(returnTraffic.packetBytes)},
                {"remote_packet_count", std::to_string(returnTraffic.packetCount)},
                {"remote_rtp_bytes", std::to_string(returnTraffic.rtpBytes)},
                {"remote_rtcp_bytes", std::to_string(returnTraffic.rtcpBytes)}}});
}

void
runLiveOffer()
{
    auto session = createLiveSession("live-offer");
    if (!session.sdp->createOffer(defaultMedia()))
        throw std::runtime_error("createOffer failed");
    forceSessionDtlsSetup(session.sdp->getLocalSdpSession(), DtlsSetup::ACTIVE);
    addIceData(*session.sdp, *session.iceTransport);

    emitEvent(makeSdpEvent("offer", printSdp(session.sdp->getLocalSdpSession())));

    const auto command = readCommand();
    if (!command || command->name != "SDP" || command->type != "answer" || command->payload.empty())
        throw std::runtime_error("expected SDP answer command");

    const auto remoteSdp = decodePayload(command->payload);
    session.pool = makePool("werift-live-answer-pool");
    auto* remoteSession = parseSdp(session.pool.get(), remoteSdp);
    if (!remoteSession)
        throw std::runtime_error("failed to parse remote answer");
    session.sdp->setActiveRemoteSdpSession(remoteSession);
    auto remoteDescriptions = getRemoteSessionDescriptions(*session.sdp, remoteSession);
    auto localDescriptions = getLocalSessionDescriptions(*session.sdp);
    applyNegotiatedDtlsRoles(localDescriptions, remoteDescriptions);

    startLiveIce(session.iceTransport, remoteSession);
    emitEvent({"CONNECTED", {{"ice", "true"}, {"dtls", "pending"}}});
    runLiveMediaExchange(session, localDescriptions, remoteDescriptions);
}

void
runLiveAnswer()
{
    const auto command = readCommand();
    if (!command || command->name != "SDP" || command->type != "offer" || command->payload.empty())
        throw std::runtime_error("expected SDP offer command");

    auto session = createLiveSession("live-answer");
    const auto remoteOffer = decodePayload(command->payload);
    session.pool = makePool("werift-live-answer-pool");
    auto* remoteSession = parseSdp(session.pool.get(), remoteOffer);
    if (!remoteSession)
        throw std::runtime_error("failed to parse remote offer");

    session.sdp->setReceivedOffer(remoteSession);
    if (!session.sdp->processIncomingOffer(defaultMedia()))
        throw std::runtime_error("processIncomingOffer failed");
    forceSessionDtlsSetup(session.sdp->getLocalSdpSession(), DtlsSetup::ACTIVE);
    addIceData(*session.sdp, *session.iceTransport);

    const auto localDescriptions = getLocalSessionDescriptions(*session.sdp);
    const auto remoteDescriptions = getRemoteSessionDescriptions(*session.sdp, remoteSession);
    emitEvent(makeSdpEvent("answer", printSdp(session.sdp->getLocalSdpSession())));

    startLiveIce(session.iceTransport, remoteSession);
    emitEvent({"CONNECTED", {{"ice", "true"}, {"dtls", "pending"}}});
    runLiveMediaExchange(session, localDescriptions, remoteDescriptions);
}

} // namespace

int
main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "usage: jami_webrtc_sdp_bridge <offer|answer|live-offer|live-answer>" << std::endl;
        return 2;
    }

    const std::string mode(argv[1]);
    int exitCode = 0;
    try {
        if (mode == "offer") {
            std::string accountId;
            auto account = createSipAccount(accountId);
            if (!account)
                throw std::runtime_error("failed to create SIP account");
            auto identity = createDtlsIdentity();
            auto sdp = makeConfiguredSdp(account, "werift-live-offer", identity);
            if (!sdp->createOffer(defaultMedia())) {
                std::cerr << "createOffer failed" << std::endl;
                exitCode = 1;
            } else {
                addStaticIceData(*sdp);
                std::cout << printSdp(sdp->getLocalSdpSession());
            }
        } else if (mode == "answer") {
            const auto remoteOffer = readStdin();
            if (remoteOffer.empty()) {
                std::cerr << "missing remote offer on stdin" << std::endl;
                exitCode = 1;
            } else {
                std::string accountId;
                auto account = createSipAccount(accountId);
                if (!account)
                    throw std::runtime_error("failed to create SIP account");
                auto identity = createDtlsIdentity();
                auto sdp = makeConfiguredSdp(account, "werift-live-answer", identity);
                auto pool = makePool("werift-live-answer-pool");
                auto* remoteSession = parseSdp(pool.get(), remoteOffer);
                if (!remoteSession) {
                    std::cerr << "failed to parse remote offer" << std::endl;
                    exitCode = 1;
                } else {
                    sdp->setReceivedOffer(remoteSession);
                    if (!sdp->processIncomingOffer(defaultMedia())) {
                        std::cerr << "processIncomingOffer failed" << std::endl;
                        exitCode = 1;
                    } else {
                        addStaticIceData(*sdp);
                        std::cout << printSdp(sdp->getLocalSdpSession());
                    }
                }
            }
        } else if (mode == "live-offer") {
            runLiveOffer();
        } else if (mode == "live-answer") {
            runLiveAnswer();
        } else {
            std::cerr << "unknown mode: " << mode << std::endl;
            exitCode = 2;
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        exitCode = 1;
    }

    libjami::fini();
    return exitCode;
}