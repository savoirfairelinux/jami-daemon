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
#include "sip/sdp.h"
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

    CPPUNIT_TEST_SUITE(SDPTest);
    CPPUNIT_TEST(testCameraOfferOrdersBaselineFirst);
    CPPUNIT_TEST(testScreenShareOfferOrdersHighProfileFirst);
    CPPUNIT_TEST(testScreenShareNegotiatesHighProfile);
    CPPUNIT_TEST(testCameraNegotiationKeepsBaseline);
    CPPUNIT_TEST_SUITE_END();

    std::unique_ptr<Sdp> createSdp(const std::string& id);
    static const pjmedia_sdp_media* videoMedia(const pjmedia_sdp_session* session);
    static std::string encodingName(const pjmedia_sdp_media* media, unsigned fmtIdx);
    static std::string fmtpParams(const pjmedia_sdp_media* media, unsigned fmtIdx);

    std::vector<std::shared_ptr<SystemCodecInfo>> videoCodecs_;
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

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::SDPTest::name());
