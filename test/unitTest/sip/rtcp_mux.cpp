#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "jami.h"
#include "manager.h"
#include "sip/sdp.h"
#include "sip/sipaccount.h"
#include "sip/sipvoiplink.h"
#include "account_const.h"
#include "jami/media_const.h"
#include "common.h"
#include "test_runner.h"

extern "C" {
#include <pjmedia/sdp.h>
}

using namespace libjami::Account;

namespace jami {
namespace test {

namespace {

constexpr auto TEST_ACCOUNT_ALIAS = "SDP_RTPCMUX_TEST";
constexpr uint16_t TEST_AUDIO_RTP_PORT = 4000;
constexpr uint16_t TEST_AUDIO_RTCP_PORT = 4001;

bool
hasMediaAttribute(const pjmedia_sdp_session* session, std::string_view attributeName)
{
    if (not session or session->media_count == 0)
        return false;

    auto* media = session->media[0];
    for (unsigned i = 0; i < media->attr_count; ++i) {
        auto* attribute = media->attr[i];
        if (attributeName == std::string_view(attribute->name.ptr, attribute->name.slen))
            return true;
    }

    return false;
}

std::unique_ptr<pj_pool_t, std::function<void(pj_pool_t*)>>
makePool(const char* name)
{
    return {pj_pool_create(&Manager::instance().sipVoIPLink().getCachingPool()->factory,
                           name,
                           4096,
                           4096,
                           nullptr),
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

class RtcpMuxSdpTest : public CppUnit::TestFixture
{
public:
    RtcpMuxSdpTest()
    {
        libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
    }

    ~RtcpMuxSdpTest() { libjami::fini(); }

    static std::string name() { return "rtcp_mux"; }

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
    void offerDoesNotAdvertiseRtcpMuxByDefault();
    void offerAdvertisesRtcpMuxWhenEnabled();
    void remoteSdpFallsBackToRtcpNextPort();
    void remoteSdpKeepsMuxOnRtpPort();

    CPPUNIT_TEST_SUITE(RtcpMuxSdpTest);
    CPPUNIT_TEST(offerDoesNotAdvertiseRtcpMuxByDefault);
    CPPUNIT_TEST(offerAdvertisesRtcpMuxWhenEnabled);
    CPPUNIT_TEST(remoteSdpFallsBackToRtcpNextPort);
    CPPUNIT_TEST(remoteSdpKeepsMuxOnRtpPort);
    CPPUNIT_TEST_SUITE_END();

    std::string accountId_ {};
    std::shared_ptr<SIPAccount> account_ {};
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(RtcpMuxSdpTest, RtcpMuxSdpTest::name());

void
RtcpMuxSdpTest::offerDoesNotAdvertiseRtcpMuxByDefault()
{
    CPPUNIT_ASSERT(account_);

    Sdp sdp("rtcp-mux-test-default");
    sdp.setPublishedIP("127.0.0.1", pj_AF_INET());
    sdp.setLocalMediaCapabilities(MediaType::MEDIA_AUDIO, account_->getActiveAccountCodecInfoList(MEDIA_AUDIO));
    sdp.setLocalPublishedAudioPorts(TEST_AUDIO_RTP_PORT, TEST_AUDIO_RTCP_PORT);
    sdp.enableRtcpMux(false);

    MediaAttribute audio(MediaType::MEDIA_AUDIO);
    audio.label_ = "audio_0";
    audio.enabled_ = true;

    CPPUNIT_ASSERT(sdp.createOffer({audio}));
    auto* localSession = sdp.getLocalSdpSession();

    CPPUNIT_ASSERT(localSession);
    CPPUNIT_ASSERT(!hasMediaAttribute(localSession, "rtcp-mux"));

    const auto descriptions = sdp.getMediaDescriptions(localSession, false);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), descriptions.size());
    CPPUNIT_ASSERT(!descriptions[0].rtcp_mux);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint16_t>(TEST_AUDIO_RTCP_PORT), descriptions[0].rtcp_addr.getPort());
}

void
RtcpMuxSdpTest::offerAdvertisesRtcpMuxWhenEnabled()
{
    CPPUNIT_ASSERT(account_);

    Sdp sdp("rtcp-mux-test-enabled");
    sdp.setPublishedIP("127.0.0.1", pj_AF_INET());
    sdp.setLocalMediaCapabilities(MediaType::MEDIA_AUDIO, account_->getActiveAccountCodecInfoList(MEDIA_AUDIO));
    sdp.setLocalPublishedAudioPorts(TEST_AUDIO_RTP_PORT, TEST_AUDIO_RTCP_PORT);
    sdp.enableRtcpMux(true);

    MediaAttribute audio(MediaType::MEDIA_AUDIO);
    audio.label_ = "audio_0";
    audio.enabled_ = true;

    CPPUNIT_ASSERT(sdp.createOffer({audio}));
    auto* localSession = sdp.getLocalSdpSession();

    CPPUNIT_ASSERT(localSession);
    CPPUNIT_ASSERT(hasMediaAttribute(localSession, "rtcp-mux"));

    const auto descriptions = sdp.getMediaDescriptions(localSession, false);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), descriptions.size());
    CPPUNIT_ASSERT(descriptions[0].rtcp_mux);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint16_t>(TEST_AUDIO_RTP_PORT), descriptions[0].rtcp_addr.getPort());
}

void
RtcpMuxSdpTest::remoteSdpFallsBackToRtcpNextPort()
{
    CPPUNIT_ASSERT(account_);

    Sdp sdp("rtcp-mux-test-fallback");
    sdp.setPublishedIP("127.0.0.1", pj_AF_INET());
    sdp.setLocalMediaCapabilities(MediaType::MEDIA_AUDIO, account_->getActiveAccountCodecInfoList(MEDIA_AUDIO));
    sdp.setLocalPublishedAudioPorts(TEST_AUDIO_RTP_PORT, TEST_AUDIO_RTCP_PORT);
    sdp.enableRtcpMux(false);

    MediaAttribute audio(MediaType::MEDIA_AUDIO);
    audio.label_ = "audio_0";
    audio.enabled_ = true;

    CPPUNIT_ASSERT(sdp.createOffer({audio}));
    auto pool = makePool("rtcp-fallback");
    const std::string remoteSdp = "v=0\r\n"
                                  "o=- 0 0 IN IP4 127.0.0.1\r\n"
                                  "s=-\r\n"
                                  "c=IN IP4 127.0.0.1\r\n"
                                  "t=0 0\r\n"
                                  "m=audio 5004 RTP/AVP 0\r\n"
                                  "a=rtpmap:0 PCMU/8000\r\n";

    auto* session = parseSdp(pool.get(), remoteSdp);
    CPPUNIT_ASSERT(session);

    const auto descriptions = sdp.getMediaDescriptions(session, true);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), descriptions.size());
    CPPUNIT_ASSERT(!descriptions[0].rtcp_mux);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint16_t>(5004), descriptions[0].addr.getPort());
    CPPUNIT_ASSERT_EQUAL(static_cast<uint16_t>(5005), descriptions[0].rtcp_addr.getPort());
}

void
RtcpMuxSdpTest::remoteSdpKeepsMuxOnRtpPort()
{
    CPPUNIT_ASSERT(account_);

    Sdp sdp("rtcp-mux-test-remote");
    sdp.setPublishedIP("127.0.0.1", pj_AF_INET());
    sdp.setLocalMediaCapabilities(MediaType::MEDIA_AUDIO, account_->getActiveAccountCodecInfoList(MEDIA_AUDIO));
    sdp.setLocalPublishedAudioPorts(TEST_AUDIO_RTP_PORT, TEST_AUDIO_RTCP_PORT);
    sdp.enableRtcpMux(false);

    MediaAttribute audio(MediaType::MEDIA_AUDIO);
    audio.label_ = "audio_0";
    audio.enabled_ = true;

    CPPUNIT_ASSERT(sdp.createOffer({audio}));
    auto pool = makePool("rtcp-mux-remote");
    const std::string remoteSdp = "v=0\r\n"
                                  "o=- 0 0 IN IP4 127.0.0.1\r\n"
                                  "s=-\r\n"
                                  "c=IN IP4 127.0.0.1\r\n"
                                  "t=0 0\r\n"
                                  "m=audio 5004 RTP/AVP 0\r\n"
                                  "a=rtpmap:0 PCMU/8000\r\n"
                                  "a=rtcp:5005 IN IP4 127.0.0.1\r\n"
                                  "a=rtcp-mux\r\n";

    auto* session = parseSdp(pool.get(), remoteSdp);
    CPPUNIT_ASSERT(session);

    const auto descriptions = sdp.getMediaDescriptions(session, true);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), descriptions.size());
    CPPUNIT_ASSERT(descriptions[0].rtcp_mux);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint16_t>(5004), descriptions[0].addr.getPort());
    CPPUNIT_ASSERT_EQUAL(static_cast<uint16_t>(5004), descriptions[0].rtcp_addr.getPort());
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::RtcpMuxSdpTest::name());