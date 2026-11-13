#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <array>
#include "jami.h"
#include "manager.h"
#include "connectivity/sip_utils.h"
#include "sip/sdp.h"
#include "sip/sipaccount.h"
#include "sip/sipcall.h"
#include "sip/sipvoiplink.h"
#include "account_const.h"
#include "jami/media_const.h"
#include "common.h"
#include "test_runner.h"

extern "C" {
#include <pjmedia/sdp.h>
#include <pjmedia/sdp_neg.h>
}

using namespace libjami::Account;

namespace jami {
namespace test {

namespace {

constexpr std::string_view MID_RTP_EXTENSION_URI {"urn:ietf:params:rtp-hdrext:sdes:mid"};
constexpr std::string_view TRANSPORT_CC_RTP_EXTENSION_URI {
    "http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01"};

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
    void peerVersionControlsLegacyRemb();
    void offerAdvertisesRtcpMuxByDefault();
    void offerSkipsRtcpMuxWhenDisabled();
    void offerAdvertisesRtcpMuxWhenEnabled();
    void offerAdvertisesBundleAndMid();
    void reofferPreservesStreamIdentityAndSlots();
    void reofferPreservesImportedMidsAndSkippedSlots();
    void reofferRejectsAmbiguousStreamIdentity();
    void mediaLookupRejectsAmbiguousIdentity();
    void mediaChangeRemovesStreamsByIdentity();
    void configureBundleSdp(Sdp& sdp);
    void offerAdvertisesTransportCcAndRemb();
    void reofferAdvertisesFeedbackForNewMedia();
    void offerAdvertisesTransportCcOnlyForActiveDirections();
    void offerAdvertisesPliAndFirForVideoOnly();
    void offerAdvertisesH264PacketizationMode1();
    void answerUsesOfferedH264PayloadForFmtp();
    void answerNegotiatesPliAndFir();
    void answerSkipsPliAndFirWhenOfferDoesNot();
    void answerPreservesRemoteBundleMidAndExtmap();
    void answerNegotiatesTransportCcAndRemb();
    void answerKeepsRembFallbackWithoutTransportCcExtmap();
    void answerNormalizesInconsistentBundleMidExtmap();
    void answerAcceptsSessionLevelBundleMidExtmap();
    void answerSkipsBundleWhenOfferDoesNotAdvertiseIt();
    void answerSkipsRtcpMuxWhenOfferDoesNotAdvertiseIt();
    void answerAdvertisesRtcpMuxWhenOfferDoes();
    void rejectsConflictingRtcpMuxPayloadTypes();
    void acceptsSafeAndNonMuxPayloadTypes();
    void rejectedMediaDoesNotRequireMuxSafePayloads();
    void negotiatedMediaFallsBackToSeparateRtcp();
    void negotiatedMediaPreservesExplicitRtcpFallback();
    void negotiatedMediaKeepsAcceptedRtcpMux();
    void checkNegotiatedRtcp(bool muxAccepted, bool explicitRtcp);
    void remoteSdpFallsBackToRtcpNextPort();
    void remoteSdpKeepsMuxOnRtpPort();
    void remoteSdpParsesTransportCcFeedback();
    void parsesBrowserOfferWithoutSipSession();
    void publishedIpAcceptsMediaLevelConnections();
    void publishedIpUpdatesSessionConnection();
    void negotiatesDynamicResolutionViaImageAttrAndWebRtc();

    CPPUNIT_TEST_SUITE(RtcpMuxSdpTest);
    CPPUNIT_TEST(peerVersionControlsLegacyRemb);
    CPPUNIT_TEST(offerAdvertisesRtcpMuxByDefault);
    CPPUNIT_TEST(offerSkipsRtcpMuxWhenDisabled);
    CPPUNIT_TEST(offerAdvertisesRtcpMuxWhenEnabled);
    CPPUNIT_TEST(offerAdvertisesBundleAndMid);
    CPPUNIT_TEST(reofferPreservesStreamIdentityAndSlots);
    CPPUNIT_TEST(reofferPreservesImportedMidsAndSkippedSlots);
    CPPUNIT_TEST(reofferRejectsAmbiguousStreamIdentity);
    CPPUNIT_TEST(mediaLookupRejectsAmbiguousIdentity);
    CPPUNIT_TEST(mediaChangeRemovesStreamsByIdentity);
    CPPUNIT_TEST(offerAdvertisesTransportCcAndRemb);
    CPPUNIT_TEST(reofferAdvertisesFeedbackForNewMedia);
    CPPUNIT_TEST(offerAdvertisesTransportCcOnlyForActiveDirections);
    CPPUNIT_TEST(offerAdvertisesPliAndFirForVideoOnly);
    CPPUNIT_TEST(offerAdvertisesH264PacketizationMode1);
    CPPUNIT_TEST(answerUsesOfferedH264PayloadForFmtp);
    CPPUNIT_TEST(answerNegotiatesPliAndFir);
    CPPUNIT_TEST(answerSkipsPliAndFirWhenOfferDoesNot);
    CPPUNIT_TEST(answerPreservesRemoteBundleMidAndExtmap);
    CPPUNIT_TEST(answerNegotiatesTransportCcAndRemb);
    CPPUNIT_TEST(answerKeepsRembFallbackWithoutTransportCcExtmap);
    CPPUNIT_TEST(answerNormalizesInconsistentBundleMidExtmap);
    CPPUNIT_TEST(answerAcceptsSessionLevelBundleMidExtmap);
    CPPUNIT_TEST(answerSkipsBundleWhenOfferDoesNotAdvertiseIt);
    CPPUNIT_TEST(answerSkipsRtcpMuxWhenOfferDoesNotAdvertiseIt);
    CPPUNIT_TEST(answerAdvertisesRtcpMuxWhenOfferDoes);
    CPPUNIT_TEST(rejectsConflictingRtcpMuxPayloadTypes);
    CPPUNIT_TEST(acceptsSafeAndNonMuxPayloadTypes);
    CPPUNIT_TEST(rejectedMediaDoesNotRequireMuxSafePayloads);
    CPPUNIT_TEST(negotiatedMediaFallsBackToSeparateRtcp);
    CPPUNIT_TEST(negotiatedMediaPreservesExplicitRtcpFallback);
    CPPUNIT_TEST(negotiatedMediaKeepsAcceptedRtcpMux);
    CPPUNIT_TEST(remoteSdpFallsBackToRtcpNextPort);
    CPPUNIT_TEST(remoteSdpKeepsMuxOnRtpPort);
    CPPUNIT_TEST(remoteSdpParsesTransportCcFeedback);
    CPPUNIT_TEST(parsesBrowserOfferWithoutSipSession);
    CPPUNIT_TEST(publishedIpAcceptsMediaLevelConnections);
    CPPUNIT_TEST(publishedIpUpdatesSessionConnection);
    CPPUNIT_TEST(negotiatesDynamicResolutionViaImageAttrAndWebRtc);
    CPPUNIT_TEST_SUITE_END();

    std::string accountId_ {};
    std::shared_ptr<SIPAccount> account_ {};
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(RtcpMuxSdpTest, RtcpMuxSdpTest::name());

void
RtcpMuxSdpTest::peerVersionControlsLegacyRemb()
{
    SIPCall call(account_, "legacy-remb-version-test", Call::CallType::OUTGOING, {});
    CPPUNIT_ASSERT(!call.peerUsesLegacyRemb_);
    for (const auto ua : {"Jami Daemon 16.0.0", "Jami Daemon 16.9.9-test Linux", "Jami Daemon 16.0.0.1 Linux"}) {
        call.setPeerUaVersion(ua);
        CPPUNIT_ASSERT(call.peerUsesLegacyRemb_);
    }
    for (const auto& ua : {account_->getUserAgentName(),
                           std::string("Jami Daemon 17.0.0"),
                           std::string("Jami Daemon 17.0.0-test Linux"),
                           std::string("Jami Daemon 18.0.0 Linux"),
                           std::string("Other SIP endpoint 16.0.0"),
                           std::string("Jami Daemon invalid Linux"),
                           std::string("Jami Daemon 17 Linux"),
                           std::string("Jami Daemon 16.bad.0 Linux"),
                           std::string("Jami Daemon 16..0 Linux")}) {
        call.setPeerUaVersion("Jami Daemon 16.0.0");
        CPPUNIT_ASSERT(call.peerUsesLegacyRemb_);
        call.setPeerUaVersion(ua);
        CPPUNIT_ASSERT(!call.peerUsesLegacyRemb_);
    }
}

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
RtcpMuxSdpTest::configureBundleSdp(Sdp& sdp)
{
    sdp.setPublishedIP("127.0.0.1", pj_AF_INET());
    sdp.setLocalMediaCapabilities(MEDIA_AUDIO, account_->getActiveAccountCodecInfoList(MEDIA_AUDIO));
    sdp.setLocalMediaCapabilities(MEDIA_VIDEO, account_->getActiveAccountCodecInfoList(MEDIA_VIDEO));
    sdp.setLocalPublishedAudioPorts(TEST_AUDIO_RTP_PORT, 0);
    sdp.setLocalPublishedVideoPorts(TEST_AUDIO_RTP_PORT, 0);
    sdp.enableRtcpMux(true);
    sdp.enableBundle(true);
}

void
RtcpMuxSdpTest::reofferPreservesStreamIdentityAndSlots()
{
    Sdp sdp("bundle-reoffer-slots");
    configureBundleSdp(sdp);
    MediaAttribute audio(MEDIA_AUDIO, false, false, true, {}, "mic");
    MediaAttribute camera(MEDIA_VIDEO, false, false, true, {}, "camera");
    MediaAttribute screen(MEDIA_VIDEO, false, false, true, {}, "screen");
    CPPUNIT_ASSERT(sdp.createOffer({audio, camera, screen}));
    auto* previous = sdp.getLocalSdpSession();

    const auto saved = sdp.saveLocalOfferState();
    CPPUNIT_ASSERT(sdp.createOfferFromExternalSdp(printSdp(previous)));
    sdp.restoreLocalOfferState(saved);

    camera.hold_ = true;
    CPPUNIT_ASSERT(sdp.createOffer({screen, audio, camera}, previous));
    auto* reordered = sdp.getLocalSdpSession();
    CPPUNIT_ASSERT_EQUAL(3u, reordered->media_count);
    CPPUNIT_ASSERT(hasMediaAttributeValue(reordered->media[0], "mid", "mic"));
    CPPUNIT_ASSERT(hasMediaAttributeValue(reordered->media[1], "mid", "camera"));
    CPPUNIT_ASSERT(hasMediaAttributeValue(reordered->media[2], "mid", "screen"));
    CPPUNIT_ASSERT(hasMediaAttributeName(reordered->media[1], "sendonly"));
    CPPUNIT_ASSERT(hasMediaAttributeName(reordered->media[2], "sendrecv"));
    CPPUNIT_ASSERT_EQUAL(previous->origin.id, reordered->origin.id);
    CPPUNIT_ASSERT_EQUAL(previous->origin.version + 1, reordered->origin.version);

    CPPUNIT_ASSERT(sdp.createOffer({screen, audio}, reordered));
    auto* removed = sdp.getLocalSdpSession();
    CPPUNIT_ASSERT_EQUAL(3u, removed->media_count);
    CPPUNIT_ASSERT_EQUAL(0u, unsigned(removed->media[1]->desc.port));
    CPPUNIT_ASSERT(hasMediaAttributeValue(removed->media[1], "mid", "camera"));
    CPPUNIT_ASSERT(hasMediaAttributeValue(removed->media[2], "mid", "screen"));
    CPPUNIT_ASSERT(hasSessionAttribute(removed, "group", "BUNDLE mic screen"));
    CPPUNIT_ASSERT_EQUAL(2u, sdp.getMediaIndex("screen").value());

    audio.enabled_ = false;
    MediaAttribute extra(MEDIA_VIDEO, false, false, true, {}, "extra");
    CPPUNIT_ASSERT(sdp.createOffer({extra, screen, audio}, removed));
    auto* appended = sdp.getLocalSdpSession();
    CPPUNIT_ASSERT_EQUAL(4u, appended->media_count);
    CPPUNIT_ASSERT_EQUAL(0u, unsigned(appended->media[0]->desc.port));
    CPPUNIT_ASSERT_EQUAL(0u, unsigned(appended->media[1]->desc.port));
    CPPUNIT_ASSERT(hasMediaAttributeValue(appended->media[0], "mid", "mic"));
    CPPUNIT_ASSERT(hasMediaAttributeValue(appended->media[2], "mid", "screen"));
    CPPUNIT_ASSERT(hasMediaAttributeValue(appended->media[3], "mid", "extra"));
    CPPUNIT_ASSERT(hasSessionAttribute(appended, "group", "BUNDLE screen extra"));
    CPPUNIT_ASSERT(pjmedia_sdp_validate(appended) == PJ_SUCCESS);
}

void
RtcpMuxSdpTest::reofferPreservesImportedMidsAndSkippedSlots()
{
    Sdp sdp("bundle-reoffer-imported-mids");
    configureBundleSdp(sdp);
    const std::string remoteOffer = "v=0\r\n"
                                    "o=- 123 7 IN IP4 127.0.0.1\r\n"
                                    "s=-\r\n"
                                    "c=IN IP4 127.0.0.1\r\n"
                                    "t=0 0\r\n"
                                    "a=group:BUNDLE audio_2 track\r\n"
                                    "m=audio 5004 RTP/AVP 0\r\n"
                                    "a=mid:audio_2\r\n"
                                    "a=rtpmap:0 PCMU/8000\r\n"
                                    "a=rtcp-mux\r\n"
                                    "m=audio 0 RTP/AVP 8\r\n"
                                    "a=mid:stopped\r\n"
                                    "a=rtpmap:8 PCMA/8000\r\n"
                                    "m=video 5004 RTP/AVP 120\r\n"
                                    "a=mid:track\r\n"
                                    "a=rtpmap:120 VP8/90000\r\n"
                                    "a=rtcp-mux\r\n";
    CPPUNIT_ASSERT(sdp.setReceivedOfferFromExternalSdp(remoteOffer));
    MediaAttribute audio(MEDIA_AUDIO, false, false, true, {}, "audio_0");
    MediaAttribute stopped(MEDIA_AUDIO, false, false, false, {}, "audio_1");
    MediaAttribute video(MEDIA_VIDEO, false, false, true, {}, "video_0");
    CPPUNIT_ASSERT(sdp.processIncomingOffer({audio, stopped, video}));
    CPPUNIT_ASSERT(sdp.startNegotiation());
    const auto* previous = sdp.getActiveLocalSdpSession();
    CPPUNIT_ASSERT(previous);

    sdp.clearIce();
    MediaAttribute extra(MEDIA_AUDIO, false, false, true, {}, "audio_2");
    CPPUNIT_ASSERT(sdp.createOffer({video, extra, audio}, previous));
    auto* reoffer = sdp.getLocalSdpSession();
    CPPUNIT_ASSERT_EQUAL(4u, reoffer->media_count);
    CPPUNIT_ASSERT(hasMediaAttributeValue(reoffer->media[0], "mid", "audio_2"));
    CPPUNIT_ASSERT_EQUAL(0u, unsigned(reoffer->media[1]->desc.port));
    CPPUNIT_ASSERT(hasMediaAttributeValue(reoffer->media[1], "mid", "stopped"));
    CPPUNIT_ASSERT(hasMediaAttributeValue(reoffer->media[2], "mid", "track"));
    CPPUNIT_ASSERT(hasMediaAttributeValue(reoffer->media[2], "rtpmap", "120 VP8/90000"));
    CPPUNIT_ASSERT(!hasMediaAttributeValue(reoffer->media[3], "mid", "audio_2"));
    CPPUNIT_ASSERT(hasMediaAttributeName(reoffer->media[3], "mid"));
    CPPUNIT_ASSERT_EQUAL(0u, sdp.getMediaIndex("audio_0").value());
    CPPUNIT_ASSERT_EQUAL(2u, sdp.getMediaIndex("video_0").value());
    CPPUNIT_ASSERT_EQUAL(3u, sdp.getMediaIndex("audio_2").value());
    CPPUNIT_ASSERT(pjmedia_sdp_validate(reoffer) == PJ_SUCCESS);
}

void
RtcpMuxSdpTest::reofferRejectsAmbiguousStreamIdentity()
{
    Sdp sdp("bundle-reoffer-invalid-identity");
    configureBundleSdp(sdp);
    MediaAttribute audio(MEDIA_AUDIO, false, false, true, {}, "audio_0");
    CPPUNIT_ASSERT(sdp.createOffer({audio}));
    auto* previous = sdp.getLocalSdpSession();
    CPPUNIT_ASSERT(!sdp.createOffer({audio, audio}, previous));
    auto invalid = audio;
    invalid.type_ = MEDIA_VIDEO;
    CPPUNIT_ASSERT(!sdp.createOffer({invalid}, previous));
    invalid = audio;
    invalid.label_.clear();
    CPPUNIT_ASSERT(!sdp.createOffer({invalid}, previous));
    CPPUNIT_ASSERT(previous == sdp.getLocalSdpSession());
}

void
RtcpMuxSdpTest::mediaLookupRejectsAmbiguousIdentity()
{
    MediaAttribute audio(MediaType::MEDIA_AUDIO);
    audio.enabled_ = true;
    auto secondAudio = audio;
    secondAudio.label_ = "audio_1";

    Sdp unique("unique-media-label");
    configureBundleSdp(unique);
    CPPUNIT_ASSERT(unique.createOffer({audio, secondAudio}));
    CPPUNIT_ASSERT_EQUAL(0u, unique.getMediaIndex("").value());
    CPPUNIT_ASSERT_EQUAL(1u, unique.getMediaIndex("audio_1").value());
    CPPUNIT_ASSERT(!unique.getMediaIndex("missing"));

    Sdp ambiguous("ambiguous-media-label");
    configureBundleSdp(ambiguous);
    CPPUNIT_ASSERT(ambiguous.createOffer({audio, audio}));
    CPPUNIT_ASSERT(!ambiguous.getMediaIndex(""));
}

void
RtcpMuxSdpTest::mediaChangeRemovesStreamsByIdentity()
{
    MediaAttribute first(MEDIA_AUDIO, false, false, true, {}, "audio_0");
    MediaAttribute middle(MEDIA_AUDIO, false, false, true, {}, "audio_1");
    MediaAttribute last(MEDIA_AUDIO, false, false, true, {}, "audio_2");
    auto call = std::make_shared<SIPCall>(account_,
                                          "media-stream-identity",
                                          Call::CallType::OUTGOING,
                                          MediaAttribute::mediaAttributesToMediaMaps({first, middle, last}));
    call->setPeerUaVersion("Jami Daemon 99.0.0 (test)");
    const auto firstSession = call->getRtpStreams()[0].rtpSession_;
    const auto lastSession = call->getRtpStreams()[2].rtpSession_;
    CPPUNIT_ASSERT(call->requestMediaChange(MediaAttribute::mediaAttributesToMediaMaps({last, first})));
    const auto& streams = call->getRtpStreams();
    CPPUNIT_ASSERT_EQUAL(size_t(2), streams.size());
    CPPUNIT_ASSERT(streams[0].rtpSession_ == firstSession);
    CPPUNIT_ASSERT(streams[1].rtpSession_ == lastSession);
    CPPUNIT_ASSERT(!call->requestMediaChange(MediaAttribute::mediaAttributesToMediaMaps({first, first})));
    CPPUNIT_ASSERT_EQUAL(size_t(2), streams.size());
    last.enabled_ = false;
    CPPUNIT_ASSERT(call->requestMediaChange(MediaAttribute::mediaAttributesToMediaMaps({last, first})));
    CPPUNIT_ASSERT(!streams[1].mediaAttribute_->enabled_);
}

void
RtcpMuxSdpTest::offerAdvertisesTransportCcAndRemb()
{
    CPPUNIT_ASSERT(account_);

    Sdp sdp("transport-cc-offer");
    sdp.setPublishedIP("127.0.0.1", pj_AF_INET());
    sdp.setLocalMediaCapabilities(MediaType::MEDIA_AUDIO, account_->getActiveAccountCodecInfoList(MEDIA_AUDIO));
    sdp.setLocalPublishedAudioPorts(TEST_AUDIO_RTP_PORT, 0);

    MediaAttribute audio(MediaType::MEDIA_AUDIO);
    audio.label_ = "audio_0";
    audio.enabled_ = true;

    CPPUNIT_ASSERT(sdp.createOffer({audio}));
    auto* localSession = sdp.getLocalSdpSession();
    CPPUNIT_ASSERT(localSession);
    CPPUNIT_ASSERT_EQUAL(1u, localSession->media_count);
    CPPUNIT_ASSERT(hasMediaAttributeValue(localSession->media[0],
                                          "extmap",
                                          std::string("3 ") + std::string(TRANSPORT_CC_RTP_EXTENSION_URI)));
    CPPUNIT_ASSERT(hasMediaAttributeValue(localSession->media[0], "rtcp-fb", "* transport-cc"));
    CPPUNIT_ASSERT(hasMediaAttributeValue(localSession->media[0], "rtcp-fb", "* goog-remb"));

    const auto descriptions = sdp.getMediaDescriptions(localSession, false);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), descriptions.size());
    CPPUNIT_ASSERT_EQUAL(3u, descriptions[0].transport_cc_rtp_ext_id);
    CPPUNIT_ASSERT(descriptions[0].rtcp_fb_transport_cc);
    CPPUNIT_ASSERT(descriptions[0].rtcp_fb_goog_remb);
}

void
RtcpMuxSdpTest::reofferAdvertisesFeedbackForNewMedia()
{
    for (const auto bundle : {false, true}) {
        for (const auto previousFeedback : {false, true}) {
            Sdp sdp("new-media-feedback-reoffer");
            sdp.setPublishedIP("127.0.0.1", pj_AF_INET());
            sdp.setLocalMediaCapabilities(MEDIA_AUDIO, account_->getActiveAccountCodecInfoList(MEDIA_AUDIO));
            sdp.setLocalMediaCapabilities(MEDIA_VIDEO, account_->getActiveAccountCodecInfoList(MEDIA_VIDEO));
            sdp.setLocalPublishedAudioPorts(TEST_AUDIO_RTP_PORT, 0);
            sdp.setLocalPublishedVideoPorts(bundle ? TEST_AUDIO_RTP_PORT : TEST_AUDIO_RTP_PORT + 2, 0);
            sdp.enableRtcpMux(true);
            sdp.enableBundle(bundle);

            MediaAttribute audio(MEDIA_AUDIO, false, false, true, {}, "audio_0");
            MediaAttribute video(MEDIA_VIDEO, false, false, true, {}, "video_0");
            CPPUNIT_ASSERT(sdp.createOffer({audio}));
            auto* previous = sdp.getLocalSdpSession();
            CPPUNIT_ASSERT(previous);
            CPPUNIT_ASSERT_EQUAL(1u, previous->media_count);
            if (not previousFeedback) {
                auto* media = previous->media[0];
                pjmedia_sdp_attr_remove_all(&media->attr_count, media->attr, "rtcp-fb");
            }

            CPPUNIT_ASSERT(sdp.createOffer({audio, video}, previous));
            const auto* reoffer = sdp.getLocalSdpSession();
            CPPUNIT_ASSERT(reoffer);
            CPPUNIT_ASSERT_EQUAL(2u, reoffer->media_count);
            CPPUNIT_ASSERT_EQUAL(previousFeedback,
                                 hasMediaAttributeValue(reoffer->media[0], "rtcp-fb", "* transport-cc"));
            CPPUNIT_ASSERT_EQUAL(previousFeedback,
                                 hasMediaAttributeValue(reoffer->media[0], "rtcp-fb", "* goog-remb"));

            const auto* newMedia = reoffer->media[1];
            CPPUNIT_ASSERT(hasMediaAttributeValue(newMedia,
                                                  "extmap",
                                                  std::string("3 ") + std::string(TRANSPORT_CC_RTP_EXTENSION_URI)));
            CPPUNIT_ASSERT(hasMediaAttributeValue(newMedia, "rtcp-fb", "* transport-cc"));
            CPPUNIT_ASSERT(hasMediaAttributeValue(newMedia, "rtcp-fb", "* goog-remb"));
            CPPUNIT_ASSERT(hasMediaAttributeValue(newMedia, "rtcp-fb", "* nack"));
            CPPUNIT_ASSERT(hasMediaAttributeValue(newMedia, "rtcp-fb", "* nack pli"));
            CPPUNIT_ASSERT(hasMediaAttributeValue(newMedia, "rtcp-fb", "* ccm fir"));
        }
    }
}

void
RtcpMuxSdpTest::offerAdvertisesTransportCcOnlyForActiveDirections()
{
    CPPUNIT_ASSERT(account_);

    struct DirectionCase
    {
        const char* name;
        bool muted;
        bool hold;
        const char* direction;
        bool expectTransportCc;
    };
    const std::array<DirectionCase, 4> cases {{
        {"sendrecv", false, false, "sendrecv", true},
        {"recvonly", true, false, "recvonly", true},
        {"sendonly", false, true, "sendonly", true},
        {"inactive", true, true, "inactive", false},
    }};

    for (const auto& directionCase : cases) {
        Sdp sdp(std::string("transport-cc-direction-") + directionCase.name);
        sdp.setPublishedIP("127.0.0.1", pj_AF_INET());
        sdp.setLocalMediaCapabilities(MediaType::MEDIA_VIDEO, account_->getActiveAccountCodecInfoList(MEDIA_VIDEO));
        sdp.setLocalPublishedVideoPorts(TEST_AUDIO_RTP_PORT, 0);

        MediaAttribute video(MediaType::MEDIA_VIDEO);
        video.label_ = "video_0";
        video.enabled_ = true;
        video.muted_ = directionCase.muted;
        video.hold_ = directionCase.hold;

        CPPUNIT_ASSERT(sdp.createOffer({video}));
        auto* localSession = sdp.getLocalSdpSession();
        CPPUNIT_ASSERT(localSession);
        CPPUNIT_ASSERT_EQUAL(1u, localSession->media_count);
        auto* media = localSession->media[0];
        CPPUNIT_ASSERT(hasMediaAttributeName(media, directionCase.direction));

        CPPUNIT_ASSERT_EQUAL(directionCase.expectTransportCc,
                             hasMediaAttributeValue(media,
                                                    "extmap",
                                                    std::string("3 ") + std::string(TRANSPORT_CC_RTP_EXTENSION_URI)));
        CPPUNIT_ASSERT_EQUAL(directionCase.expectTransportCc,
                             hasMediaAttributeValue(media, "rtcp-fb", "* transport-cc"));
    }
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
    CPPUNIT_ASSERT(!hasMediaAttributeValue(localSession->media[0], "rtcp-fb", "* nack"));
    CPPUNIT_ASSERT(!hasMediaAttributeValue(localSession->media[0], "rtcp-fb", "* ccm fir"));
    CPPUNIT_ASSERT(hasMediaAttributeValue(localSession->media[1], "rtcp-fb", "* nack"));
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
RtcpMuxSdpTest::answerUsesOfferedH264PayloadForFmtp()
{
    CPPUNIT_ASSERT(account_);

    Sdp sdp("h264-answer-payload");
    sdp.setPublishedIP("127.0.0.1", pj_AF_INET());
    sdp.setLocalMediaCapabilities(MediaType::MEDIA_VIDEO, account_->getActiveAccountCodecInfoList(MEDIA_VIDEO));
    sdp.setLocalPublishedVideoPorts(TEST_AUDIO_RTP_PORT, 0);

    auto pool = makePool("h264-answer-payload");
    const std::string remoteOffer = "v=0\r\n"
                                    "o=- 0 0 IN IP4 127.0.0.1\r\n"
                                    "s=-\r\n"
                                    "c=IN IP4 127.0.0.1\r\n"
                                    "t=0 0\r\n"
                                    "m=video 5004 UDP/TLS/RTP/SAVPF 103\r\n"
                                    "a=rtpmap:103 H264/90000\r\n"
                                    "a=fmtp:103 profile-level-id=428029;packetization-mode=1\r\n";

    auto* session = parseSdp(pool.get(), remoteOffer);
    CPPUNIT_ASSERT(session);
    sdp.setReceivedOffer(session);

    MediaAttribute video(MediaType::MEDIA_VIDEO);
    video.label_ = "video_0";
    video.enabled_ = true;

    CPPUNIT_ASSERT(sdp.processIncomingOffer({video}));
    CPPUNIT_ASSERT(sdp.startNegotiation());

    auto* activeLocalSession = sdp.getActiveLocalSdpSession();
    CPPUNIT_ASSERT(activeLocalSession);
    CPPUNIT_ASSERT_EQUAL(1u, activeLocalSession->media_count);

    const auto answer = printSdp(activeLocalSession);
    CPPUNIT_ASSERT_MESSAGE(answer, answer.find("m=video 4000 UDP/TLS/RTP/SAVPF 103\r\n") != std::string::npos);
    CPPUNIT_ASSERT_MESSAGE(answer, answer.find("a=rtpmap:103 H264/90000\r\n") != std::string::npos);
    CPPUNIT_ASSERT_MESSAGE(answer,
                           answer.find("a=fmtp:103 profile-level-id=428029;packetization-mode=1\r\n")
                               != std::string::npos);
    CPPUNIT_ASSERT_MESSAGE(answer, answer.find("a=fmtp:96 ") == std::string::npos);
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
    CPPUNIT_ASSERT(hasMediaAttributeValue(localSession->media[0], "rtcp-fb", "* nack"));
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
    CPPUNIT_ASSERT(!hasMediaAttributeValue(localSession->media[0], "rtcp-fb", "* nack"));
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
RtcpMuxSdpTest::answerNegotiatesTransportCcAndRemb()
{
    CPPUNIT_ASSERT(account_);

    for (const auto uri : {
             TRANSPORT_CC_RTP_EXTENSION_URI,
             std::string_view {"http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-02"},
             std::string_view {"http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-03"},
         }) {
        for (const auto bundle : {false, true}) {
            for (const auto sessionLevel : {false, true}) {
                Sdp sdp("transport-cc-answer");
                sdp.setPublishedIP("127.0.0.1", pj_AF_INET());
                sdp.setLocalMediaCapabilities(MediaType::MEDIA_AUDIO,
                                              account_->getActiveAccountCodecInfoList(MEDIA_AUDIO));
                sdp.setLocalPublishedAudioPorts(TEST_AUDIO_RTP_PORT, 0);
                sdp.enableRtcpMux(true);
                sdp.enableBundle(bundle);

                const std::array mediaUris {uri, sessionLevel ? uri : TRANSPORT_CC_RTP_EXTENSION_URI};
                const std::array extmapIds {7u, mediaUris[0] == mediaUris[1] ? 7u : 8u};
                const auto extmap = std::string("a=extmap:7 ") + std::string(uri) + "\r\n";
                std::string remoteOffer = "v=0\r\n"
                                          "o=- 0 0 IN IP4 127.0.0.1\r\n"
                                          "s=-\r\n"
                                          "c=IN IP4 127.0.0.1\r\n"
                                          "t=0 0\r\n";
                if (bundle)
                    remoteOffer += "a=group:BUNDLE audio_0 audio_1\r\n";
                if (sessionLevel)
                    remoteOffer += extmap;
                std::vector<MediaAttribute> mediaList;
                for (unsigned i = 0; i < (bundle ? 2u : 1u); ++i) {
                    MediaAttribute audio(MediaType::MEDIA_AUDIO);
                    audio.label_ = "audio_" + std::to_string(i);
                    audio.enabled_ = true;
                    remoteOffer += "m=audio 5004 RTP/AVPF 0\r\n"
                                   "a=rtpmap:0 PCMU/8000\r\n"
                                   "a=rtcp-mux\r\n"
                                   "a=mid:" + audio.label_ + "\r\n";
                    if (not sessionLevel)
                        remoteOffer += "a=extmap:" + std::to_string(extmapIds[i]) + " "
                                       + std::string(mediaUris[i]) + "\r\n";
                    remoteOffer += "a=rtcp-fb:0 transport-cc\r\n"
                                   "a=rtcp-fb:0 goog-remb\r\n";
                    mediaList.push_back(audio);
                }

                auto pool = makePool("transport-cc-answer");
                auto* session = parseSdp(pool.get(), remoteOffer);
                CPPUNIT_ASSERT(session);
                sdp.setReceivedOffer(session);
                CPPUNIT_ASSERT(sdp.processIncomingOffer(mediaList));
                auto* localSession = sdp.getLocalSdpSession();
                CPPUNIT_ASSERT(localSession);
                CPPUNIT_ASSERT_EQUAL(mediaList.size(), static_cast<size_t>(localSession->media_count));
                if (bundle)
                    CPPUNIT_ASSERT(hasSessionAttribute(localSession, "group", "BUNDLE audio_0 audio_1"));
                const auto descriptions = sdp.getMediaDescriptions(localSession, false);
                CPPUNIT_ASSERT_EQUAL(mediaList.size(), descriptions.size());
                for (unsigned i = 0; i < localSession->media_count; ++i) {
                    CPPUNIT_ASSERT(hasMediaAttributeValue(localSession->media[i],
                                                          "extmap",
                                                          std::to_string(extmapIds[i]) + " "
                                                              + std::string(mediaUris[i])));
                    CPPUNIT_ASSERT(hasMediaAttributeValue(localSession->media[i], "rtcp-fb", "* transport-cc"));
                    CPPUNIT_ASSERT(hasMediaAttributeValue(localSession->media[i], "rtcp-fb", "* goog-remb"));
                    CPPUNIT_ASSERT_EQUAL(extmapIds[i], descriptions[i].transport_cc_rtp_ext_id);
                    CPPUNIT_ASSERT(descriptions[i].rtcp_fb_transport_cc);
                    CPPUNIT_ASSERT(descriptions[i].rtcp_fb_goog_remb);
                }
            }
        }
    }
}

void
RtcpMuxSdpTest::answerKeepsRembFallbackWithoutTransportCcExtmap()
{
    CPPUNIT_ASSERT(account_);

    Sdp sdp("transport-cc-answer-no-extmap");
    sdp.setPublishedIP("127.0.0.1", pj_AF_INET());
    sdp.setLocalMediaCapabilities(MediaType::MEDIA_AUDIO, account_->getActiveAccountCodecInfoList(MEDIA_AUDIO));
    sdp.setLocalPublishedAudioPorts(TEST_AUDIO_RTP_PORT, 0);
    sdp.enableRtcpMux(true);

    auto pool = makePool("transport-cc-answer-no-extmap");
    const std::string remoteOffer = "v=0\r\n"
                                    "o=- 0 0 IN IP4 127.0.0.1\r\n"
                                    "s=-\r\n"
                                    "c=IN IP4 127.0.0.1\r\n"
                                    "t=0 0\r\n"
                                    "m=audio 5004 UDP/TLS/RTP/SAVPF 0\r\n"
                                    "a=rtpmap:0 PCMU/8000\r\n"
                                    "a=rtcp-mux\r\n"
                                    "a=rtcp-fb:0 transport-cc\r\n"
                                    "a=rtcp-fb:0 goog-remb\r\n";

    auto* session = parseSdp(pool.get(), remoteOffer);
    CPPUNIT_ASSERT(session);
    sdp.setReceivedOffer(session);

    MediaAttribute audio(MediaType::MEDIA_AUDIO);
    audio.label_ = "audio_0";
    audio.enabled_ = true;

    CPPUNIT_ASSERT(sdp.processIncomingOffer({audio}));
    auto* localSession = sdp.getLocalSdpSession();
    CPPUNIT_ASSERT(localSession);
    CPPUNIT_ASSERT_EQUAL(1u, localSession->media_count);
    CPPUNIT_ASSERT(!hasMediaAttributeValue(localSession->media[0], "rtcp-fb", "* transport-cc"));
    CPPUNIT_ASSERT(hasMediaAttributeValue(localSession->media[0], "rtcp-fb", "* goog-remb"));

    const auto descriptions = sdp.getMediaDescriptions(localSession, false);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), descriptions.size());
    CPPUNIT_ASSERT_EQUAL(0u, descriptions[0].transport_cc_rtp_ext_id);
    CPPUNIT_ASSERT(!descriptions[0].rtcp_fb_transport_cc);
    CPPUNIT_ASSERT(descriptions[0].rtcp_fb_goog_remb);
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
    CPPUNIT_ASSERT(!hasMediaAttributeValue(localSession->media[0],
                                           "extmap",
                                           std::string("1 ") + std::string(MID_RTP_EXTENSION_URI)));
    CPPUNIT_ASSERT(!hasMediaAttributeValue(localSession->media[1],
                                           "extmap",
                                           std::string("1 ") + std::string(MID_RTP_EXTENSION_URI)));
    CPPUNIT_ASSERT(hasMediaAttributeValue(localSession->media[0],
                                          "extmap",
                                          std::string("3 ") + std::string(TRANSPORT_CC_RTP_EXTENSION_URI)));
    CPPUNIT_ASSERT(hasMediaAttributeValue(localSession->media[1],
                                          "extmap",
                                          std::string("3 ") + std::string(TRANSPORT_CC_RTP_EXTENSION_URI)));

    const auto descriptions = sdp.getMediaDescriptions(localSession, false);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), descriptions.size());
    CPPUNIT_ASSERT(descriptions[0].mid.empty());
    CPPUNIT_ASSERT(descriptions[1].mid.empty());
    CPPUNIT_ASSERT_EQUAL(0u, descriptions[0].mid_rtp_ext_id);
    CPPUNIT_ASSERT_EQUAL(0u, descriptions[1].mid_rtp_ext_id);
    CPPUNIT_ASSERT_EQUAL(3u, descriptions[0].transport_cc_rtp_ext_id);
    CPPUNIT_ASSERT_EQUAL(3u, descriptions[1].transport_cc_rtp_ext_id);
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
RtcpMuxSdpTest::rejectsConflictingRtcpMuxPayloadTypes()
{
    auto pool = makePool("rtcp-mux-payloads");
    MediaAttribute audio(MEDIA_AUDIO);
    audio.enabled_ = true;
    audio.label_ = "audio_0";

    for (unsigned payload = 64; payload <= 95; ++payload) {
        Sdp sdp("rtcp-mux-conflicting-payload");
        sdp.setPublishedIP("127.0.0.1", pj_AF_INET());
        sdp.setLocalPublishedAudioPorts(TEST_AUDIO_RTP_PORT, 0);
        const auto codec = std::make_shared<SystemAudioCodecInfo>(
            *std::static_pointer_cast<SystemAudioCodecInfo>(account_->getActiveAccountCodecInfoList(MEDIA_AUDIO).front()));
        codec->payloadType = payload;
        sdp.setLocalMediaCapabilities(MEDIA_AUDIO, {codec});
        sdp.enableRtcpMux(true);
        CPPUNIT_ASSERT(!sdp.createOffer({audio}));

        const auto pt = std::to_string(payload);
        const auto raw = "v=0\r\n"
                         "o=- 0 0 IN IP4 127.0.0.1\r\n"
                         "s=-\r\n"
                         "c=IN IP4 127.0.0.1\r\n"
                         "t=0 0\r\n"
                         "m=audio 5004 RTP/AVP " + pt + "\r\n"
                         "a=rtpmap:" + pt + " opus/48000/2\r\n"
                         "a=rtcp-mux\r\n";
        CPPUNIT_ASSERT(!sdp.createOfferFromExternalSdp(raw));
        CPPUNIT_ASSERT(!sdp.setReceivedOfferFromExternalSdp(raw));

        auto* session = parseSdp(pool.get(), raw);
        CPPUNIT_ASSERT(session);
        sdp.setReceivedOffer(session);
        CPPUNIT_ASSERT(!sdp.processIncomingOffer({audio}));
        CPPUNIT_ASSERT(!sdp.setLocalAnswerFromExternalSdp(raw));
    }
}

void
RtcpMuxSdpTest::acceptsSafeAndNonMuxPayloadTypes()
{
    for (const bool mux : {false, true}) {
        for (const unsigned payload : {0u, 63u, 64u, 77u, 95u, 96u, 127u}) {
            if (mux && payload >= 64 && payload <= 95)
                continue;

            const auto pt = std::to_string(payload);
            const auto raw = "v=0\r\n"
                             "o=- 0 0 IN IP4 127.0.0.1\r\n"
                             "s=-\r\n"
                             "c=IN IP4 127.0.0.1\r\n"
                             "t=0 0\r\n"
                             "m=audio 5004 RTP/AVP " + pt + "\r\n"
                             "a=rtpmap:" + pt + (payload == 0 ? " PCMU/8000\r\n" : " opus/48000/2\r\n")
                             + (mux ? "a=rtcp-mux\r\n" : "");
            Sdp sdp("rtcp-mux-safe-payload");
            CPPUNIT_ASSERT(sdp.createOfferFromExternalSdp(raw));
            CPPUNIT_ASSERT(sdp.setReceivedOfferFromExternalSdp(raw));
            CPPUNIT_ASSERT(sdp.setLocalAnswerFromExternalSdp(raw));
        }
    }
}

void
RtcpMuxSdpTest::rejectedMediaDoesNotRequireMuxSafePayloads()
{
    auto pool = makePool("rtcp-mux-rejected-media");
    const std::string raw = "v=0\r\n"
                            "o=- 0 0 IN IP4 127.0.0.1\r\n"
                            "s=-\r\n"
                            "c=IN IP4 127.0.0.1\r\n"
                            "t=0 0\r\n"
                            "m=audio 0 RTP/AVP 77\r\n"
                            "a=rtpmap:77 opus/48000/2\r\n"
                            "a=rtcp-mux\r\n";
    auto* session = parseSdp(pool.get(), raw);
    CPPUNIT_ASSERT(session);
    CPPUNIT_ASSERT(Sdp::validateRtcpMuxPayloadTypes(session));

    CPPUNIT_ASSERT(pjmedia_sdp_media_add_attr(session->media[0],
                                              pjmedia_sdp_attr_create(pool.get(), "bundle-only", nullptr))
                   == PJ_SUCCESS);
    CPPUNIT_ASSERT(!Sdp::validateRtcpMuxPayloadTypes(session));
}

void
RtcpMuxSdpTest::negotiatedMediaFallsBackToSeparateRtcp()
{
    checkNegotiatedRtcp(false, false);
}

void
RtcpMuxSdpTest::negotiatedMediaPreservesExplicitRtcpFallback()
{
    checkNegotiatedRtcp(false, true);
}

void
RtcpMuxSdpTest::negotiatedMediaKeepsAcceptedRtcpMux()
{
    checkNegotiatedRtcp(true, true);
}

void
RtcpMuxSdpTest::checkNegotiatedRtcp(bool muxAccepted, bool explicitRtcp)
{
    for (const bool secure : {false, true}) {
        Sdp caller("rtcp-mux-negotiated-caller");
        Sdp peer("rtcp-mux-negotiated-peer");
        for (auto* sdp : {&caller, &peer}) {
            sdp->setPublishedIP("127.0.0.1", pj_AF_INET());
            sdp->setLocalMediaCapabilities(MEDIA_AUDIO, account_->getActiveAccountCodecInfoList(MEDIA_AUDIO));
            sdp->setLocalMediaCapabilities(MEDIA_VIDEO, account_->getActiveAccountCodecInfoList(MEDIA_VIDEO));
            sdp->setSecureMediaKeyExchange(KeyExchangeProtocol::SDES);
        }
        caller.setLocalPublishedAudioPorts(TEST_AUDIO_RTP_PORT, explicitRtcp ? 4100 : 0);
        caller.setLocalPublishedVideoPorts(TEST_AUDIO_RTP_PORT + 2, explicitRtcp ? 4102 : 0);
        caller.enableRtcpMux(true);
        peer.setLocalPublishedAudioPorts(5004, explicitRtcp ? 5104 : 0);
        peer.setLocalPublishedVideoPorts(5006, explicitRtcp ? 5106 : 0);
        peer.enableRtcpMux(muxAccepted);

        MediaAttribute audio(MEDIA_AUDIO);
        audio.label_ = "audio_0";
        audio.enabled_ = true;
        audio.secure_ = secure;
        MediaAttribute video(MEDIA_VIDEO);
        video.label_ = "video_0";
        video.enabled_ = true;
        video.secure_ = secure;
        const std::vector<MediaAttribute> media {audio, video};

        CPPUNIT_ASSERT(caller.createOffer(media));
        CPPUNIT_ASSERT(hasMediaAttribute(caller.getLocalSdpSession(), "rtcp-mux"));
        peer.setReceivedOffer(caller.getLocalSdpSession());
        CPPUNIT_ASSERT(peer.processIncomingOffer(media));
        CPPUNIT_ASSERT_EQUAL(muxAccepted, hasMediaAttribute(peer.getLocalSdpSession(), "rtcp-mux"));
        CPPUNIT_ASSERT(caller.getIceAttributes().ufrag.empty());
        CPPUNIT_ASSERT(peer.getIceAttributes().ufrag.empty());

        auto pool = makePool("rtcp-mux-negotiated");
        CPPUNIT_ASSERT(pjmedia_sdp_neg_set_remote_answer(pool.get(),
                                                         caller.saveLocalOfferState().negotiator,
                                                         peer.getLocalSdpSession())
                       == PJ_SUCCESS);
        CPPUNIT_ASSERT(caller.startNegotiation());
        CPPUNIT_ASSERT(peer.startNegotiation());

        // PJSIP retains the offered mux attribute even when the answer rejects it.
        CPPUNIT_ASSERT(hasMediaAttribute(caller.getActiveLocalSdpSession(), "rtcp-mux"));
        for (const auto& slots : {caller.getMediaSlots(), peer.getMediaSlots()}) {
            CPPUNIT_ASSERT_EQUAL(size_t(2), slots.size());
            for (const auto& [local, remote] : slots) {
                for (const auto* description : {&local, &remote}) {
                    CPPUNIT_ASSERT(description->enabled);
                    CPPUNIT_ASSERT_EQUAL(muxAccepted, description->rtcp_mux);
                    const auto rtpPort = description->addr.getPort();
                    const auto expectedRtcp = muxAccepted ? rtpPort
                                                          : static_cast<uint16_t>(rtpPort + (explicitRtcp ? 100 : 1));
                    CPPUNIT_ASSERT_EQUAL(expectedRtcp, description->rtcp_addr.getPort());
                    CPPUNIT_ASSERT_EQUAL(secure ? KeyExchangeProtocol::SDES : KeyExchangeProtocol::NONE,
                                         description->key_exchange);
                }
                if (secure) {
                    CPPUNIT_ASSERT(local.crypto);
                    CPPUNIT_ASSERT(remote.crypto);
                    CPPUNIT_ASSERT_EQUAL(local.crypto.getCryptoSuite(), remote.crypto.getCryptoSuite());
                }
            }
        }
    }
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
    CPPUNIT_ASSERT_EQUAL(0u, descriptions[0].transport_cc_rtp_ext_id);
    CPPUNIT_ASSERT(!descriptions[0].rtcp_fb_transport_cc);
    CPPUNIT_ASSERT(!descriptions[0].rtcp_fb_goog_remb);
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
    CPPUNIT_ASSERT_EQUAL(0u, descriptions[0].transport_cc_rtp_ext_id);
    CPPUNIT_ASSERT(!descriptions[0].rtcp_fb_transport_cc);
    CPPUNIT_ASSERT(!descriptions[0].rtcp_fb_goog_remb);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint16_t>(5004), descriptions[0].addr.getPort());
    CPPUNIT_ASSERT_EQUAL(static_cast<uint16_t>(5004), descriptions[0].rtcp_addr.getPort());
}

void
RtcpMuxSdpTest::remoteSdpParsesTransportCcFeedback()
{
    CPPUNIT_ASSERT(account_);

    Sdp sdp("transport-cc-remote");
    sdp.setPublishedIP("127.0.0.1", pj_AF_INET());
    sdp.setLocalMediaCapabilities(MediaType::MEDIA_AUDIO, account_->getActiveAccountCodecInfoList(MEDIA_AUDIO));
    sdp.setLocalPublishedAudioPorts(TEST_AUDIO_RTP_PORT, TEST_AUDIO_RTCP_PORT);

    auto pool = makePool("transport-cc-remote");
    const std::string remoteSdp = "v=0\r\n"
                                  "o=- 0 0 IN IP4 127.0.0.1\r\n"
                                  "s=-\r\n"
                                  "c=IN IP4 127.0.0.1\r\n"
                                  "t=0 0\r\n"
                                  "a=group:BUNDLE 0\r\n"
                                  "m=audio 5004 UDP/TLS/RTP/SAVPF 0\r\n"
                                  "a=rtpmap:0 PCMU/8000\r\n"
                                  "a=rtcp-mux\r\n"
                                  "a=mid:0\r\n"
                                  "a=extmap:3 "
                                  + std::string(TRANSPORT_CC_RTP_EXTENSION_URI)
                                  + "\r\n"
                                    "a=rtcp-fb:* transport-cc\r\n"
                                    "a=rtcp-fb:0 goog-remb\r\n";

    auto* session = parseSdp(pool.get(), remoteSdp);
    CPPUNIT_ASSERT(session);

    const auto descriptions = sdp.getMediaDescriptions(session, true);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), descriptions.size());
    CPPUNIT_ASSERT(descriptions[0].rtcp_mux);
    CPPUNIT_ASSERT_EQUAL(3u, descriptions[0].transport_cc_rtp_ext_id);
    CPPUNIT_ASSERT(descriptions[0].rtcp_fb_transport_cc);
    CPPUNIT_ASSERT(descriptions[0].rtcp_fb_goog_remb);
}

void
RtcpMuxSdpTest::parsesBrowserOfferWithoutSipSession()
{
    Sdp sdp("browser-conference-offer");
    const std::string offer = "v=0\r\n"
                              "o=- 0 0 IN IP4 127.0.0.1\r\n"
                              "s=-\r\n"
                              "c=IN IP4 127.0.0.1\r\n"
                              "t=0 0\r\n"
                              "a=group:BUNDLE 0\r\n"
                              "m=audio 9 UDP/TLS/RTP/SAVPF 111\r\n"
                              "a=mid:0\r\n"
                              "a=rtcp-mux\r\n"
                              "a=rtpmap:111 opus/48000/2\r\n"
                              "a=ice-ufrag:browser\r\n"
                              "a=ice-pwd:browserpassword0123456789\r\n"
                              "a=fingerprint:sha-256 "
                              "7B:8B:F0:65:5F:78:E2:51:3B:AC:6F:F3:3F:46:1B:35:DC:B8:5F:64:1A:24:C2:43:F0:A1:58:D0:A1:2C:19:08\r\n"
                              "a=setup:actpass\r\n";

    CPPUNIT_ASSERT(sdp.setReceivedOfferFromExternalSdp(offer));
    CPPUNIT_ASSERT_EQUAL(std::string("browser"), sdp.getIceAttributes().ufrag);
    CPPUNIT_ASSERT_EQUAL(1u, sdp.getRemoteSdpSession()->media_count);
    CPPUNIT_ASSERT(Sdp::toString(sdp.getRemoteSdpSession()).find("m=audio 9 UDP/TLS/RTP/SAVPF 111")
                   != std::string::npos);
    CPPUNIT_ASSERT(!sdp.setReceivedOfferFromExternalSdp("not an SDP"));
    CPPUNIT_ASSERT_EQUAL(std::string("browser"), sdp.getIceAttributes().ufrag);
}

void
RtcpMuxSdpTest::publishedIpAcceptsMediaLevelConnections()
{
    Sdp sdp("browser-published-address");
    const std::string offer = "v=0\r\n"
                              "o=- 1 1 IN IP4 127.0.0.1\r\n"
                              "s=-\r\n"
                              "t=0 0\r\n"
                              "m=audio 9 UDP/TLS/RTP/SAVPF 111\r\n"
                              "c=IN IP4 192.0.2.10\r\n"
                              "a=rtpmap:111 opus/48000/2\r\n"
                              "m=video 9 UDP/TLS/RTP/SAVPF 96\r\n"
                              "c=IN IP4 192.0.2.11\r\n"
                              "a=rtpmap:96 H264/90000\r\n";
    CPPUNIT_ASSERT(sdp.createOfferFromExternalSdp(offer));
    const auto* session = sdp.getLocalSdpSession();
    CPPUNIT_ASSERT(not session->conn);
    CPPUNIT_ASSERT_EQUAL(2u, session->media_count);

    sdp.setPublishedIP("2001:db8::1", pj_AF_INET6());

    CPPUNIT_ASSERT(not session->conn);
    CPPUNIT_ASSERT_EQUAL(std::string_view("IP6"), sip_utils::as_view(session->origin.addr_type));
    CPPUNIT_ASSERT_EQUAL(std::string_view("2001:db8::1"), sip_utils::as_view(session->origin.addr));
    CPPUNIT_ASSERT_EQUAL(std::string_view("192.0.2.10"), sip_utils::as_view(session->media[0]->conn->addr));
    CPPUNIT_ASSERT_EQUAL(std::string_view("192.0.2.11"), sip_utils::as_view(session->media[1]->conn->addr));
    CPPUNIT_ASSERT(pjmedia_sdp_validate(session) == PJ_SUCCESS);
}

void
RtcpMuxSdpTest::publishedIpUpdatesSessionConnection()
{
    Sdp sdp("native-published-address");
    const std::string offer = "v=0\r\n"
                              "o=- 1 1 IN IP4 127.0.0.1\r\n"
                              "s=-\r\n"
                              "c=IN IP4 127.0.0.1\r\n"
                              "t=0 0\r\n"
                              "m=audio 9 RTP/AVP 0\r\n"
                              "a=rtpmap:0 PCMU/8000\r\n";
    CPPUNIT_ASSERT(sdp.createOfferFromExternalSdp(offer));
    const auto* session = sdp.getLocalSdpSession();
    CPPUNIT_ASSERT(session->conn);

    sdp.setPublishedIP("192.0.2.1", pj_AF_INET());

    CPPUNIT_ASSERT_EQUAL(std::string_view("192.0.2.1"), sip_utils::as_view(session->origin.addr));
    CPPUNIT_ASSERT_EQUAL(std::string_view("192.0.2.1"), sip_utils::as_view(session->conn->addr));
    CPPUNIT_ASSERT(pjmedia_sdp_validate(session) == PJ_SUCCESS);
}

void
RtcpMuxSdpTest::negotiatesDynamicResolutionViaImageAttrAndWebRtc()
{
    CPPUNIT_ASSERT(account_);

    MediaAttribute video(MediaType::MEDIA_VIDEO);
    video.label_ = "video_0";
    video.enabled_ = true;
    video.secure_ = true;

    // 1. Modern Jami <-> Modern Jami (RTP/SAVP + RFC 6236 a=imageattr): enabled.
    Sdp offerer("imageattr-offerer");
    offerer.setPublishedIP("127.0.0.1", pj_AF_INET());
    offerer.setLocalMediaCapabilities(MediaType::MEDIA_VIDEO, account_->getActiveAccountCodecInfoList(MEDIA_VIDEO));
    offerer.setLocalPublishedVideoPorts(5004, 0);
    CPPUNIT_ASSERT(offerer.createOffer({video}));
    CPPUNIT_ASSERT(hasMediaAttributeName(offerer.getLocalSdpSession()->media[0], "imageattr"));

    Sdp answerer("imageattr-answerer");
    answerer.setPublishedIP("127.0.0.1", pj_AF_INET());
    answerer.setLocalMediaCapabilities(MediaType::MEDIA_VIDEO, account_->getActiveAccountCodecInfoList(MEDIA_VIDEO));
    answerer.setLocalPublishedVideoPorts(5006, 0);
    answerer.setReceivedOffer(offerer.getLocalSdpSession());
    CPPUNIT_ASSERT(answerer.processIncomingOffer({video}));
    CPPUNIT_ASSERT(hasMediaAttributeName(answerer.getLocalSdpSession()->media[0], "imageattr"));
    CPPUNIT_ASSERT(answerer.startNegotiation());
    const auto answererSlots = answerer.getMediaSlots();
    CPPUNIT_ASSERT_EQUAL(size_t(1), answererSlots.size());
    CPPUNIT_ASSERT(answererSlots[0].first.dynamic_resolution);
    CPPUNIT_ASSERT(answererSlots[0].second.dynamic_resolution);

    // 2. Legacy SIP / older Jami (RTP/SAVP without a=imageattr): omitted in answer and disabled.
    auto pool = makePool("imageattr-legacy");
    const std::string legacyOffer = "v=0\r\n"
                                    "o=- 0 0 IN IP4 127.0.0.1\r\n"
                                    "s=-\r\n"
                                    "c=IN IP4 127.0.0.1\r\n"
                                    "t=0 0\r\n"
                                    "m=video 5004 RTP/SAVP 96\r\n"
                                    "a=rtpmap:96 H264/90000\r\n"
                                    "a=crypto:1 AES_CM_128_HMAC_SHA1_80 "
                                    "inline:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\r\n";
    auto* legacySession = parseSdp(pool.get(), legacyOffer);
    CPPUNIT_ASSERT(legacySession);

    Sdp legacyAnswerer("imageattr-legacy-answerer");
    legacyAnswerer.setPublishedIP("127.0.0.1", pj_AF_INET());
    legacyAnswerer.setLocalMediaCapabilities(MediaType::MEDIA_VIDEO,
                                             account_->getActiveAccountCodecInfoList(MEDIA_VIDEO));
    legacyAnswerer.setLocalPublishedVideoPorts(5008, 0);
    legacyAnswerer.setReceivedOffer(legacySession);
    CPPUNIT_ASSERT(legacyAnswerer.processIncomingOffer({video}));
    CPPUNIT_ASSERT(!hasMediaAttributeName(legacyAnswerer.getLocalSdpSession()->media[0], "imageattr"));
    CPPUNIT_ASSERT(legacyAnswerer.startNegotiation());
    const auto legacySlots = legacyAnswerer.getMediaSlots();
    CPPUNIT_ASSERT_EQUAL(size_t(1), legacySlots.size());
    CPPUNIT_ASSERT(!legacySlots[0].first.dynamic_resolution);
    CPPUNIT_ASSERT(!legacySlots[0].second.dynamic_resolution);

    // 3. WebRTC endpoint (UDP/TLS/RTP/SAVPF + DTLS without a=imageattr): enabled per RFC 7742,
    //    unless remote explicitly restricts recv to a single fixed size via a=imageattr.
    static constexpr const char* TEST_FINGERPRINT
        = "00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF";
    const std::string webrtcOffer = std::string("v=0\r\n"
                                                "o=- 0 0 IN IP4 127.0.0.1\r\n"
                                                "s=-\r\n"
                                                "c=IN IP4 127.0.0.1\r\n"
                                                "t=0 0\r\n"
                                                "m=video 5004 UDP/TLS/RTP/SAVPF 96\r\n"
                                                "a=rtpmap:96 H264/90000\r\n"
                                                "a=setup:actpass\r\n"
                                                "a=fingerprint:sha-256 ")
                                    + TEST_FINGERPRINT + "\r\n";
    auto* webrtcSession = parseSdp(pool.get(), webrtcOffer);
    CPPUNIT_ASSERT(webrtcSession);

    Sdp webrtcAnswerer("imageattr-webrtc-answerer");
    webrtcAnswerer.setPublishedIP("127.0.0.1", pj_AF_INET());
    webrtcAnswerer.setLocalMediaCapabilities(MediaType::MEDIA_VIDEO,
                                             account_->getActiveAccountCodecInfoList(MEDIA_VIDEO));
    webrtcAnswerer.setLocalPublishedVideoPorts(5010, 0);
    webrtcAnswerer.setLocalDtlsFingerprint("sha-256", TEST_FINGERPRINT);
    webrtcAnswerer.setReceivedOffer(webrtcSession);
    CPPUNIT_ASSERT(webrtcAnswerer.processIncomingOffer({video}));
    CPPUNIT_ASSERT(!hasMediaAttributeName(webrtcAnswerer.getLocalSdpSession()->media[0], "imageattr"));
    CPPUNIT_ASSERT(webrtcAnswerer.startNegotiation());
    const auto webrtcSlots = webrtcAnswerer.getMediaSlots();
    CPPUNIT_ASSERT_EQUAL(size_t(1), webrtcSlots.size());
    CPPUNIT_ASSERT(webrtcSlots[0].second.dynamic_resolution);

    const std::string fixedOffer = webrtcOffer + "a=imageattr:96 recv [x=640,y=480]\r\n";
    auto* fixedSession = parseSdp(pool.get(), fixedOffer);
    CPPUNIT_ASSERT(fixedSession);
    const auto fixedDescrs = webrtcAnswerer.getMediaDescriptions(fixedSession, true);
    CPPUNIT_ASSERT_EQUAL(size_t(1), fixedDescrs.size());
    CPPUNIT_ASSERT(!fixedDescrs[0].dynamic_resolution);
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::RtcpMuxSdpTest::name());