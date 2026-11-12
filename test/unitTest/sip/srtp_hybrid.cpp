/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
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
#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <tuple>
#include <utility>

#include "jami.h"
#include "manager.h"
#include "sip/sdp.h"
#include "sip/sipaccount.h"
#include "sip/sipvoiplink.h"
#include "account_const.h"
#include "common.h"
#include "test_runner.h"

extern "C" {
#include <pjmedia/sdp.h>
}

using namespace libjami::Account;

namespace jami {
namespace test {

namespace {

constexpr auto TEST_ACCOUNT_ALIAS = "SDP_SRTP_HYBRID_TEST";
constexpr uint16_t TEST_AUDIO_RTP_PORT = 4000;
constexpr std::string_view TEST_FINGERPRINT {
    "AB:CD:EF:01:23:45:67:89:AB:CD:EF:01:23:45:67:89:AB:CD:EF:01:23:45:67:89:AB:CD:EF:01:23:45:67:89"};

std::vector<std::string>
getMediaAttributes(const pjmedia_sdp_session* session, std::string_view attributeName, unsigned mediaIndex = 0)
{
    std::vector<std::string> attributes;
    if (not session or mediaIndex >= session->media_count)
        return attributes;

    auto* media = session->media[mediaIndex];
    for (unsigned i = 0; i < media->attr_count; ++i) {
        auto* attribute = media->attr[i];
        if (attributeName == std::string_view(attribute->name.ptr, attribute->name.slen))
            attributes.emplace_back(attribute->value.ptr, attribute->value.slen);
    }

    return attributes;
}

std::string
getMediaTransport(const pjmedia_sdp_session* session)
{
    if (not session or session->media_count == 0)
        return {};

    return {session->media[0]->desc.transport.ptr, static_cast<size_t>(session->media[0]->desc.transport.slen)};
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

std::string
dtlsSdp(std::string_view sessionAttributes, std::string_view mediaAttributes)
{
    return "v=0\r\n"
           "o=- 0 0 IN IP4 127.0.0.1\r\n"
           "s=-\r\n"
           "c=IN IP4 127.0.0.1\r\n"
           "t=0 0\r\n"
           + std::string(sessionAttributes)
           + "m=audio 5004 UDP/TLS/RTP/SAVPF 0\r\n"
             "a=rtpmap:0 PCMU/8000\r\n"
           + std::string(mediaAttributes);
}

} // namespace

/**
 * SDP-level tests for the hybrid SDES+DTLS offer used by Jami accounts:
 * the offer stays on the SDES-compatible RTP/SAVP transport but also
 * carries a DTLS fingerprint (RFC 5764 4.1); the answerer picks DTLS
 * when it can, and SDES-only peers keep working.
 */
class SrtpHybridSdpTest : public CppUnit::TestFixture
{
public:
    SrtpHybridSdpTest()
    {
        libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
    }

    ~SrtpHybridSdpTest() { libjami::fini(); }

    static std::string name() { return "srtp_hybrid"; }

    void setUp() override
    {
        std::map<std::string, std::string> details = libjami::getAccountTemplate("SIP");
        details[ConfProperties::TYPE] = "SIP";
        details[ConfProperties::DISPLAYNAME] = TEST_ACCOUNT_ALIAS;
        details[ConfProperties::ALIAS] = TEST_ACCOUNT_ALIAS;
        details[ConfProperties::UPNP_ENABLED] = "false";
        accountId_ = Manager::instance().addAccount(details);
        account_ = Manager::instance().getAccount<SIPAccount>(accountId_);
        CPPUNIT_ASSERT(account_);
    }

    void tearDown() override
    {
        account_.reset();
        if (not accountId_.empty())
            wait_for_removal_of({accountId_});
        accountId_.clear();
    }

private:
    void offerAdvertisesHybridSdesAndDtls();
    void offerStaysSdesWithoutDtlsIdentity();
    void answerPrefersDtlsForHybridOffer();
    void answerFallsBackToSdesWhenOfferHasNoFingerprint();
    void answerPreservesRejectedMediaSections();
    void offererMatchesReleaseSelectedSdesSuite();
    void legacyFallbackOfferUsesReleaseCryptoTag();
    void offerAdvertisesSdesWithDefaultKeyExchange();
    void rejectsMalformedDtlsAttributes();
    void dtlsAttributesRespectMediaOverrides();
    void validatesDtlsHashesAndAnswerRoles();
    void bundleUsesNegotiatedTagDtlsParameters();

    CPPUNIT_TEST_SUITE(SrtpHybridSdpTest);
    CPPUNIT_TEST(offerAdvertisesHybridSdesAndDtls);
    CPPUNIT_TEST(offerStaysSdesWithoutDtlsIdentity);
    CPPUNIT_TEST(answerPrefersDtlsForHybridOffer);
    CPPUNIT_TEST(answerFallsBackToSdesWhenOfferHasNoFingerprint);
    CPPUNIT_TEST(answerPreservesRejectedMediaSections);
    CPPUNIT_TEST(offererMatchesReleaseSelectedSdesSuite);
    CPPUNIT_TEST(legacyFallbackOfferUsesReleaseCryptoTag);
    CPPUNIT_TEST(offerAdvertisesSdesWithDefaultKeyExchange);
    CPPUNIT_TEST(rejectsMalformedDtlsAttributes);
    CPPUNIT_TEST(dtlsAttributesRespectMediaOverrides);
    CPPUNIT_TEST(validatesDtlsHashesAndAnswerRoles);
    CPPUNIT_TEST(bundleUsesNegotiatedTagDtlsParameters);
    CPPUNIT_TEST_SUITE_END();

    void configureSdp(Sdp& sdp, bool withDtlsIdentity)
    {
        sdp.setPublishedIP("127.0.0.1", pj_AF_INET());
        sdp.setLocalMediaCapabilities(MediaType::MEDIA_AUDIO, account_->getActiveAccountCodecInfoList(MEDIA_AUDIO));
        sdp.setLocalPublishedAudioPorts(TEST_AUDIO_RTP_PORT, 0);
        sdp.setSecureMediaKeyExchange(KeyExchangeProtocol::SDES);
        if (withDtlsIdentity)
            sdp.setLocalDtlsFingerprint("SHA-256", std::string(TEST_FINGERPRINT));
    }

    std::string accountId_ {};
    std::shared_ptr<SIPAccount> account_ {};
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(SrtpHybridSdpTest, SrtpHybridSdpTest::name());

void
SrtpHybridSdpTest::rejectsMalformedDtlsAttributes()
{
    const auto fingerprint = "a=fingerprint:SHA-256 " + std::string(TEST_FINGERPRINT) + "\r\n";
    for (const auto& attributes : {
             std::string("a=setup:actpass\r\n"),
             fingerprint,
             fingerprint + "a=setup:invalid\r\n",
             fingerprint + "a=setup:holdconn\r\n",
             fingerprint + "a=setup:active\r\na=setup:passive\r\n",
             std::string("a=fingerprint:unknown AA:BB\r\na=setup:actpass\r\n"),
             std::string("a=fingerprint:SHA-256\r\na=setup:actpass\r\n"),
             std::string("a=fingerprint:SHA-256 AA:BB\r\na=setup:actpass\r\n"),
             std::string("a=fingerprint:SHA-256 ZZ") + std::string(TEST_FINGERPRINT.substr(2))
                 + "\r\na=setup:actpass\r\n",
             std::string("a=fingerprint:SHA-256 ab") + std::string(TEST_FINGERPRINT.substr(2))
                 + "\r\na=setup:actpass\r\n",
             std::string("a=fingerprint:SHA-256 AB-") + std::string(TEST_FINGERPRINT.substr(3))
                 + "\r\na=setup:actpass\r\n",
             std::string("a=fingerprint:SHA-256 ") + std::string(TEST_FINGERPRINT) + ":\r\na=setup:actpass\r\n",
             std::string("a=fingerprint:SHA-256  ") + std::string(TEST_FINGERPRINT) + "\r\na=setup:actpass\r\n",
         }) {
        Sdp sdp("invalid-dtls");
        configureSdp(sdp, true);
        auto pool = makePool("invalid-dtls");
        const auto raw = dtlsSdp({}, attributes);
        auto* session = parseSdp(pool.get(), raw);
        CPPUNIT_ASSERT(session);
        CPPUNIT_ASSERT(not Sdp::validateDtlsAttributes(session));
        CPPUNIT_ASSERT(Sdp::getMediaAttributeListFromSdp(session).empty());
        sdp.setReceivedOffer(session);
        MediaAttribute audio(MediaType::MEDIA_AUDIO);
        audio.enabled_ = true;
        CPPUNIT_ASSERT(not sdp.processIncomingOffer({audio}));
        CPPUNIT_ASSERT(not sdp.setReceivedOfferFromExternalSdp(raw));
    }
}

void
SrtpHybridSdpTest::bundleUsesNegotiatedTagDtlsParameters()
{
    const auto fingerprint = std::string(TEST_FINGERPRINT) + ":" + std::string(TEST_FINGERPRINT);
    for (const auto& [tagOnly, rejectFirst] :
         {std::pair {false, false}, std::pair {true, false}, std::pair {false, true}}) {
        Sdp sdp("bundle-dtls");
        configureSdp(sdp, true);
        sdp.enableRtcpMux(true);
        sdp.enableBundle(true);
        auto pool = makePool("bundle-dtls");
        const auto group = rejectFirst ? "a=group:BUNDLE first second\r\n" : "a=group:BUNDLE second first\r\n";
        const auto firstAttributes = tagOnly ? std::string {}
                                             : "a=fingerprint:SHA-256 " + std::string(TEST_FINGERPRINT)
                                                   + "\r\na=setup:actpass\r\n";
        const auto raw = dtlsSdp(group, "a=mid:first\r\na=rtcp-mux\r\n" + firstAttributes)
                         + "m=audio 5006 UDP/TLS/RTP/SAVPF 0\r\n"
                           "a=rtpmap:0 PCMU/8000\r\n"
                           "a=mid:second\r\n"
                           "a=rtcp-mux\r\n"
                           "a=fingerprint:SHA-512 "
                         + fingerprint + "\r\na=setup:actpass\r\n";
        auto* offer = parseSdp(pool.get(), raw);
        CPPUNIT_ASSERT(offer);
        CPPUNIT_ASSERT(Sdp::validateDtlsAttributes(offer, SdpDirection::OFFER));
        sdp.setReceivedOffer(offer);
        MediaAttribute first(MediaType::MEDIA_AUDIO);
        first.enabled_ = not rejectFirst;
        MediaAttribute second(MediaType::MEDIA_AUDIO);
        second.enabled_ = true;
        CPPUNIT_ASSERT(sdp.processIncomingOffer({first, second}));
        CPPUNIT_ASSERT(sdp.startNegotiation());
        const auto slots = sdp.getMediaSlots();
        CPPUNIT_ASSERT_EQUAL(size_t(2), slots.size());
        for (unsigned i = rejectFirst ? 1 : 0; i < slots.size(); ++i) {
            const auto& [local, remote] = slots[i];
            CPPUNIT_ASSERT(local.enabled && remote.enabled);
            CPPUNIT_ASSERT_EQUAL(std::string("SHA-512"), remote.dtls_fingerprint_type);
            CPPUNIT_ASSERT_EQUAL(fingerprint, remote.dtls_fingerprint);
            CPPUNIT_ASSERT_EQUAL(DtlsSetup::ACTPASS, remote.dtls_setup);
            CPPUNIT_ASSERT_EQUAL(DtlsSetup::ACTIVE, local.dtls_setup);
            CPPUNIT_ASSERT_EQUAL(std::string(TEST_FINGERPRINT), local.dtls_fingerprint);
        }
        const auto* answer = sdp.getLocalSdpSession();
        if (rejectFirst) {
            CPPUNIT_ASSERT(not slots[0].first.enabled);
        } else {
            CPPUNIT_ASSERT(getMediaAttributes(answer, "fingerprint", 0).empty());
            CPPUNIT_ASSERT(getMediaAttributes(answer, "setup", 0).empty());
        }
        CPPUNIT_ASSERT_EQUAL(size_t(1), getMediaAttributes(answer, "fingerprint", 1).size());
        CPPUNIT_ASSERT_EQUAL(size_t(1), getMediaAttributes(answer, "setup", 1).size());

        Sdp offerer("bundle-dtls-offerer");
        configureSdp(offerer, true);
        CPPUNIT_ASSERT(offerer.createOfferFromExternalSdp(raw));
        offerer.setActiveLocalSdpSession(offerer.getLocalSdpSession());
        offerer.setActiveRemoteSdpSession(answer);
        const auto offeredSlots = offerer.getMediaSlots();
        CPPUNIT_ASSERT_EQUAL(size_t(2), offeredSlots.size());
        for (unsigned i = rejectFirst ? 1 : 0; i < offeredSlots.size(); ++i) {
            const auto& [local, remote] = offeredSlots[i];
            CPPUNIT_ASSERT_EQUAL(fingerprint, local.dtls_fingerprint);
            CPPUNIT_ASSERT_EQUAL(DtlsSetup::PASSIVE, local.dtls_setup);
            CPPUNIT_ASSERT_EQUAL(std::string(TEST_FINGERPRINT), remote.dtls_fingerprint);
            CPPUNIT_ASSERT_EQUAL(DtlsSetup::ACTIVE, remote.dtls_setup);
        }
    }
}

void
SrtpHybridSdpTest::dtlsAttributesRespectMediaOverrides()
{
    const auto fingerprint = "a=fingerprint:SHA-256 " + std::string(TEST_FINGERPRINT) + "\r\n";
    const auto inherited = fingerprint + "a=setup:actpass\r\n";
    auto pool = makePool("dtls-overrides");
    for (const auto& [sessionAttributes, mediaAttributes, valid] : {
             std::tuple {inherited, std::string {}, true},
             std::tuple {inherited, std::string("a=setup:passive\r\n"), true},
             std::tuple {std::string("a=fingerprint:invalid\r\na=setup:invalid\r\n"), inherited, true},
             std::tuple {inherited, std::string("a=setup:invalid\r\n"), false},
             std::tuple {inherited, std::string("a=setup:\r\n"), false},
             std::tuple {inherited, std::string("a=fingerprint:SHA-256 AA:BB\r\n"), false},
             std::tuple {inherited, std::string("a=fingerprint:\r\n"), false},
         }) {
        const auto raw = dtlsSdp(sessionAttributes, mediaAttributes);
        auto* session = parseSdp(pool.get(), raw);
        CPPUNIT_ASSERT(session);
        CPPUNIT_ASSERT_EQUAL(valid, Sdp::validateDtlsAttributes(session));
        const auto media = Sdp::getMediaAttributeListFromSdp(session);
        CPPUNIT_ASSERT_EQUAL(valid, not media.empty());
        if (valid)
            CPPUNIT_ASSERT(media.front().secure_);
    }
}

void
SrtpHybridSdpTest::validatesDtlsHashesAndAnswerRoles()
{
    auto pool = makePool("dtls-hashes");
    for (const auto& [hash, size] :
         {std::pair {"sHa-1", 20u}, std::pair {"sha-256", 32u}, std::pair {"SHA-384", 48u}, std::pair {"SHA-512", 64u}}) {
        std::string fingerprint = "AB";
        for (unsigned i = 1; i < size; ++i)
            fingerprint += ":CD";
        for (const auto* role : {"active", "passive", "actpass", "ACTIVE", "PaSsIvE"}) {
            const auto attributes = "a=fingerprint:" + std::string(hash) + " " + fingerprint + "\r\na=setup:" + role
                                    + "\r\n";
            const auto raw = dtlsSdp({}, attributes);
            auto* session = parseSdp(pool.get(), raw);
            CPPUNIT_ASSERT(session);
            CPPUNIT_ASSERT(Sdp::validateDtlsAttributes(session, SdpDirection::OFFER));
            CPPUNIT_ASSERT_EQUAL(std::string_view(role) != "actpass",
                                 Sdp::validateDtlsAttributes(session, SdpDirection::ANSWER));
            const auto media = Sdp::getMediaAttributeListFromSdp(session);
            CPPUNIT_ASSERT_EQUAL(size_t(1), media.size());
            CPPUNIT_ASSERT(media.front().secure_);
        }
    }

    Sdp sdp("dtls-alternative-fingerprints");
    configureSdp(sdp, true);
    const auto fingerprint = std::string(TEST_FINGERPRINT) + ":" + std::string(TEST_FINGERPRINT);
    const auto raw = dtlsSdp({},
                             "a=fingerprint:unknown AA:BB\r\na=fingerprint:sha-256 " + std::string(TEST_FINGERPRINT)
                                 + "\r\na=fingerprint:SHA-512 " + fingerprint + "\r\na=setup:actpass\r\n");
    auto* session = parseSdp(pool.get(), raw);
    CPPUNIT_ASSERT(session);
    CPPUNIT_ASSERT(Sdp::validateDtlsAttributes(session));
    const auto media = sdp.getMediaDescriptions(session, true);
    CPPUNIT_ASSERT_EQUAL(size_t(1), media.size());
    CPPUNIT_ASSERT_EQUAL(std::string("SHA-512"), media.front().dtls_fingerprint_type);
    CPPUNIT_ASSERT_EQUAL(fingerprint, media.front().dtls_fingerprint);
}

void
SrtpHybridSdpTest::offerAdvertisesHybridSdesAndDtls()
{
    Sdp sdp("hybrid-offer");
    configureSdp(sdp, true);

    MediaAttribute audio(MediaType::MEDIA_AUDIO);
    audio.label_ = "audio_0";
    audio.enabled_ = true;

    CPPUNIT_ASSERT(sdp.createOffer({audio}));
    auto* localSession = sdp.getLocalSdpSession();
    CPPUNIT_ASSERT(localSession);

    // SDES-compatible transport with both key exchanges offered.
    CPPUNIT_ASSERT_EQUAL(std::string("RTP/SAVP"), getMediaTransport(localSession));
    CPPUNIT_ASSERT(!getMediaAttributes(localSession, "crypto").empty());

    const auto fingerprints = getMediaAttributes(localSession, "fingerprint");
    CPPUNIT_ASSERT_EQUAL(size_t(1), fingerprints.size());
    CPPUNIT_ASSERT_EQUAL(std::string("SHA-256 ") + std::string(TEST_FINGERPRINT), fingerprints[0]);

    const auto setup = getMediaAttributes(localSession, "setup");
    CPPUNIT_ASSERT_EQUAL(size_t(1), setup.size());
    CPPUNIT_ASSERT_EQUAL(std::string("actpass"), setup[0]);
}

void
SrtpHybridSdpTest::offerStaysSdesWithoutDtlsIdentity()
{
    Sdp sdp("sdes-only-offer");
    configureSdp(sdp, false);

    MediaAttribute audio(MediaType::MEDIA_AUDIO);
    audio.label_ = "audio_0";
    audio.enabled_ = true;

    CPPUNIT_ASSERT(sdp.createOffer({audio}));
    auto* localSession = sdp.getLocalSdpSession();
    CPPUNIT_ASSERT(localSession);

    CPPUNIT_ASSERT_EQUAL(std::string("RTP/SAVP"), getMediaTransport(localSession));
    CPPUNIT_ASSERT(!getMediaAttributes(localSession, "crypto").empty());
    CPPUNIT_ASSERT(getMediaAttributes(localSession, "fingerprint").empty());
    CPPUNIT_ASSERT(getMediaAttributes(localSession, "setup").empty());
}

void
SrtpHybridSdpTest::answerPrefersDtlsForHybridOffer()
{
    Sdp sdp("hybrid-answer");
    configureSdp(sdp, true);

    auto pool = makePool("hybrid-answer");
    const std::string remoteOffer = "v=0\r\n"
                                    "o=- 0 0 IN IP4 127.0.0.1\r\n"
                                    "s=-\r\n"
                                    "c=IN IP4 127.0.0.1\r\n"
                                    "t=0 0\r\n"
                                    "m=audio 5004 RTP/SAVP 0\r\n"
                                    "a=rtpmap:0 PCMU/8000\r\n"
                                    "a=rtcp-mux\r\n"
                                    "a=crypto:1 AES_CM_128_HMAC_SHA1_80 "
                                    "inline:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\r\n"
                                    "a=fingerprint:SHA-256 "
                                    + std::string(TEST_FINGERPRINT)
                                    + "\r\n"
                                      "a=setup:actpass\r\n"
                                      "a=sendrecv\r\n";

    auto* session = parseSdp(pool.get(), remoteOffer);
    CPPUNIT_ASSERT(session);
    sdp.setReceivedOffer(session);

    MediaAttribute audio(MediaType::MEDIA_AUDIO);
    audio.label_ = "audio_0";
    audio.enabled_ = true;

    CPPUNIT_ASSERT(sdp.processIncomingOffer({audio}));
    auto* localSession = sdp.getLocalSdpSession();
    CPPUNIT_ASSERT(localSession);

    // The answer commits to DTLS: fingerprint and negotiated setup, no SDES crypto.
    const auto fingerprints = getMediaAttributes(localSession, "fingerprint");
    CPPUNIT_ASSERT_EQUAL(size_t(1), fingerprints.size());
    const auto setup = getMediaAttributes(localSession, "setup");
    CPPUNIT_ASSERT_EQUAL(size_t(1), setup.size());
    CPPUNIT_ASSERT_EQUAL(std::string("active"), setup[0]);
    CPPUNIT_ASSERT(getMediaAttributes(localSession, "crypto").empty());

    CPPUNIT_ASSERT(sdp.startNegotiation());

    const auto slots = sdp.getMediaSlots();
    CPPUNIT_ASSERT_EQUAL(size_t(1), slots.size());
    CPPUNIT_ASSERT_EQUAL(KeyExchangeProtocol::DTLS, slots[0].first.key_exchange);
    CPPUNIT_ASSERT_EQUAL(KeyExchangeProtocol::DTLS, slots[0].second.key_exchange);
    CPPUNIT_ASSERT_EQUAL(std::string(TEST_FINGERPRINT), slots[0].first.dtls_fingerprint);
    CPPUNIT_ASSERT_EQUAL(std::string(TEST_FINGERPRINT), slots[0].second.dtls_fingerprint);
    CPPUNIT_ASSERT(slots[0].first.dtls_setup == DtlsSetup::ACTIVE);
}

void
SrtpHybridSdpTest::answerFallsBackToSdesWhenOfferHasNoFingerprint()
{
    Sdp sdp("sdes-answer");
    configureSdp(sdp, true);

    auto pool = makePool("sdes-answer");
    const std::string remoteOffer = "v=0\r\n"
                                    "o=- 0 0 IN IP4 127.0.0.1\r\n"
                                    "s=-\r\n"
                                    "c=IN IP4 127.0.0.1\r\n"
                                    "t=0 0\r\n"
                                    "m=audio 5004 RTP/SAVP 0\r\n"
                                    "a=rtpmap:0 PCMU/8000\r\n"
                                    "a=rtcp-mux\r\n"
                                    "a=crypto:1 AES_CM_128_HMAC_SHA1_80 "
                                    "inline:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\r\n"
                                    "a=sendrecv\r\n";

    auto* session = parseSdp(pool.get(), remoteOffer);
    CPPUNIT_ASSERT(session);
    sdp.setReceivedOffer(session);

    MediaAttribute audio(MediaType::MEDIA_AUDIO);
    audio.label_ = "audio_0";
    audio.enabled_ = true;

    CPPUNIT_ASSERT(sdp.processIncomingOffer({audio}));
    auto* localSession = sdp.getLocalSdpSession();
    CPPUNIT_ASSERT(localSession);

    CPPUNIT_ASSERT(!getMediaAttributes(localSession, "crypto").empty());
    CPPUNIT_ASSERT(getMediaAttributes(localSession, "fingerprint").empty());
    CPPUNIT_ASSERT(getMediaAttributes(localSession, "setup").empty());

    CPPUNIT_ASSERT(sdp.startNegotiation());

    const auto slots = sdp.getMediaSlots();
    CPPUNIT_ASSERT_EQUAL(size_t(1), slots.size());
    CPPUNIT_ASSERT_EQUAL(KeyExchangeProtocol::SDES, slots[0].first.key_exchange);
    CPPUNIT_ASSERT_EQUAL(KeyExchangeProtocol::SDES, slots[0].second.key_exchange);
}


void
SrtpHybridSdpTest::answerPreservesRejectedMediaSections()
{
    Sdp sdp("rejected-media-answer");
    configureSdp(sdp, false);
    sdp.enableBundle(true);

    auto pool = makePool("rejected-media-answer");
    const std::string remoteOffer = "v=0\r\n"
                                   "o=- 0 0 IN IP4 127.0.0.1\r\n"
                                   "s=-\r\n"
                                   "c=IN IP4 127.0.0.1\r\n"
                                   "t=0 0\r\n"
                                   "a=group:BUNDLE video audio stopped\r\n"
                                   "m=video 5006 RTP/SAVPF 120\r\n"
                                   "a=mid:video\r\n"
                                   "a=rtpmap:120 H264/90000\r\n"
                                   "a=crypto:1 UNSUPPORTED inline:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\r\n"
                                   "m=audio 5004 RTP/SAVP 110\r\n"
                                   "a=mid:audio\r\n"
                                   "a=rtpmap:110 PCMU/8000\r\n"
                                   "a=crypto:7 AES_CM_128_HMAC_SHA1_80 "
                                   "inline:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\r\n"
                                   "a=sendrecv\r\n"
                                   "m=audio 0 RTP/SAVP 8\r\n"
                                   "a=mid:stopped\r\n"
                                   "a=rtpmap:8 PCMA/8000\r\n";

    auto* session = parseSdp(pool.get(), remoteOffer);
    CPPUNIT_ASSERT(session);
    sdp.setReceivedOffer(session);

    MediaAttribute video(MediaType::MEDIA_VIDEO);
    video.enabled_ = false;
    MediaAttribute audio(MediaType::MEDIA_AUDIO);
    audio.enabled_ = true;

    CPPUNIT_ASSERT(sdp.processIncomingOffer({video, audio, audio}));
    auto* localSession = sdp.getLocalSdpSession();
    CPPUNIT_ASSERT(localSession);
    CPPUNIT_ASSERT_EQUAL(3u, localSession->media_count);
    for (unsigned i : {0u, 2u}) {
        CPPUNIT_ASSERT_EQUAL(0u, unsigned(localSession->media[i]->desc.port));
        CPPUNIT_ASSERT(pj_strcmp(&session->media[i]->desc.media, &localSession->media[i]->desc.media) == 0);
        CPPUNIT_ASSERT(pj_strcmp(&session->media[i]->desc.transport, &localSession->media[i]->desc.transport) == 0);
        CPPUNIT_ASSERT(pj_strcmp(&session->media[i]->desc.fmt[0], &localSession->media[i]->desc.fmt[0]) == 0);
        CPPUNIT_ASSERT(getMediaAttributes(localSession, "crypto", i).empty());
        CPPUNIT_ASSERT(getMediaAttributes(localSession, "mid", i) == getMediaAttributes(session, "mid", i));
    }

    const auto crypto = getMediaAttributes(localSession, "crypto", 1);
    CPPUNIT_ASSERT_EQUAL(size_t(1), crypto.size());
    CPPUNIT_ASSERT(crypto[0].rfind("7 AES_CM_128_HMAC_SHA1_80 inline:", 0) == 0);
    CPPUNIT_ASSERT(getMediaAttributes(localSession, "mid", 1) == std::vector<std::string> {"audio"});
    auto* group = pjmedia_sdp_attr_find2(localSession->attr_count, localSession->attr, "group", nullptr);
    CPPUNIT_ASSERT(group);
    CPPUNIT_ASSERT(pj_strcmp2(&group->value, "BUNDLE audio") == 0);

    CPPUNIT_ASSERT(sdp.startNegotiation());
    const auto slots = sdp.getMediaSlots();
    CPPUNIT_ASSERT_EQUAL(size_t(3), slots.size());
    CPPUNIT_ASSERT(!slots[0].first.enabled);
    CPPUNIT_ASSERT(slots[1].first.enabled);
    CPPUNIT_ASSERT_EQUAL(KeyExchangeProtocol::SDES, slots[1].first.key_exchange);
    CPPUNIT_ASSERT_EQUAL(KeyExchangeProtocol::SDES, slots[1].second.key_exchange);
    CPPUNIT_ASSERT(!slots[2].first.enabled);
}

void
SrtpHybridSdpTest::offererMatchesReleaseSelectedSdesSuite()
{
    // Regression test: a release ("current master") peer answering our SDES
    // offer advertises a single, hard-tagged crypto line
    // ("1 AES_CM_128_HMAC_SHA1_80"). Our offer lists AES-256 at tag 1 and
    // AES-128 at tag 2, so matching the answer by tag fails. The offerer must
    // still select the local crypto whose *suite* matches the one the peer
    // committed to (AES-128), not silently fall back to a different suite.
    Sdp sdp("release-interop-offer");
    configureSdp(sdp, false);

    MediaAttribute audio(MediaType::MEDIA_AUDIO);
    audio.label_ = "audio_0";
    audio.enabled_ = true;

    CPPUNIT_ASSERT(sdp.createOffer({audio}));
    auto* localSession = sdp.getLocalSdpSession();
    CPPUNIT_ASSERT(localSession);

    // The offer advertises AES-256 (tag 1) and AES-128 (tag 2).
    const auto offeredCrypto = getMediaAttributes(localSession, "crypto");
    CPPUNIT_ASSERT_EQUAL(size_t(2), offeredCrypto.size());

    auto pool = makePool("release-interop-answer");
    // Release peers hardcode the answer to tag 1 / AES-128, regardless of the
    // tag the same suite was offered with.
    const std::string releaseAnswer = "v=0\r\n"
                                       "o=- 0 0 IN IP4 127.0.0.1\r\n"
                                       "s=-\r\n"
                                       "c=IN IP4 127.0.0.1\r\n"
                                       "t=0 0\r\n"
                                       "m=audio 5004 RTP/SAVP 104\r\n"
                                       "a=rtpmap:104 opus/48000/2\r\n"
                                       "a=crypto:1 AES_CM_128_HMAC_SHA1_80 "
                                       "inline:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\r\n"
                                       "a=sendrecv\r\n";

    auto* remoteAnswer = parseSdp(pool.get(), releaseAnswer);
    CPPUNIT_ASSERT(remoteAnswer);

    // pjmedia keeps the offered crypto attributes in the offerer active-local.
    sdp.setActiveLocalSdpSession(localSession);
    sdp.setActiveRemoteSdpSession(remoteAnswer);

    const auto slots = sdp.getMediaSlots();
    CPPUNIT_ASSERT(not slots.empty());
    CPPUNIT_ASSERT_EQUAL(KeyExchangeProtocol::SDES, slots[0].first.key_exchange);
    CPPUNIT_ASSERT(static_cast<bool>(slots[0].first.crypto));
    CPPUNIT_ASSERT_EQUAL(std::string("AES_CM_128_HMAC_SHA1_80"),
                         slots[0].first.crypto.getCryptoSuite());
}

void
SrtpHybridSdpTest::legacyFallbackOfferUsesReleaseCryptoTag()
{
    Sdp sdp("legacy-fallback-offer");
    configureSdp(sdp, true);

    MediaAttribute audio(MediaType::MEDIA_AUDIO);
    audio.label_ = "audio_0";
    audio.enabled_ = true;

    CPPUNIT_ASSERT(sdp.createOffer({audio}, nullptr, true));
    const auto crypto = getMediaAttributes(sdp.getLocalSdpSession(), "crypto");
    CPPUNIT_ASSERT_EQUAL(size_t(1), crypto.size());
    CPPUNIT_ASSERT(crypto[0].rfind("1 AES_CM_128_HMAC_SHA1_80 inline:", 0) == 0);
}

void
SrtpHybridSdpTest::offerAdvertisesSdesWithDefaultKeyExchange()
{
    // Jami accounts enable SRTP but report no explicit key exchange
    // (KeyExchangeProtocol::NONE). The offer must still advertise SDES crypto,
    // otherwise calling a release peer fails media negotiation with
    // "no usable local parameters".
    Sdp sdp("default-kx-offer");
    sdp.setPublishedIP("127.0.0.1", pj_AF_INET());
    sdp.setLocalMediaCapabilities(MediaType::MEDIA_AUDIO,
                                  account_->getActiveAccountCodecInfoList(MEDIA_AUDIO));
    sdp.setLocalPublishedAudioPorts(TEST_AUDIO_RTP_PORT, 0);
    sdp.setSecureMediaKeyExchange(KeyExchangeProtocol::NONE);

    MediaAttribute audio(MediaType::MEDIA_AUDIO);
    audio.label_ = "audio_0";
    audio.enabled_ = true;

    CPPUNIT_ASSERT(sdp.createOffer({audio}));
    auto* localSession = sdp.getLocalSdpSession();
    CPPUNIT_ASSERT(localSession);

    CPPUNIT_ASSERT_EQUAL(std::string("RTP/SAVP"), getMediaTransport(localSession));
    CPPUNIT_ASSERT(!getMediaAttributes(localSession, "crypto").empty());
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::SrtpHybridSdpTest::name());
