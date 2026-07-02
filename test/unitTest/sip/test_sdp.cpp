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
#include "media/media_attribute.h"
#include "media/system_codec_container.h"
#include "media/h265_profile.h"
#include "sip/sdp.h"
#include "sip/sipvoiplink.h"
#include "connectivity/sip_utils.h"

#include "../../test_runner.h"

namespace jami {
namespace test {

class SDPTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "sdp"; }

    void setUp();
    void tearDown();

private:
    void testCameraOfferOrdersBaselineFirst();
    void testScreenShareOfferOrdersHighProfileFirst();
    void testScreenShareNegotiatesHighProfile();
    void testCameraNegotiationKeepsBaseline();
    void testOfferAdvertisesLevelAsymmetry();
    void testAnswerClampsLevelWithoutAsymmetry();
    void testAnswerKeepsLevelWithAsymmetry();
    void testSendLevelClampedWhenSymmetric();
    void testH265OfferAdvertisesProfiles();
    void testH265AnswerMatchesProfile();
    void testH265AnswerClampsLevel();

    CPPUNIT_TEST_SUITE(SDPTest);
    CPPUNIT_TEST(testCameraOfferOrdersBaselineFirst);
    CPPUNIT_TEST(testScreenShareOfferOrdersHighProfileFirst);
    CPPUNIT_TEST(testScreenShareNegotiatesHighProfile);
    CPPUNIT_TEST(testCameraNegotiationKeepsBaseline);
    CPPUNIT_TEST(testOfferAdvertisesLevelAsymmetry);
    CPPUNIT_TEST(testAnswerClampsLevelWithoutAsymmetry);
    CPPUNIT_TEST(testAnswerKeepsLevelWithAsymmetry);
    CPPUNIT_TEST(testSendLevelClampedWhenSymmetric);
    CPPUNIT_TEST(testH265OfferAdvertisesProfiles);
    CPPUNIT_TEST(testH265AnswerMatchesProfile);
    CPPUNIT_TEST(testH265AnswerClampsLevel);
    CPPUNIT_TEST_SUITE_END();

    std::unique_ptr<Sdp> createSdp(const std::string& id);
    std::unique_ptr<Sdp> createH265Sdp(const std::string& id);
    static const pjmedia_sdp_media* videoMedia(const pjmedia_sdp_session* session);
    static std::string encodingName(const pjmedia_sdp_media* media, unsigned fmtIdx);
    static std::string fmtpParams(const pjmedia_sdp_media* media, unsigned fmtIdx);
    const pjmedia_sdp_session* parseRemoteOffer(const std::string& mediaLines);

    std::vector<std::shared_ptr<SystemCodecInfo>> videoCodecs_;
    sip_utils::PoolPtr testPool_;
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(SDPTest, SDPTest::name());

static constexpr auto CAMERA_URI = "camera:///dev/video0";
static constexpr auto SCREEN_URI = "display://:0";

void
SDPTest::setUp()
{
    // Initialize the daemon only once for the whole suite: the SIP link
    // (and its pj memory pools used by Sdp) does not survive an
    // init/fini cycle within the same process.
    if (not Manager::instance().initialized) {
        libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
        std::atexit([] { libjami::fini(); });
    }

    auto codecs = std::make_shared<SystemCodecContainer>();
    codecs->init(false);
    auto h264 = codecs->searchCodecByName("H264", MEDIA_VIDEO);
    CPPUNIT_ASSERT(h264);
    videoCodecs_ = {h264};
}

void
SDPTest::tearDown()
{
    videoCodecs_.clear();
    testPool_.reset();
    Manager::instance().videoPreferences.setEncodingAccelerated(false);
}

std::unique_ptr<Sdp>
SDPTest::createSdp(const std::string& id)
{
    auto sdp = std::make_unique<Sdp>(id);
    sdp->setPublishedIP("127.0.0.1");
    sdp->setLocalPublishedVideoPorts(50000, 50001);
    sdp->setLocalMediaCapabilities(MEDIA_VIDEO, videoCodecs_);
    return sdp;
}

std::unique_ptr<Sdp>
SDPTest::createH265Sdp(const std::string& id)
{
    // Build the codec directly: the container disables H265 when no
    // hardware encoder is available, but SDP negotiation must be
    // testable independently of the local hardware. The H265 filter in
    // setLocalMediaCapabilities follows the acceleration preference.
    Manager::instance().videoPreferences.setEncodingAccelerated(true);
    auto h265 = std::make_shared<SystemVideoCodecInfo>(AV_CODEC_ID_HEVC,
                                                       AV_CODEC_ID_HEVC,
                                                       "H.265/HEVC",
                                                       "H265",
                                                       "",
                                                       CODEC_ENCODER_DECODER,
                                                       SystemCodecInfo::DEFAULT_VIDEO_BITRATE);
    auto sdp = std::make_unique<Sdp>(id);
    sdp->setPublishedIP("127.0.0.1");
    sdp->setLocalPublishedVideoPorts(50000, 50001);
    sdp->setLocalMediaCapabilities(MEDIA_VIDEO, {h265});
    return sdp;
}

const pjmedia_sdp_media*
SDPTest::videoMedia(const pjmedia_sdp_session* session)
{
    for (unsigned i = 0; i < session->media_count; i++)
        if (!pj_stricmp2(&session->media[i]->desc.media, "video"))
            return session->media[i];
    return nullptr;
}

std::string
SDPTest::encodingName(const pjmedia_sdp_media* media, unsigned fmtIdx)
{
    auto* m = const_cast<pjmedia_sdp_media*>(media);
    auto* attr = pjmedia_sdp_media_find_attr2(m, "rtpmap", &m->desc.fmt[fmtIdx]);
    if (!attr)
        return {};
    pjmedia_sdp_rtpmap rtpmap;
    if (pjmedia_sdp_attr_get_rtpmap(attr, &rtpmap) != PJ_SUCCESS)
        return {};
    return std::string(rtpmap.enc_name.ptr, rtpmap.enc_name.slen);
}

std::string
SDPTest::fmtpParams(const pjmedia_sdp_media* media, unsigned fmtIdx)
{
    auto* m = const_cast<pjmedia_sdp_media*>(media);
    auto* attr = pjmedia_sdp_media_find_attr2(m, "fmtp", &m->desc.fmt[fmtIdx]);
    if (!attr)
        return {};
    return std::string(attr->value.ptr, attr->value.slen);
}

static MediaAttribute
videoAttribute(const char* source)
{
    return MediaAttribute(MediaType::MEDIA_VIDEO, false, false, true, source, "video_0");
}

const pjmedia_sdp_session*
SDPTest::parseRemoteOffer(const std::string& mediaLines)
{
    if (!testPool_)
        testPool_.reset(
            pj_pool_create(&Manager::instance().sipVoIPLink().getCachingPool()->factory, "sdpTest", 4096, 4096, nullptr));
    auto body = "v=0\r\n"
                "o=- 0 0 IN IP4 127.0.0.1\r\n"
                "s=-\r\n"
                "c=IN IP4 127.0.0.1\r\n"
                "t=0 0\r\n"
                + mediaLines + "a=sendrecv\r\n";
    pjmedia_sdp_session* session = nullptr;
    auto status = pjmedia_sdp_parse(testPool_.get(), body.data(), body.size(), &session);
    CPPUNIT_ASSERT(status == PJ_SUCCESS);
    // The parsed session points into `body`; clone it before the buffer dies
    return pjmedia_sdp_session_clone(testPool_.get(), session);
}

static std::string
h264Offer(const std::string& fmtp)
{
    return "m=video 60000 RTP/AVP 96\r\n"
           "a=rtpmap:96 H264/90000\r\n"
           "a=fmtp:96 "
           + fmtp + "\r\n";
}

void
SDPTest::testCameraOfferOrdersBaselineFirst()
{
    auto sdp = createSdp("offer-camera");
    CPPUNIT_ASSERT(sdp->createOffer({videoAttribute(CAMERA_URI)}));

    auto* media = videoMedia(sdp->getLocalSdpSession());
    CPPUNIT_ASSERT(media);
    // One payload per negotiable H264 profile: baseline + 4:4:4 + 4:2:2
    CPPUNIT_ASSERT_EQUAL(3u, media->desc.fmt_count);
    for (unsigned i = 0; i < media->desc.fmt_count; i++)
        CPPUNIT_ASSERT_EQUAL(std::string("H264"), encodingName(media, i));

    // Camera: interop-safe baseline profile first
    CPPUNIT_ASSERT(fmtpParams(media, 0).find("profile-level-id=428029") != std::string::npos);
    // Capability advertisement: high chroma profiles follow, best first
    CPPUNIT_ASSERT(fmtpParams(media, 1).find("profile-level-id=f40029") != std::string::npos);
    CPPUNIT_ASSERT(fmtpParams(media, 2).find("profile-level-id=7a0029") != std::string::npos);
}

void
SDPTest::testScreenShareOfferOrdersHighProfileFirst()
{
    auto sdp = createSdp("offer-screen");
    CPPUNIT_ASSERT(sdp->createOffer({videoAttribute(SCREEN_URI)}));

    auto* media = videoMedia(sdp->getLocalSdpSession());
    CPPUNIT_ASSERT(media);
    CPPUNIT_ASSERT_EQUAL(3u, media->desc.fmt_count);

    // Screen sharing: best chroma sampling first, baseline last for interop
    CPPUNIT_ASSERT(fmtpParams(media, 0).find("profile-level-id=f40029") != std::string::npos);
    CPPUNIT_ASSERT(fmtpParams(media, 1).find("profile-level-id=7a0029") != std::string::npos);
    CPPUNIT_ASSERT(fmtpParams(media, 2).find("profile-level-id=428029") != std::string::npos);
}

void
SDPTest::testScreenShareNegotiatesHighProfile()
{
    auto offerer = createSdp("nego-screen-offerer");
    CPPUNIT_ASSERT(offerer->createOffer({videoAttribute(SCREEN_URI)}));

    auto answerer = createSdp("nego-screen-answerer");
    answerer->setReceivedOffer(offerer->getLocalSdpSession());
    CPPUNIT_ASSERT(answerer->processIncomingOffer({videoAttribute(CAMERA_URI)}));
    CPPUNIT_ASSERT(answerer->startNegotiation());

    // RFC 6184 §8.2.2: the answer must pair payload types of the same profile.
    // The offerer prefers High 4:4:4 for its screen share; the answerer
    // supports it, so the negotiated first payload must be High 4:4:4.
    auto localDescrs = answerer->getActiveMediaDescription(false);
    auto remoteDescrs = answerer->getActiveMediaDescription(true);
    CPPUNIT_ASSERT_EQUAL(size_t(1), localDescrs.size());
    CPPUNIT_ASSERT_EQUAL(size_t(1), remoteDescrs.size());
    CPPUNIT_ASSERT(localDescrs[0].enabled);
    CPPUNIT_ASSERT(localDescrs[0].parameters.find("profile-level-id=f40029") != std::string::npos);
    CPPUNIT_ASSERT(remoteDescrs[0].parameters.find("profile-level-id=f40029") != std::string::npos);
    // Same payload type on both sides for the selected profile
    CPPUNIT_ASSERT_EQUAL(remoteDescrs[0].payload_type, localDescrs[0].payload_type);
}

void
SDPTest::testCameraNegotiationKeepsBaseline()
{
    auto offerer = createSdp("nego-camera-offerer");
    CPPUNIT_ASSERT(offerer->createOffer({videoAttribute(CAMERA_URI)}));

    auto answerer = createSdp("nego-camera-answerer");
    answerer->setReceivedOffer(offerer->getLocalSdpSession());
    CPPUNIT_ASSERT(answerer->processIncomingOffer({videoAttribute(CAMERA_URI)}));
    CPPUNIT_ASSERT(answerer->startNegotiation());

    // Camera streams keep the interop-safe baseline profile
    auto localDescrs = answerer->getActiveMediaDescription(false);
    CPPUNIT_ASSERT_EQUAL(size_t(1), localDescrs.size());
    CPPUNIT_ASSERT(localDescrs[0].enabled);
    CPPUNIT_ASSERT(localDescrs[0].parameters.find("profile-level-id=428029") != std::string::npos);
}

void
SDPTest::testOfferAdvertisesLevelAsymmetry()
{
    // RFC 6184 §8.1: level asymmetry must be explicitly allowed on both
    // sides; advertise it on every H264 payload we offer.
    auto sdp = createSdp("offer-laa");
    CPPUNIT_ASSERT(sdp->createOffer({videoAttribute(CAMERA_URI)}));

    auto* media = videoMedia(sdp->getLocalSdpSession());
    CPPUNIT_ASSERT(media);
    for (unsigned i = 0; i < media->desc.fmt_count; i++)
        CPPUNIT_ASSERT(fmtpParams(media, i).find("level-asymmetry-allowed=1") != std::string::npos);
}

void
SDPTest::testAnswerClampsLevelWithoutAsymmetry()
{
    // Remote offer at level 3.1 without level-asymmetry-allowed: the level
    // is symmetric (RFC 6184 §8.1), so our answer must not exceed it.
    auto answerer = createSdp("laa-clamp");
    answerer->setReceivedOffer(parseRemoteOffer(h264Offer("profile-level-id=42801f")));
    CPPUNIT_ASSERT(answerer->processIncomingOffer({videoAttribute(CAMERA_URI)}));
    CPPUNIT_ASSERT(answerer->startNegotiation());

    auto* answerMedia = videoMedia(answerer->getActiveLocalSdpSession());
    CPPUNIT_ASSERT(answerMedia);
    auto params = fmtpParams(answerMedia, 0);
    CPPUNIT_ASSERT(params.find("profile-level-id=42801f") != std::string::npos);
    CPPUNIT_ASSERT(params.find("level-asymmetry-allowed=1") == std::string::npos);

    // The sending level follows the (lower) symmetric level
    auto remoteDescrs = answerer->getActiveMediaDescription(true);
    CPPUNIT_ASSERT_EQUAL(size_t(1), remoteDescrs.size());
    CPPUNIT_ASSERT(remoteDescrs[0].parameters.find("profile-level-id=42801f") != std::string::npos);
}

void
SDPTest::testAnswerKeepsLevelWithAsymmetry()
{
    // Remote offer at level 3.1 WITH level-asymmetry-allowed: our answer
    // advertises our own receive level, and we may send up to the
    // remote receive level (RFC 6184 §8.1).
    auto answerer = createSdp("laa-keep");
    answerer->setReceivedOffer(parseRemoteOffer(h264Offer("profile-level-id=42801f;level-asymmetry-allowed=1")));
    CPPUNIT_ASSERT(answerer->processIncomingOffer({videoAttribute(CAMERA_URI)}));
    CPPUNIT_ASSERT(answerer->startNegotiation());

    auto* answerMedia = videoMedia(answerer->getActiveLocalSdpSession());
    CPPUNIT_ASSERT(answerMedia);
    auto params = fmtpParams(answerMedia, 0);
    CPPUNIT_ASSERT(params.find("profile-level-id=428029") != std::string::npos);
    CPPUNIT_ASSERT(params.find("level-asymmetry-allowed=1") != std::string::npos);

    // Sending parameters keep the remote receive level
    auto remoteDescrs = answerer->getActiveMediaDescription(true);
    CPPUNIT_ASSERT_EQUAL(size_t(1), remoteDescrs.size());
    CPPUNIT_ASSERT(remoteDescrs[0].parameters.find("profile-level-id=42801f") != std::string::npos);
}

void
SDPTest::testSendLevelClampedWhenSymmetric()
{
    // Remote offer at level 5.1 without level-asymmetry-allowed: our answer
    // keeps our lower level 4.1 and the symmetric sending level is 4.1,
    // even though the remote advertised a higher one.
    auto answerer = createSdp("laa-send-clamp");
    answerer->setReceivedOffer(parseRemoteOffer(h264Offer("profile-level-id=428033")));
    CPPUNIT_ASSERT(answerer->processIncomingOffer({videoAttribute(CAMERA_URI)}));
    CPPUNIT_ASSERT(answerer->startNegotiation());

    auto* answerMedia = videoMedia(answerer->getActiveLocalSdpSession());
    CPPUNIT_ASSERT(answerMedia);
    CPPUNIT_ASSERT(fmtpParams(answerMedia, 0).find("profile-level-id=428029") != std::string::npos);

    // The sending level is the lower of both levels
    auto remoteDescrs = answerer->getActiveMediaDescription(true);
    CPPUNIT_ASSERT_EQUAL(size_t(1), remoteDescrs.size());
    CPPUNIT_ASSERT(remoteDescrs[0].parameters.find("profile-level-id=428029") != std::string::npos);
}

void
SDPTest::testH265OfferAdvertisesProfiles()
{
    auto sdp = createH265Sdp("h265-offer");
    CPPUNIT_ASSERT(sdp->createOffer({videoAttribute(CAMERA_URI)}));

    auto* media = videoMedia(sdp->getLocalSdpSession());
    CPPUNIT_ASSERT(media);
    // One payload per negotiable profile: Main + locally supported
    // high chroma profiles (hardware encoder dependent)
    const auto highProfiles = h265::negotiableHighProfiles();
    CPPUNIT_ASSERT_EQUAL(unsigned(1 + highProfiles.size()), media->desc.fmt_count);
    for (unsigned i = 0; i < media->desc.fmt_count; i++)
        CPPUNIT_ASSERT_EQUAL(std::string("H265"), encodingName(media, i));

    // Camera: interop-safe Main profile first, explicit parameters
    CPPUNIT_ASSERT(fmtpParams(media, 0).find("profile-id=1") != std::string::npos);
    CPPUNIT_ASSERT(fmtpParams(media, 0).find("level-id=123") != std::string::npos);
    // High chroma profiles advertised after, as Range Extensions payloads
    for (unsigned i = 1; i < media->desc.fmt_count; i++)
        CPPUNIT_ASSERT(fmtpParams(media, i).find("profile-id=4") != std::string::npos);

    // Screen share: high profiles first when available
    if (!highProfiles.empty()) {
        auto screenSdp = createH265Sdp("h265-offer-screen");
        CPPUNIT_ASSERT(screenSdp->createOffer({videoAttribute(SCREEN_URI)}));
        auto* screenMedia = videoMedia(screenSdp->getLocalSdpSession());
        CPPUNIT_ASSERT(screenMedia);
        CPPUNIT_ASSERT(fmtpParams(screenMedia, 0).find("profile-id=4") != std::string::npos);
        CPPUNIT_ASSERT(fmtpParams(screenMedia, screenMedia->desc.fmt_count - 1).find("profile-id=1")
                       != std::string::npos);
    }
}

void
SDPTest::testH265AnswerMatchesProfile()
{
    // RFC 7798 §7.2.2: profile parameters are symmetric per payload
    // type. A remote offer preferring Main 4:4:4 must only be paired
    // with a local Main 4:4:4 payload; otherwise the Main payload types
    // must be paired together.
    auto answerer = createH265Sdp("h265-match");
    answerer->setReceivedOffer(
        parseRemoteOffer("m=video 60000 RTP/AVP 96 97\r\n"
                         "a=rtpmap:96 H265/90000\r\n"
                         "a=fmtp:96 profile-id=4;level-id=123;interop-constraints=BE0800000000\r\n"
                         "a=rtpmap:97 H265/90000\r\n"
                         "a=fmtp:97 profile-id=1;level-id=123\r\n"));
    CPPUNIT_ASSERT(answerer->processIncomingOffer({videoAttribute(CAMERA_URI)}));
    CPPUNIT_ASSERT(answerer->startNegotiation());

    auto localDescrs = answerer->getActiveMediaDescription(false);
    auto remoteDescrs = answerer->getActiveMediaDescription(true);
    CPPUNIT_ASSERT_EQUAL(size_t(1), localDescrs.size());
    CPPUNIT_ASSERT(localDescrs[0].enabled);
    const bool canDo444 = h265::canEncode(h265::Profile::Main444);
    if (canDo444) {
        // Offerer prefers 4:4:4 and we support it: both sides use it
        CPPUNIT_ASSERT(localDescrs[0].parameters.find("interop-constraints=BE0800000000") != std::string::npos);
        CPPUNIT_ASSERT_EQUAL(96u, remoteDescrs[0].payload_type);
    } else {
        // We only support Main: the 4:4:4 payload must be skipped
        CPPUNIT_ASSERT(localDescrs[0].parameters.find("profile-id=1") != std::string::npos);
        CPPUNIT_ASSERT_EQUAL(97u, remoteDescrs[0].payload_type);
    }
}

void
SDPTest::testH265AnswerClampsLevel()
{
    // RFC 7798 §7.2.2: the answer level-id must not indicate a higher
    // level than the offer.
    auto answerer = createH265Sdp("h265-level");
    answerer->setReceivedOffer(parseRemoteOffer("m=video 60000 RTP/AVP 96\r\n"
                                                "a=rtpmap:96 H265/90000\r\n"
                                                "a=fmtp:96 profile-id=1;level-id=90\r\n"));
    CPPUNIT_ASSERT(answerer->processIncomingOffer({videoAttribute(CAMERA_URI)}));
    CPPUNIT_ASSERT(answerer->startNegotiation());

    auto* answerMedia = videoMedia(answerer->getActiveLocalSdpSession());
    CPPUNIT_ASSERT(answerMedia);
    CPPUNIT_ASSERT(fmtpParams(answerMedia, 0).find("level-id=90") != std::string::npos);

    // The sending level follows the (lower) negotiated level
    auto remoteDescrs = answerer->getActiveMediaDescription(true);
    CPPUNIT_ASSERT_EQUAL(size_t(1), remoteDescrs.size());
    CPPUNIT_ASSERT(remoteDescrs[0].parameters.find("level-id=90") != std::string::npos);

    // A remote offer with a higher level keeps our own level in the answer,
    // and our sending level must not exceed our advertised level
    auto answerer2 = createH265Sdp("h265-level-high");
    answerer2->setReceivedOffer(parseRemoteOffer("m=video 60000 RTP/AVP 96\r\n"
                                                 "a=rtpmap:96 H265/90000\r\n"
                                                 "a=fmtp:96 profile-id=1;level-id=180\r\n"));
    CPPUNIT_ASSERT(answerer2->processIncomingOffer({videoAttribute(CAMERA_URI)}));
    CPPUNIT_ASSERT(answerer2->startNegotiation());
    auto* answerMedia2 = videoMedia(answerer2->getActiveLocalSdpSession());
    CPPUNIT_ASSERT(answerMedia2);
    CPPUNIT_ASSERT(fmtpParams(answerMedia2, 0).find("level-id=123") != std::string::npos);
    auto remoteDescrs2 = answerer2->getActiveMediaDescription(true);
    CPPUNIT_ASSERT(remoteDescrs2[0].parameters.find("level-id=123") != std::string::npos);
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::SDPTest::name());
