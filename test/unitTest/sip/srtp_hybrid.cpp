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
getMediaAttributes(const pjmedia_sdp_session* session, std::string_view attributeName)
{
    std::vector<std::string> attributes;
    if (not session or session->media_count == 0)
        return attributes;

    auto* media = session->media[0];
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

    CPPUNIT_TEST_SUITE(SrtpHybridSdpTest);
    CPPUNIT_TEST(offerAdvertisesHybridSdesAndDtls);
    CPPUNIT_TEST(offerStaysSdesWithoutDtlsIdentity);
    CPPUNIT_TEST(answerPrefersDtlsForHybridOffer);
    CPPUNIT_TEST(answerFallsBackToSdesWhenOfferHasNoFingerprint);
    CPPUNIT_TEST_SUITE_END();

    void configureSdp(Sdp& sdp, bool withDtlsIdentity)
    {
        sdp.setPublishedIP("127.0.0.1", pj_AF_INET());
        sdp.setLocalMediaCapabilities(MediaType::MEDIA_AUDIO, account_->getActiveAccountCodecInfoList(MEDIA_AUDIO));
        sdp.setLocalPublishedAudioPorts(TEST_AUDIO_RTP_PORT, 0);
        sdp.enableRtcpMux(true);
        sdp.setSecureMediaKeyExchange(KeyExchangeProtocol::SDES);
        if (withDtlsIdentity)
            sdp.setLocalDtlsFingerprint("SHA-256", std::string(TEST_FINGERPRINT));
    }

    std::string accountId_ {};
    std::shared_ptr<SIPAccount> account_ {};
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(SrtpHybridSdpTest, SrtpHybridSdpTest::name());

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

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::SrtpHybridSdpTest::name());
