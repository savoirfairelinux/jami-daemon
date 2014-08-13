/*
 *  Copyright (C) 2014 Savoir-Faire Linux Inc.
 *
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include "sipaccountbase.h"
#include "sipvoiplink.h"

#include "account_schema.h"
#include "config/yamlnode.h"

bool SIPAccountBase::portsInUse_[HALF_MAX_PORT];


static std::array<std::shared_ptr<Conf::ScalarNode>, 2>
serializeRange(Conf::MappingNode &accountMap, const char *minKey, const char *maxKey, const std::pair<uint16_t, uint16_t> &range)
{
    using namespace Conf;
    std::array<std::shared_ptr<ScalarNode>, 2> result;

    std::ostringstream os;
    os << range.first;
    result[0].reset(new ScalarNode(os.str()));
    os.str("");
    accountMap.setKeyValue(minKey, result[0]);

    os << range.second;
    ScalarNode portMax(os.str());
    result[1].reset(new ScalarNode(os.str()));
    accountMap.setKeyValue(maxKey, result[1]);
    return result;
}

static void
updateRange(int min, int max, std::pair<uint16_t, uint16_t> &range)
{
    if (min > 0 and (max > min) and max <= MAX_PORT - 2) {
        range.first = min;
        range.second = max;
    }
}

static void
unserializeRange(const Conf::YamlNode &mapNode, const char *minKey, const char *maxKey, std::pair<uint16_t, uint16_t> &range)
{
    int tmpMin = 0;
    int tmpMax = 0;
    mapNode.getValue(minKey, &tmpMin);
    mapNode.getValue(maxKey, &tmpMax);
    updateRange(tmpMin, tmpMax, range);
}


void
SIPAccountBase::toYaml(Conf::MappingNode& accountmap) const
{
    using namespace Conf;
    ScalarNode id(Account::accountID_);
    ScalarNode username(Account::username_);
    ScalarNode alias(Account::alias_);
    ScalarNode displayName(displayName_);

    ScalarNode enable(enabled_);
    ScalarNode autoAnswer(autoAnswerEnabled_);
    ScalarNode type(getAccountType());
    ScalarNode interface(interface_);
    std::stringstream portstr;
    portstr << localPort_;
    ScalarNode port(portstr.str());
    ScalarNode publishAddr(publishedIpAddress_);
    std::stringstream publicportstr;
    publicportstr << publishedPort_;

    ScalarNode publishPort(publicportstr.str());

    ScalarNode sameasLocal(publishedSameasLocal_);
    ScalarNode dtmfType(dtmfType_);
    ScalarNode audioCodecs(audioCodecStr_);
#ifdef SFL_VIDEO
    auto videoCodecs = std::make_shared<SequenceNode>();
    accountmap.setKeyValue(VIDEO_CODECS_KEY, videoCodecs);
    for (const auto& codec : videoCodecList_) {
        MappingNode mapNode(nullptr);
        mapNode.setKeyValue(VIDEO_CODEC_NAME, ScalarNode(codec.find(VIDEO_CODEC_NAME)->second));
        mapNode.setKeyValue(VIDEO_CODEC_BITRATE, ScalarNode(codec.find(VIDEO_CODEC_BITRATE)->second));
        mapNode.setKeyValue(VIDEO_CODEC_ENABLED, ScalarNode(codec.find(VIDEO_CODEC_ENABLED)->second));
        mapNode.setKeyValue(VIDEO_CODEC_PARAMETERS, ScalarNode(codec.find(VIDEO_CODEC_PARAMETERS)->second));
        videoCodecs->addNode(mapNode);
    }

#endif
    ScalarNode mailbox(mailBox_);
    ScalarNode ringtonePath(ringtonePath_);
    ScalarNode ringtoneEnabled(ringtoneEnabled_);
    ScalarNode videoEnabled(videoEnabled_);

    ScalarNode srtpenabled(srtpEnabled_);
    ScalarNode keyExchange(srtpKeyExchange_);
    ScalarNode rtpFallback(srtpFallback_);

    accountmap.setKeyValue(ALIAS_KEY, alias);
    accountmap.setKeyValue(TYPE_KEY, type);
    accountmap.setKeyValue(ID_KEY, id);
    accountmap.setKeyValue(USERNAME_KEY, username);
    accountmap.setKeyValue(DISPLAY_NAME_KEY, displayName);
    accountmap.setKeyValue(ACCOUNT_ENABLE_KEY, enable);
    accountmap.setKeyValue(ACCOUNT_AUTOANSWER_KEY, autoAnswer);
    accountmap.setKeyValue(MAILBOX_KEY, mailbox);
    accountmap.setKeyValue(INTERFACE_KEY, interface);
    accountmap.setKeyValue(PORT_KEY, port);
    accountmap.setKeyValue(PUBLISH_ADDR_KEY, publishAddr);
    accountmap.setKeyValue(PUBLISH_PORT_KEY, publishPort);
    accountmap.setKeyValue(SAME_AS_LOCAL_KEY, sameasLocal);
    accountmap.setKeyValue(AUDIO_CODECS_KEY, audioCodecs);
    accountmap.setKeyValue(DTMF_TYPE_KEY, dtmfType);
    accountmap.setKeyValue(RINGTONE_PATH_KEY, ringtonePath);
    accountmap.setKeyValue(RINGTONE_ENABLED_KEY, ringtoneEnabled);
    accountmap.setKeyValue(VIDEO_ENABLED_KEY, videoEnabled);

    auto srtpmap = std::make_shared<MappingNode>();
    accountmap.setKeyValue(SRTP_KEY, srtpmap);
    srtpmap->setKeyValue(SRTP_ENABLE_KEY, srtpenabled);
    srtpmap->setKeyValue(KEY_EXCHANGE_KEY, keyExchange);
    srtpmap->setKeyValue(RTP_FALLBACK_KEY, rtpFallback);

    ScalarNode userAgent(userAgent_);
    accountmap.setKeyValue(USER_AGENT_KEY, userAgent);

    ScalarNode hasCustomUserAgent(hasCustomUserAgent_);
    accountmap.setKeyValue(HAS_CUSTOM_USER_AGENT_KEY, hasCustomUserAgent);

    auto audioPortNodes(serializeRange(accountmap, AUDIO_PORT_MIN_KEY, AUDIO_PORT_MAX_KEY, audioPortRange_));
#ifdef SFL_VIDEO
    auto videoPortNodes(serializeRange(accountmap, VIDEO_PORT_MIN_KEY, VIDEO_PORT_MAX_KEY, videoPortRange_));
#endif

}

void
SIPAccountBase::fromYaml(const Conf::YamlNode& mapNode)
{
    using namespace Conf;
    unserializeRange(mapNode, AUDIO_PORT_MIN_KEY, AUDIO_PORT_MAX_KEY, audioPortRange_);
#ifdef SFL_VIDEO
    unserializeRange(mapNode, VIDEO_PORT_MIN_KEY, VIDEO_PORT_MAX_KEY, videoPortRange_);
#endif
}

void
SIPAccountBase::setAccountDetails(const std::map<std::string, std::string> &details)
{
    parseString(details, CONFIG_ACCOUNT_ALIAS, alias_);
    parseString(details, CONFIG_ACCOUNT_USERNAME, username_);
    parseBool(details, CONFIG_ACCOUNT_ENABLE, enabled_);
    parseBool(details, CONFIG_ACCOUNT_AUTOANSWER, autoAnswerEnabled_);
    parseString(details, CONFIG_RINGTONE_PATH, ringtonePath_);
    parseBool(details, CONFIG_RINGTONE_ENABLED, ringtoneEnabled_);
    parseBool(details, CONFIG_VIDEO_ENABLED, videoEnabled_);
    parseString(details, CONFIG_ACCOUNT_MAILBOX, mailBox_);

    parseString(details, CONFIG_LOCAL_INTERFACE, interface_);
    parseBool(details, CONFIG_PUBLISHED_SAMEAS_LOCAL, publishedSameasLocal_);
    parseString(details, CONFIG_PUBLISHED_ADDRESS, publishedIpAddress_);
    parseInt(details, CONFIG_LOCAL_PORT, localPort_);
    parseInt(details, CONFIG_PUBLISHED_PORT, publishedPort_);

    int tmpMin = -1;
    parseInt(details, CONFIG_ACCOUNT_AUDIO_PORT_MIN, tmpMin);
    int tmpMax = -1;
    parseInt(details, CONFIG_ACCOUNT_AUDIO_PORT_MAX, tmpMax);
    updateRange(tmpMin, tmpMax, audioPortRange_);
#ifdef SFL_VIDEO
    tmpMin = -1;
    parseInt(details, CONFIG_ACCOUNT_VIDEO_PORT_MIN, tmpMin);
    tmpMax = -1;
    parseInt(details, CONFIG_ACCOUNT_VIDEO_PORT_MAX, tmpMax);
    updateRange(tmpMin, tmpMax, videoPortRange_);
#endif
}


SIPAccountBase::SIPAccountBase(const std::string& accountID)
    : Account(accountID), link_(getSIPVoIPLink())
{}


void
SIPAccountBase::setTransport(pjsip_transport* transport, pjsip_tpfactory* lis)
{
    // release old transport
    if (transport_ && transport_ != transport) {
        pjsip_transport_dec_ref(transport_);
    }
    if (tlsListener_ && tlsListener_ != lis)
        tlsListener_->destroy(tlsListener_);
    // set new transport
    transport_ = transport;
    tlsListener_ = lis;
}

// returns even number in range [lower, upper]
uint16_t
SIPAccountBase::getRandomEvenNumber(const std::pair<uint16_t, uint16_t> &range)
{
    const uint16_t halfUpper = range.second * 0.5;
    const uint16_t halfLower = range.first * 0.5;
    uint16_t result;
    do {
        result = 2 * (halfLower + rand() % (halfUpper - halfLower + 1));
    } while (portsInUse_[result / 2]);

    portsInUse_[result / 2] = true;
    return result;
}

void
SIPAccountBase::releasePort(uint16_t port)
{
    portsInUse_[port / 2] = false;
}

uint16_t
SIPAccountBase::generateAudioPort() const
{
    return getRandomEvenNumber(audioPortRange_);
}

#ifdef SFL_VIDEO
uint16_t
SIPAccountBase::generateVideoPort() const
{
    return getRandomEvenNumber(videoPortRange_);
}
#endif
