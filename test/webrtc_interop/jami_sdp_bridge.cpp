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
constexpr std::chrono::seconds REMOTE_PACKET_TIMEOUT {1};
constexpr uint16_t SDP_ONLY_PORT = 4000;
constexpr uint32_t TEST_AUDIO_SSRC = 0x12345678;
constexpr uint16_t TEST_AUDIO_PAYLOAD_TYPE = 111;
constexpr uint16_t TEST_AUDIO_SEQUENCE = 32000;
constexpr uint32_t TEST_AUDIO_TIMESTAMP = 96000;

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
                     const std::vector<MediaDescription>& localDescriptions,
                     const std::vector<MediaDescription>& remoteDescriptions,
                     const std::shared_ptr<dht::crypto::Certificate>& certificate,
                     const std::shared_ptr<dht::crypto::PrivateKey>& privateKey)
{
    if (localDescriptions.empty() || remoteDescriptions.empty())
        throw std::runtime_error("missing negotiated media descriptions");

    auto rtpSocket = std::make_unique<dhtnet::IceSocket>(iceTransport, 1);
    auto dtlsSrtp = negotiateDtlsSrtp(*rtpSocket,
                                      localDescriptions.front().dtls_setup,
                                      remoteDescriptions.front().dtls_fingerprint_type,
                                      remoteDescriptions.front().dtls_fingerprint,
                                      certificate,
                                      privateKey);

    auto socketPair = std::make_unique<SocketPair>(std::move(rtpSocket), nullptr, true);
    socketPair->createSRTP(dtlsSrtp.suite.c_str(),
                           dtlsSrtp.outboundKeyInfo.c_str(),
                           dtlsSrtp.suite.c_str(),
                           dtlsSrtp.inboundKeyInfo.c_str());
    return socketPair;
}

std::vector<uint8_t>
makeTestRtpPacket(uint16_t payloadType = TEST_AUDIO_PAYLOAD_TYPE,
                  uint16_t sequence = TEST_AUDIO_SEQUENCE,
                  uint32_t timestamp = TEST_AUDIO_TIMESTAMP,
                  uint32_t ssrc = TEST_AUDIO_SSRC)
{
    std::vector<uint8_t> packet(16, 0);
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
    return packet;
}

void
sendEncryptedRtpPacket(SocketPair& socketPair, const std::vector<uint8_t>& packet)
{
    std::unique_ptr<MediaIOHandle> ioContext(socketPair.createIOContext(1500));
    auto* context = ioContext->getContext();
    avio_write(context, packet.data(), static_cast<int>(packet.size()));
    avio_flush(context);
    if (context->error < 0)
        throw std::runtime_error("failed to send SRTP packet");
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

void
waitForStartRtpCommand()
{
    const auto command = readCommand();
    if (!command || command->name != "START_RTP")
        throw std::runtime_error("expected START_RTP command");
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

    auto socketPair = createLiveSocketPair(session.iceTransport,
                                           localDescriptions,
                                           remoteDescriptions,
                                           session.certificate,
                                           session.privateKey);
    emitEvent({"CONNECTED", {{"ice", "true"}, {"dtls", "true"}}});
    waitForStartRtpCommand();

    auto packet = makeTestRtpPacket(localDescriptions.front().payload_type ?: TEST_AUDIO_PAYLOAD_TYPE);
    sendEncryptedRtpPacket(*socketPair, packet);
    emitEvent({"RTP_SENT", {{"ssrc", std::to_string(TEST_AUDIO_SSRC)}}});
    // Release the media callback so the raw ICE queue can capture return-path diagnostics.
    socketPair.reset();

    const auto returnTraffic = waitForEncryptedReturnTraffic(session.iceTransport);
    emitEvent({"RESULT",
               {{"ice", "true"},
                {"dtls", "true"},
                {"rtp_sent", "true"},
                {"remote_packet_bytes", std::to_string(returnTraffic.packetBytes)},
                {"remote_packet_count", std::to_string(returnTraffic.packetCount)},
                {"remote_rtp_bytes", std::to_string(returnTraffic.rtpBytes)},
                {"remote_rtcp_bytes", std::to_string(returnTraffic.rtcpBytes)}}});
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

    auto socketPair = createLiveSocketPair(session.iceTransport,
                                           localDescriptions,
                                           remoteDescriptions,
                                           session.certificate,
                                           session.privateKey);
    emitEvent({"CONNECTED", {{"ice", "true"}, {"dtls", "true"}}});
    waitForStartRtpCommand();

    auto packet = makeTestRtpPacket(localDescriptions.front().payload_type ?: TEST_AUDIO_PAYLOAD_TYPE);
    sendEncryptedRtpPacket(*socketPair, packet);
    emitEvent({"RTP_SENT", {{"ssrc", std::to_string(TEST_AUDIO_SSRC)}}});
    // Release the media callback so the raw ICE queue can capture return-path diagnostics.
    socketPair.reset();

    const auto returnTraffic = waitForEncryptedReturnTraffic(session.iceTransport);
    emitEvent({"RESULT",
               {{"ice", "true"},
                {"dtls", "true"},
                {"rtp_sent", "true"},
                {"remote_packet_bytes", std::to_string(returnTraffic.packetBytes)},
                {"remote_packet_count", std::to_string(returnTraffic.packetCount)},
                {"remote_rtp_bytes", std::to_string(returnTraffic.rtpBytes)},
                {"remote_rtcp_bytes", std::to_string(returnTraffic.rtcpBytes)}}});
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