#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <array>
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

constexpr std::string_view MID_RTP_EXTENSION_URI {"urn:ietf:params:rtp-hdrext:sdes:mid"};

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

bool
hasSessionAttribute(const pjmedia_sdp_session* session, std::string_view attributeName, std::string_view valuePart = {})
{
    if (not session)
        return false;

    for (unsigned i = 0; i < session->attr_count; ++i) {
        auto* attribute = session->attr[i];
        if (attributeName != std::string_view(attribute->name.ptr, attribute->name.slen))
            continue;

        if (valuePart.empty())
            return true;

        const std::string_view value(attribute->value.ptr, attribute->value.slen);
        if (value.find(valuePart) != std::string_view::npos)
            return true;
    }

    return false;
}

bool
hasMediaAttributeValue(const pjmedia_sdp_media* media, std::string_view attributeName, std::string_view value)
{
    if (not media)
        return false;

    for (unsigned i = 0; i < media->attr_count; ++i) {
        auto* attribute = media->attr[i];
        if (attributeName != std::string_view(attribute->name.ptr, attribute->name.slen))
            continue;
        if (value == std::string_view(attribute->value.ptr, attribute->value.slen))
            return true;
    }

    return false;
}

bool
hasMediaAttributeName(const pjmedia_sdp_media* media, std::string_view attributeName)
{
    if (not media)
        return false;

    for (unsigned i = 0; i < media->attr_count; ++i) {
        auto* attribute = media->attr[i];
        if (attributeName == std::string_view(attribute->name.ptr, attribute->name.slen))
            return true;
    }

    return false;
}

std::string
printSdp(const pjmedia_sdp_session* session)
{
    std::array<char, 4096> buffer {};
    const auto size = pjmedia_sdp_print(session, buffer.data(), buffer.size());
    return size > 0 ? std::string(buffer.data(), static_cast<size_t>(size)) : std::string {};
}

void
replaceFirst(std::string& haystack, std::string_view needle, std::string_view replacement)
{
    const auto pos = haystack.find(needle);
    CPPUNIT_ASSERT(pos != std::string::npos);
    haystack.replace(pos, needle.size(), replacement);
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
    void offerAdvertisesRtcpMuxByDefault();
    void offerSkipsRtcpMuxWhenDisabled();
    void offerAdvertisesRtcpMuxWhenEnabled();
    void offerAdvertisesBundleAndMid();
    void offerAdvertisesPliAndFirForVideoOnly();
    void offerAdvertisesH264PacketizationMode1();
    void answerNegotiatesPliAndFir();
    void answerSkipsPliAndFirWhenOfferDoesNot();
    void answerPreservesRemoteBundleMidAndExtmap();
    void answerNormalizesInconsistentBundleMidExtmap();
    void answerAcceptsSessionLevelBundleMidExtmap();
    void answerSkipsBundleWhenOfferDoesNotAdvertiseIt();
    void answerSkipsRtcpMuxWhenOfferDoesNotAdvertiseIt();
    void answerAdvertisesRtcpMuxWhenOfferDoes();
    void remoteSdpFallsBackToRtcpNextPort();
    void remoteSdpKeepsMuxOnRtpPort();

    CPPUNIT_TEST_SUITE(RtcpMuxSdpTest);
    CPPUNIT_TEST(offerAdvertisesRtcpMuxByDefault);
    CPPUNIT_TEST(offerSkipsRtcpMuxWhenDisabled);
    CPPUNIT_TEST(offerAdvertisesRtcpMuxWhenEnabled);
    CPPUNIT_TEST(offerAdvertisesBundleAndMid);
    CPPUNIT_TEST(offerAdvertisesPliAndFirForVideoOnly);
    CPPUNIT_TEST(offerAdvertisesH264PacketizationMode1);
    CPPUNIT_TEST(answerNegotiatesPliAndFir);
    CPPUNIT_TEST(answerSkipsPliAndFirWhenOfferDoesNot);
    CPPUNIT_TEST(answerPreservesRemoteBundleMidAndExtmap);
    CPPUNIT_TEST(answerNormalizesInconsistentBundleMidExtmap);
    CPPUNIT_TEST(answerAcceptsSessionLevelBundleMidExtmap);
    CPPUNIT_TEST(answerSkipsBundleWhenOfferDoesNotAdvertiseIt);
    CPPUNIT_TEST(answerSkipsRtcpMuxWhenOfferDoesNotAdvertiseIt);
    CPPUNIT_TEST(answerAdvertisesRtcpMuxWhenOfferDoes);
    CPPUNIT_TEST(remoteSdpFallsBackToRtcpNextPort);
    CPPUNIT_TEST(remoteSdpKeepsMuxOnRtpPort);
    CPPUNIT_TEST_SUITE_END();

    std::string accountId_ {};
    std::shared_ptr<SIPAccount> account_ {};
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(RtcpMuxSdpTest, RtcpMuxSdpTest::name());

void
RtcpMuxSdpTest::offerAdvertisesRtcpMuxByDefault()
{
    CPPUNIT_ASSERT(account_);
    CPPUNIT_ASSERT(account_->isRtcpMuxEnabled());

    Sdp sdp("rtcp-mux-test-default");
    sdp.setPublishedIP("127.0.0.1", pj_AF_INET());
    sdp.setLocalMediaCapabilities(MediaType::MEDIA_AUDIO, account_->getActiveAccountCodecInfoList(MEDIA_AUDIO));
    sdp.setLocalPublishedAudioPorts(TEST_AUDIO_RTP_PORT, 0);
    sdp.enableRtcpMux(account_->isRtcpMuxEnabled());

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
RtcpMuxSdpTest::offerSkipsRtcpMuxWhenDisabled()
{
    CPPUNIT_ASSERT(account_);
    CPPUNIT_ASSERT(account_->isRtcpMuxEnabled());

    Sdp sdp("rtcp-mux-test-disabled");
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
RtcpMuxSdpTest::offerAdvertisesBundleAndMid()
{
    CPPUNIT_ASSERT(account_);

    Sdp sdp("bundle-test-offer");
    sdp.setPublishedIP("127.0.0.1", pj_AF_INET());
    sdp.setLocalMediaCapabilities(MediaType::MEDIA_AUDIO, account_->getActiveAccountCodecInfoList(MEDIA_AUDIO));
    sdp.setLocalMediaCapabilities(MediaType::MEDIA_VIDEO, account_->getActiveAccountCodecInfoList(MEDIA_VIDEO));
    sdp.setLocalPublishedAudioPorts(TEST_AUDIO_RTP_PORT, 0);
    sdp.setLocalPublishedVideoPorts(TEST_AUDIO_RTP_PORT, 0);
    sdp.enableRtcpMux(true);
    sdp.enableBundle(true);

    MediaAttribute audio(MediaType::MEDIA_AUDIO);
    audio.label_ = "audio_0";
    audio.enabled_ = true;

    MediaAttribute video(MediaType::MEDIA_VIDEO);
    video.label_ = "video_0";
    video.enabled_ = true;

    CPPUNIT_ASSERT(sdp.createOffer({audio, video}));
    auto* localSession = sdp.getLocalSdpSession();

    CPPUNIT_ASSERT(localSession);
    CPPUNIT_ASSERT_EQUAL(2u, localSession->media_count);
    CPPUNIT_ASSERT(hasSessionAttribute(localSession, "group", "BUNDLE audio_0 video_0"));
    CPPUNIT_ASSERT(hasMediaAttributeValue(localSession->media[0], "mid", "audio_0"));
    CPPUNIT_ASSERT(hasMediaAttributeValue(localSession->media[1], "mid", "video_0"));
    CPPUNIT_ASSERT(hasMediaAttributeValue(localSession->media[0],
                                          "extmap",
                                          std::string("1 ") + std::string(MID_RTP_EXTENSION_URI)));
    CPPUNIT_ASSERT(hasMediaAttributeValue(localSession->media[1],
                                          "extmap",
                                          std::string("1 ") + std::string(MID_RTP_EXTENSION_URI)));

    const auto descriptions = sdp.getMediaDescriptions(localSession, false);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), descriptions.size());
    CPPUNIT_ASSERT_EQUAL(static_cast<uint16_t>(TEST_AUDIO_RTP_PORT), descriptions[0].addr.getPort());
    CPPUNIT_ASSERT_EQUAL(static_cast<uint16_t>(TEST_AUDIO_RTP_PORT), descriptions[1].addr.getPort());
    CPPUNIT_ASSERT_EQUAL(std::string {"audio_0"}, descriptions[0].mid);
    CPPUNIT_ASSERT_EQUAL(std::string {"video_0"}, descriptions[1].mid);
    CPPUNIT_ASSERT_EQUAL(1u, descriptions[0].mid_rtp_ext_id);
    CPPUNIT_ASSERT_EQUAL(1u, descriptions[1].mid_rtp_ext_id);
    CPPUNIT_ASSERT(descriptions[0].payload_type != descriptions[1].payload_type);
    CPPUNIT_ASSERT(descriptions[0].rtcp_mux);
    CPPUNIT_ASSERT(descriptions[1].rtcp_mux);
}

void
RtcpMuxSdpTest::offerAdvertisesPliAndFirForVideoOnly()
{
    CPPUNIT_ASSERT(account_);

    Sdp sdp("pli-fir-offer");
    sdp.setPublishedIP("127.0.0.1", pj_AF_INET());
    sdp.setLocalMediaCapabilities(MediaType::MEDIA_AUDIO, account_->getActiveAccountCodecInfoList(MEDIA_AUDIO));
    sdp.setLocalMediaCapabilities(MediaType::MEDIA_VIDEO, account_->getActiveAccountCodecInfoList(MEDIA_VIDEO));
    sdp.setLocalPublishedAudioPorts(TEST_AUDIO_RTP_PORT, 0);
    sdp.setLocalPublishedVideoPorts(TEST_AUDIO_RTP_PORT, 0);
    sdp.enableRtcpMux(true);

    MediaAttribute audio(MediaType::MEDIA_AUDIO);
    audio.label_ = "audio_0";
    audio.enabled_ = true;

    MediaAttribute video(MediaType::MEDIA_VIDEO);
    video.label_ = "video_0";
    video.enabled_ = true;

    CPPUNIT_ASSERT(sdp.createOffer({audio, video}));
    auto* localSession = sdp.getLocalSdpSession();
    CPPUNIT_ASSERT(localSession);
    CPPUNIT_ASSERT_EQUAL(2u, localSession->media_count);

    CPPUNIT_ASSERT(!hasMediaAttributeValue(localSession->media[0], "rtcp-fb", "* nack pli"));
    CPPUNIT_ASSERT(!hasMediaAttributeValue(localSession->media[0], "rtcp-fb", "* ccm fir"));
    CPPUNIT_ASSERT(hasMediaAttributeValue(localSession->media[1], "rtcp-fb", "* nack pli"));
    CPPUNIT_ASSERT(hasMediaAttributeValue(localSession->media[1], "rtcp-fb", "* ccm fir"));

    const auto descriptions = sdp.getMediaDescriptions(localSession, false);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), descriptions.size());
    CPPUNIT_ASSERT(!descriptions[0].rtcp_fb_nack_pli);
    CPPUNIT_ASSERT(!descriptions[0].rtcp_fb_ccm_fir);
    CPPUNIT_ASSERT(descriptions[1].rtcp_fb_nack_pli);
    CPPUNIT_ASSERT(descriptions[1].rtcp_fb_ccm_fir);
}

void
RtcpMuxSdpTest::offerAdvertisesH264PacketizationMode1()
{
    CPPUNIT_ASSERT(account_);

    Sdp sdp("h264-packetization-mode");
    sdp.setPublishedIP("127.0.0.1", pj_AF_INET());
    sdp.setLocalMediaCapabilities(MediaType::MEDIA_VIDEO, account_->getActiveAccountCodecInfoList(MEDIA_VIDEO));
    sdp.setLocalPublishedVideoPorts(TEST_AUDIO_RTP_PORT, 0);

    MediaAttribute video(MediaType::MEDIA_VIDEO);
    video.label_ = "video_0";
    video.enabled_ = true;

    CPPUNIT_ASSERT(sdp.createOffer({video}));
    auto* localSession = sdp.getLocalSdpSession();
    CPPUNIT_ASSERT(localSession);
    CPPUNIT_ASSERT_EQUAL(1u, localSession->media_count);

    // RFC 6184 5.4: without packetization-mode the single NAL unit mode (0)
    // is assumed, but our RTP payloader emits FU-A fragments for large NALs
    // (mode 1 behavior). Advertise mode 1 so strict receivers such as
    // browsers accept fragmented keyframes.
    auto* media = localSession->media[0];
    bool h264FmtpHasPacketizationMode1 = false;
    for (unsigned i = 0; i < media->attr_count; ++i) {
        auto* attribute = media->attr[i];
        // The fmtp attribute is stored as a single pre-formatted string.
        const std::string_view name(attribute->name.ptr, attribute->name.slen);
        const std::string_view value(attribute->value.ptr, attribute->value.slen);
        const auto attributeText = std::string(name) + " " + std::string(value);
        if (attributeText.find("fmtp") == std::string::npos)
            continue;
        if (attributeText.find("profile-level-id=") != std::string::npos
            && attributeText.find("packetization-mode=1") != std::string::npos)
            h264FmtpHasPacketizationMode1 = true;
    }
    CPPUNIT_ASSERT_MESSAGE(printSdp(localSession), h264FmtpHasPacketizationMode1);
}

void
RtcpMuxSdpTest::answerNegotiatesPliAndFir()
{
    CPPUNIT_ASSERT(account_);

    Sdp sdp("pli-fir-answer");
    sdp.setPublishedIP("127.0.0.1", pj_AF_INET());
    sdp.setLocalMediaCapabilities(MediaType::MEDIA_VIDEO, account_->getActiveAccountCodecInfoList(MEDIA_VIDEO));
    sdp.setLocalPublishedVideoPorts(TEST_AUDIO_RTP_PORT, 0);
    sdp.enableRtcpMux(true);

    auto pool = makePool("pli-fir-answer");
    const std::string remoteOffer = "v=0\r\n"
                                    "o=- 0 0 IN IP4 127.0.0.1\r\n"
                                    "s=-\r\n"
                                    "c=IN IP4 127.0.0.1\r\n"
                                    "t=0 0\r\n"
                                    "m=video 5004 UDP/TLS/RTP/SAVPF 96\r\n"
                                    "a=rtpmap:96 VP8/90000\r\n"
                                    "a=rtcp-mux\r\n"
                                    "a=rtcp-fb:96 nack\r\n"
                                    "a=rtcp-fb:96 nack pli\r\n"
                                    "a=rtcp-fb:96 ccm fir\r\n";

    auto* session = parseSdp(pool.get(), remoteOffer);
    CPPUNIT_ASSERT(session);
    sdp.setReceivedOffer(session);

    MediaAttribute video(MediaType::MEDIA_VIDEO);
    video.label_ = "video_0";
    video.enabled_ = true;

    CPPUNIT_ASSERT(sdp.processIncomingOffer({video}));
    auto* localSession = sdp.getLocalSdpSession();
    CPPUNIT_ASSERT(localSession);
    CPPUNIT_ASSERT_EQUAL(1u, localSession->media_count);
    CPPUNIT_ASSERT(hasMediaAttributeValue(localSession->media[0], "rtcp-fb", "* nack pli"));
    CPPUNIT_ASSERT(hasMediaAttributeValue(localSession->media[0], "rtcp-fb", "* ccm fir"));

    const auto descriptions = sdp.getMediaDescriptions(localSession, false);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), descriptions.size());
    CPPUNIT_ASSERT(descriptions[0].rtcp_fb_nack_pli);
    CPPUNIT_ASSERT(descriptions[0].rtcp_fb_ccm_fir);

    const auto remoteDescriptions = sdp.getMediaDescriptions(session, true);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), remoteDescriptions.size());
    CPPUNIT_ASSERT(remoteDescriptions[0].rtcp_fb_nack_pli);
    CPPUNIT_ASSERT(remoteDescriptions[0].rtcp_fb_ccm_fir);
}

void
RtcpMuxSdpTest::answerSkipsPliAndFirWhenOfferDoesNot()
{
    CPPUNIT_ASSERT(account_);

    Sdp sdp("pli-fir-answer-none");
    sdp.setPublishedIP("127.0.0.1", pj_AF_INET());
    sdp.setLocalMediaCapabilities(MediaType::MEDIA_VIDEO, account_->getActiveAccountCodecInfoList(MEDIA_VIDEO));
    sdp.setLocalPublishedVideoPorts(TEST_AUDIO_RTP_PORT, 0);
    sdp.enableRtcpMux(true);

    auto pool = makePool("pli-fir-answer-none");
    const std::string remoteOffer = "v=0\r\n"
                                    "o=- 0 0 IN IP4 127.0.0.1\r\n"
                                    "s=-\r\n"
                                    "c=IN IP4 127.0.0.1\r\n"
                                    "t=0 0\r\n"
                                    "m=video 5004 UDP/TLS/RTP/SAVPF 96\r\n"
                                    "a=rtpmap:96 VP8/90000\r\n"
                                    "a=rtcp-mux\r\n";

    auto* session = parseSdp(pool.get(), remoteOffer);
    CPPUNIT_ASSERT(session);
    sdp.setReceivedOffer(session);

    MediaAttribute video(MediaType::MEDIA_VIDEO);
    video.label_ = "video_0";
    video.enabled_ = true;

    CPPUNIT_ASSERT(sdp.processIncomingOffer({video}));
    auto* localSession = sdp.getLocalSdpSession();
    CPPUNIT_ASSERT(localSession);
    CPPUNIT_ASSERT_EQUAL(1u, localSession->media_count);
    CPPUNIT_ASSERT(!hasMediaAttributeValue(localSession->media[0], "rtcp-fb", "* nack pli"));
    CPPUNIT_ASSERT(!hasMediaAttributeValue(localSession->media[0], "rtcp-fb", "* ccm fir"));

    const auto descriptions = sdp.getMediaDescriptions(localSession, false);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), descriptions.size());
    CPPUNIT_ASSERT(!descriptions[0].rtcp_fb_nack_pli);
    CPPUNIT_ASSERT(!descriptions[0].rtcp_fb_ccm_fir);
}

void
RtcpMuxSdpTest::answerPreservesRemoteBundleMidAndExtmap()
{
    CPPUNIT_ASSERT(account_);

    Sdp remoteOfferBuilder("bundle-answer-reference");
    remoteOfferBuilder.setPublishedIP("127.0.0.1", pj_AF_INET());
    remoteOfferBuilder.setLocalMediaCapabilities(MediaType::MEDIA_AUDIO,
                                                 account_->getActiveAccountCodecInfoList(MEDIA_AUDIO));
    remoteOfferBuilder.setLocalMediaCapabilities(MediaType::MEDIA_VIDEO,
                                                 account_->getActiveAccountCodecInfoList(MEDIA_VIDEO));
    remoteOfferBuilder.setLocalPublishedAudioPorts(5004, 0);
    remoteOfferBuilder.setLocalPublishedVideoPorts(5004, 0);
    remoteOfferBuilder.enableRtcpMux(true);
    remoteOfferBuilder.enableBundle(true);

    MediaAttribute remoteAudio(MediaType::MEDIA_AUDIO);
    remoteAudio.label_ = "audio_0";
    remoteAudio.enabled_ = true;

    MediaAttribute remoteVideo(MediaType::MEDIA_VIDEO);
    remoteVideo.label_ = "video_0";
    remoteVideo.enabled_ = true;

    CPPUNIT_ASSERT(remoteOfferBuilder.createOffer({remoteAudio, remoteVideo}));
    auto remoteOffer = printSdp(remoteOfferBuilder.getLocalSdpSession());
    CPPUNIT_ASSERT(!remoteOffer.empty());

    replaceFirst(remoteOffer, "a=group:BUNDLE audio_0 video_0\r\n", "a=group:BUNDLE 0 1\r\n");
    replaceFirst(remoteOffer, "a=mid:audio_0\r\n", "a=mid:0\r\n");
    replaceFirst(remoteOffer, "a=mid:video_0\r\n", "a=mid:1\r\n");
    replaceFirst(remoteOffer,
                 std::string("a=extmap:1 ") + std::string(MID_RTP_EXTENSION_URI) + "\r\n",
                 std::string("a=extmap:3 ") + std::string(MID_RTP_EXTENSION_URI) + "\r\n");
    replaceFirst(remoteOffer,
                 std::string("a=extmap:1 ") + std::string(MID_RTP_EXTENSION_URI) + "\r\n",
                 std::string("a=extmap:3 ") + std::string(MID_RTP_EXTENSION_URI) + "\r\n");

    Sdp sdp("bundle-answer-test");
    sdp.setPublishedIP("127.0.0.1", pj_AF_INET());
    sdp.setLocalMediaCapabilities(MediaType::MEDIA_AUDIO, account_->getActiveAccountCodecInfoList(MEDIA_AUDIO));
    sdp.setLocalMediaCapabilities(MediaType::MEDIA_VIDEO, account_->getActiveAccountCodecInfoList(MEDIA_VIDEO));
    sdp.setLocalPublishedAudioPorts(TEST_AUDIO_RTP_PORT, 0);
    sdp.setLocalPublishedVideoPorts(TEST_AUDIO_RTP_PORT, 0);
    sdp.enableRtcpMux(true);
    sdp.enableBundle(true);

    auto pool = makePool("bundle-answer-mid-extmap");
    auto* session = parseSdp(pool.get(), remoteOffer);
    CPPUNIT_ASSERT(session);
    sdp.setReceivedOffer(session);

    MediaAttribute audio(MediaType::MEDIA_AUDIO);
    audio.label_ = "audio_0";
    audio.enabled_ = true;

    MediaAttribute video(MediaType::MEDIA_VIDEO);
    video.label_ = "video_0";
    video.enabled_ = true;

    CPPUNIT_ASSERT(sdp.processIncomingOffer({audio, video}));
    auto* localSession = sdp.getLocalSdpSession();

    CPPUNIT_ASSERT(localSession);
    CPPUNIT_ASSERT_EQUAL(2u, localSession->media_count);
    CPPUNIT_ASSERT(hasSessionAttribute(localSession, "group", "BUNDLE 0 1"));
    CPPUNIT_ASSERT(hasMediaAttributeValue(localSession->media[0], "mid", "0"));
    CPPUNIT_ASSERT(hasMediaAttributeValue(localSession->media[1], "mid", "1"));
    CPPUNIT_ASSERT(hasMediaAttributeValue(localSession->media[0],
                                          "extmap",
                                          std::string("3 ") + std::string(MID_RTP_EXTENSION_URI)));
    CPPUNIT_ASSERT(hasMediaAttributeValue(localSession->media[1],
                                          "extmap",
                                          std::string("3 ") + std::string(MID_RTP_EXTENSION_URI)));

    const auto descriptions = sdp.getMediaDescriptions(localSession, false);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), descriptions.size());
    CPPUNIT_ASSERT_EQUAL(std::string {"0"}, descriptions[0].mid);
    CPPUNIT_ASSERT_EQUAL(std::string {"1"}, descriptions[1].mid);
    CPPUNIT_ASSERT_EQUAL(3u, descriptions[0].mid_rtp_ext_id);
    CPPUNIT_ASSERT_EQUAL(3u, descriptions[1].mid_rtp_ext_id);
}

void
RtcpMuxSdpTest::answerNormalizesInconsistentBundleMidExtmap()
{
    CPPUNIT_ASSERT(account_);

    Sdp remoteOfferBuilder("bundle-answer-inconsistent-extmap");
    remoteOfferBuilder.setPublishedIP("127.0.0.1", pj_AF_INET());
    remoteOfferBuilder.setLocalMediaCapabilities(MediaType::MEDIA_AUDIO,
                                                 account_->getActiveAccountCodecInfoList(MEDIA_AUDIO));
    remoteOfferBuilder.setLocalMediaCapabilities(MediaType::MEDIA_VIDEO,
                                                 account_->getActiveAccountCodecInfoList(MEDIA_VIDEO));
    remoteOfferBuilder.setLocalPublishedAudioPorts(5004, 0);
    remoteOfferBuilder.setLocalPublishedVideoPorts(5004, 0);
    remoteOfferBuilder.enableRtcpMux(true);
    remoteOfferBuilder.enableBundle(true);

    MediaAttribute remoteAudio(MediaType::MEDIA_AUDIO);
    remoteAudio.label_ = "audio_0";
    remoteAudio.enabled_ = true;

    MediaAttribute remoteVideo(MediaType::MEDIA_VIDEO);
    remoteVideo.label_ = "video_0";
    remoteVideo.enabled_ = true;

    CPPUNIT_ASSERT(remoteOfferBuilder.createOffer({remoteAudio, remoteVideo}));
    auto remoteOffer = printSdp(remoteOfferBuilder.getLocalSdpSession());
    CPPUNIT_ASSERT(!remoteOffer.empty());

    replaceFirst(remoteOffer, "a=group:BUNDLE audio_0 video_0\r\n", "a=group:BUNDLE 0 1\r\n");
    replaceFirst(remoteOffer, "a=mid:audio_0\r\n", "a=mid:0\r\n");
    replaceFirst(remoteOffer, "a=mid:video_0\r\n", "a=mid:1\r\n");
    replaceFirst(remoteOffer,
                 std::string("a=extmap:1 ") + std::string(MID_RTP_EXTENSION_URI) + "\r\n",
                 std::string("a=extmap:3 ") + std::string(MID_RTP_EXTENSION_URI) + "\r\n");
    replaceFirst(remoteOffer,
                 std::string("a=extmap:1 ") + std::string(MID_RTP_EXTENSION_URI) + "\r\n",
                 std::string("a=extmap:4 ") + std::string(MID_RTP_EXTENSION_URI) + "\r\n");

    Sdp sdp("bundle-answer-normalized-extmap");
    sdp.setPublishedIP("127.0.0.1", pj_AF_INET());
    sdp.setLocalMediaCapabilities(MediaType::MEDIA_AUDIO, account_->getActiveAccountCodecInfoList(MEDIA_AUDIO));
    sdp.setLocalMediaCapabilities(MediaType::MEDIA_VIDEO, account_->getActiveAccountCodecInfoList(MEDIA_VIDEO));
    sdp.setLocalPublishedAudioPorts(TEST_AUDIO_RTP_PORT, 0);
    sdp.setLocalPublishedVideoPorts(TEST_AUDIO_RTP_PORT, 0);
    sdp.enableRtcpMux(true);
    sdp.enableBundle(true);

    auto pool = makePool("bundle-answer-normalized-mid-extmap");
    auto* session = parseSdp(pool.get(), remoteOffer);
    CPPUNIT_ASSERT(session);
    sdp.setReceivedOffer(session);

    MediaAttribute audio(MediaType::MEDIA_AUDIO);
    audio.label_ = "audio_0";
    audio.enabled_ = true;

    MediaAttribute video(MediaType::MEDIA_VIDEO);
    video.label_ = "video_0";
    video.enabled_ = true;

    CPPUNIT_ASSERT(sdp.processIncomingOffer({audio, video}));
    auto* localSession = sdp.getLocalSdpSession();

    CPPUNIT_ASSERT(localSession);
    CPPUNIT_ASSERT_EQUAL(2u, localSession->media_count);
    CPPUNIT_ASSERT(hasMediaAttributeValue(localSession->media[0],
                                          "extmap",
                                          std::string("3 ") + std::string(MID_RTP_EXTENSION_URI)));
    CPPUNIT_ASSERT(hasMediaAttributeValue(localSession->media[1],
                                          "extmap",
                                          std::string("3 ") + std::string(MID_RTP_EXTENSION_URI)));

    const auto descriptions = sdp.getMediaDescriptions(localSession, false);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), descriptions.size());
    CPPUNIT_ASSERT_EQUAL(3u, descriptions[0].mid_rtp_ext_id);
    CPPUNIT_ASSERT_EQUAL(3u, descriptions[1].mid_rtp_ext_id);
}

void
RtcpMuxSdpTest::answerAcceptsSessionLevelBundleMidExtmap()
{
    CPPUNIT_ASSERT(account_);

    Sdp remoteOfferBuilder("bundle-answer-session-level-extmap");
    remoteOfferBuilder.setPublishedIP("127.0.0.1", pj_AF_INET());
    remoteOfferBuilder.setLocalMediaCapabilities(MediaType::MEDIA_AUDIO,
                                                 account_->getActiveAccountCodecInfoList(MEDIA_AUDIO));
    remoteOfferBuilder.setLocalMediaCapabilities(MediaType::MEDIA_VIDEO,
                                                 account_->getActiveAccountCodecInfoList(MEDIA_VIDEO));
    remoteOfferBuilder.setLocalPublishedAudioPorts(5004, 0);
    remoteOfferBuilder.setLocalPublishedVideoPorts(5004, 0);
    remoteOfferBuilder.enableRtcpMux(true);
    remoteOfferBuilder.enableBundle(true);

    MediaAttribute remoteAudio(MediaType::MEDIA_AUDIO);
    remoteAudio.label_ = "audio_0";
    remoteAudio.enabled_ = true;

    MediaAttribute remoteVideo(MediaType::MEDIA_VIDEO);
    remoteVideo.label_ = "video_0";
    remoteVideo.enabled_ = true;

    CPPUNIT_ASSERT(remoteOfferBuilder.createOffer({remoteAudio, remoteVideo}));
    auto remoteOffer = printSdp(remoteOfferBuilder.getLocalSdpSession());
    CPPUNIT_ASSERT(!remoteOffer.empty());

    replaceFirst(remoteOffer,
                 "a=group:BUNDLE audio_0 video_0\r\n",
                 std::string("a=group:BUNDLE 0 1\r\na=extmap:3 ") + std::string(MID_RTP_EXTENSION_URI) + "\r\n");
    replaceFirst(remoteOffer, "a=mid:audio_0\r\n", "a=mid:0\r\n");
    replaceFirst(remoteOffer, "a=mid:video_0\r\n", "a=mid:1\r\n");
    replaceFirst(remoteOffer, std::string("a=extmap:1 ") + std::string(MID_RTP_EXTENSION_URI) + "\r\n", "");
    replaceFirst(remoteOffer, std::string("a=extmap:1 ") + std::string(MID_RTP_EXTENSION_URI) + "\r\n", "");

    Sdp sdp("bundle-answer-session-level-extmap");
    sdp.setPublishedIP("127.0.0.1", pj_AF_INET());
    sdp.setLocalMediaCapabilities(MediaType::MEDIA_AUDIO, account_->getActiveAccountCodecInfoList(MEDIA_AUDIO));
    sdp.setLocalMediaCapabilities(MediaType::MEDIA_VIDEO, account_->getActiveAccountCodecInfoList(MEDIA_VIDEO));
    sdp.setLocalPublishedAudioPorts(TEST_AUDIO_RTP_PORT, 0);
    sdp.setLocalPublishedVideoPorts(TEST_AUDIO_RTP_PORT, 0);
    sdp.enableRtcpMux(true);
    sdp.enableBundle(true);

    auto pool = makePool("bundle-answer-session-level-extmap");
    auto* session = parseSdp(pool.get(), remoteOffer);
    CPPUNIT_ASSERT(session);
    sdp.setReceivedOffer(session);

    MediaAttribute audio(MediaType::MEDIA_AUDIO);
    audio.label_ = "audio_0";
    audio.enabled_ = true;

    MediaAttribute video(MediaType::MEDIA_VIDEO);
    video.label_ = "video_0";
    video.enabled_ = true;

    const auto remoteDescriptions = sdp.getMediaDescriptions(session, true);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), remoteDescriptions.size());
    CPPUNIT_ASSERT_EQUAL(3u, remoteDescriptions[0].mid_rtp_ext_id);
    CPPUNIT_ASSERT_EQUAL(3u, remoteDescriptions[1].mid_rtp_ext_id);

    CPPUNIT_ASSERT(sdp.processIncomingOffer({audio, video}));
    auto* localSession = sdp.getLocalSdpSession();
    CPPUNIT_ASSERT(localSession);
    CPPUNIT_ASSERT(hasMediaAttributeValue(localSession->media[0],
                                          "extmap",
                                          std::string("3 ") + std::string(MID_RTP_EXTENSION_URI)));
    CPPUNIT_ASSERT(hasMediaAttributeValue(localSession->media[1],
                                          "extmap",
                                          std::string("3 ") + std::string(MID_RTP_EXTENSION_URI)));

    const auto descriptions = sdp.getMediaDescriptions(localSession, false);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), descriptions.size());
    CPPUNIT_ASSERT_EQUAL(3u, descriptions[0].mid_rtp_ext_id);
    CPPUNIT_ASSERT_EQUAL(3u, descriptions[1].mid_rtp_ext_id);
}

void
RtcpMuxSdpTest::answerSkipsBundleWhenOfferDoesNotAdvertiseIt()
{
    CPPUNIT_ASSERT(account_);

    Sdp remoteOfferBuilder("answer-without-bundle-offer");
    remoteOfferBuilder.setPublishedIP("127.0.0.1", pj_AF_INET());
    remoteOfferBuilder.setLocalMediaCapabilities(MediaType::MEDIA_AUDIO,
                                                 account_->getActiveAccountCodecInfoList(MEDIA_AUDIO));
    remoteOfferBuilder.setLocalMediaCapabilities(MediaType::MEDIA_VIDEO,
                                                 account_->getActiveAccountCodecInfoList(MEDIA_VIDEO));
    remoteOfferBuilder.setLocalPublishedAudioPorts(5004, 5005);
    remoteOfferBuilder.setLocalPublishedVideoPorts(5006, 5007);
    remoteOfferBuilder.enableRtcpMux(true);

    MediaAttribute remoteAudio(MediaType::MEDIA_AUDIO);
    remoteAudio.label_ = "audio_0";
    remoteAudio.enabled_ = true;

    MediaAttribute remoteVideo(MediaType::MEDIA_VIDEO);
    remoteVideo.label_ = "video_0";
    remoteVideo.enabled_ = true;

    CPPUNIT_ASSERT(remoteOfferBuilder.createOffer({remoteAudio, remoteVideo}));
    auto* remoteSession = remoteOfferBuilder.getLocalSdpSession();
    CPPUNIT_ASSERT(remoteSession);
    CPPUNIT_ASSERT(!hasSessionAttribute(remoteSession, "group", "BUNDLE"));

    Sdp sdp("answer-without-bundle-offer");
    sdp.setPublishedIP("127.0.0.1", pj_AF_INET());
    sdp.setLocalMediaCapabilities(MediaType::MEDIA_AUDIO, account_->getActiveAccountCodecInfoList(MEDIA_AUDIO));
    sdp.setLocalMediaCapabilities(MediaType::MEDIA_VIDEO, account_->getActiveAccountCodecInfoList(MEDIA_VIDEO));
    sdp.setLocalPublishedAudioPorts(TEST_AUDIO_RTP_PORT, 0);
    sdp.setLocalPublishedVideoPorts(TEST_AUDIO_RTP_PORT + 2, 0);
    sdp.enableRtcpMux(true);
    sdp.enableBundle(true);
    sdp.setReceivedOffer(remoteSession);

    MediaAttribute audio(MediaType::MEDIA_AUDIO);
    audio.label_ = "audio_0";
    audio.enabled_ = true;

    MediaAttribute video(MediaType::MEDIA_VIDEO);
    video.label_ = "video_0";
    video.enabled_ = true;

    CPPUNIT_ASSERT(sdp.processIncomingOffer({audio, video}));
    auto* localSession = sdp.getLocalSdpSession();
    CPPUNIT_ASSERT(localSession);
    CPPUNIT_ASSERT(!hasSessionAttribute(localSession, "group", "BUNDLE"));
    CPPUNIT_ASSERT(!hasMediaAttributeName(localSession->media[0], "mid"));
    CPPUNIT_ASSERT(!hasMediaAttributeName(localSession->media[1], "mid"));
    CPPUNIT_ASSERT(!hasMediaAttributeName(localSession->media[0], "extmap"));
    CPPUNIT_ASSERT(!hasMediaAttributeName(localSession->media[1], "extmap"));

    const auto descriptions = sdp.getMediaDescriptions(localSession, false);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), descriptions.size());
    CPPUNIT_ASSERT(descriptions[0].mid.empty());
    CPPUNIT_ASSERT(descriptions[1].mid.empty());
    CPPUNIT_ASSERT_EQUAL(0u, descriptions[0].mid_rtp_ext_id);
    CPPUNIT_ASSERT_EQUAL(0u, descriptions[1].mid_rtp_ext_id);
}

void
RtcpMuxSdpTest::answerSkipsRtcpMuxWhenOfferDoesNotAdvertiseIt()
{
    CPPUNIT_ASSERT(account_);

    Sdp sdp("rtcp-mux-test-answer-legacy");
    sdp.setPublishedIP("127.0.0.1", pj_AF_INET());
    sdp.setLocalMediaCapabilities(MediaType::MEDIA_AUDIO, account_->getActiveAccountCodecInfoList(MEDIA_AUDIO));
    sdp.setLocalPublishedAudioPorts(TEST_AUDIO_RTP_PORT, TEST_AUDIO_RTCP_PORT);
    sdp.enableRtcpMux(true);

    auto pool = makePool("rtcp-mux-answer-legacy");
    const std::string remoteOffer = "v=0\r\n"
                                    "o=- 0 0 IN IP4 127.0.0.1\r\n"
                                    "s=-\r\n"
                                    "c=IN IP4 127.0.0.1\r\n"
                                    "t=0 0\r\n"
                                    "m=audio 5004 RTP/AVP 0\r\n"
                                    "a=rtpmap:0 PCMU/8000\r\n";

    auto* session = parseSdp(pool.get(), remoteOffer);
    CPPUNIT_ASSERT(session);
    sdp.setReceivedOffer(session);

    MediaAttribute audio(MediaType::MEDIA_AUDIO);
    audio.label_ = "audio_0";
    audio.enabled_ = true;

    CPPUNIT_ASSERT(sdp.processIncomingOffer({audio}));
    auto* localSession = sdp.getLocalSdpSession();
    CPPUNIT_ASSERT(localSession);
    CPPUNIT_ASSERT(!hasMediaAttribute(localSession, "rtcp-mux"));
    CPPUNIT_ASSERT(hasMediaAttribute(localSession, "rtcp"));

    const auto descriptions = sdp.getMediaDescriptions(localSession, false);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), descriptions.size());
    CPPUNIT_ASSERT(!descriptions[0].rtcp_mux);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint16_t>(TEST_AUDIO_RTCP_PORT), descriptions[0].rtcp_addr.getPort());
}

void
RtcpMuxSdpTest::answerAdvertisesRtcpMuxWhenOfferDoes()
{
    CPPUNIT_ASSERT(account_);

    Sdp sdp("rtcp-mux-test-answer-mux");
    sdp.setPublishedIP("127.0.0.1", pj_AF_INET());
    sdp.setLocalMediaCapabilities(MediaType::MEDIA_AUDIO, account_->getActiveAccountCodecInfoList(MEDIA_AUDIO));
    sdp.setLocalPublishedAudioPorts(TEST_AUDIO_RTP_PORT, 0);
    sdp.enableRtcpMux(true);

    auto pool = makePool("rtcp-mux-answer-mux");
    const std::string remoteOffer = "v=0\r\n"
                                    "o=- 0 0 IN IP4 127.0.0.1\r\n"
                                    "s=-\r\n"
                                    "c=IN IP4 127.0.0.1\r\n"
                                    "t=0 0\r\n"
                                    "m=audio 5004 RTP/AVP 0\r\n"
                                    "a=rtpmap:0 PCMU/8000\r\n"
                                    "a=rtcp-mux\r\n";

    auto* session = parseSdp(pool.get(), remoteOffer);
    CPPUNIT_ASSERT(session);
    sdp.setReceivedOffer(session);

    MediaAttribute audio(MediaType::MEDIA_AUDIO);
    audio.label_ = "audio_0";
    audio.enabled_ = true;

    CPPUNIT_ASSERT(sdp.processIncomingOffer({audio}));
    auto* localSession = sdp.getLocalSdpSession();
    CPPUNIT_ASSERT(localSession);
    CPPUNIT_ASSERT(hasMediaAttribute(localSession, "rtcp-mux"));
    CPPUNIT_ASSERT(!hasMediaAttribute(localSession, "rtcp"));

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