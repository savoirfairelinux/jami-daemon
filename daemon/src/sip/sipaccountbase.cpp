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

#ifdef SFL_VIDEO
#include "video/libav_utils.h"
#endif

#include "account_schema.h"
#include <yaml-cpp/yaml.h>
#include "config/yamlparser.h"

bool SIPAccountBase::portsInUse_[HALF_MAX_PORT];

static const char *const VALID_SRTP_KEY_EXCHANGES[] = {"", "sdes", "zrtp"};

SIPAccountBase::SIPAccountBase(const std::string& accountID)
    : Account(accountID), link_(getSIPVoIPLink())
{}

template <typename T>
static void
validate(std::string &member, const std::string &param, const T& valid)
{
    const auto begin = std::begin(valid);
    const auto end = std::end(valid);
    if (find(begin, end, param) != end)
        member = param;
    else
        ERROR("Invalid parameter \"%s\"", param.c_str());
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
unserializeRange(const YAML::Node &node, const char *minKey, const char *maxKey, std::pair<uint16_t, uint16_t> &range)
{
    int tmpMin = 0;
    int tmpMax = 0;
    yaml_utils::parseValue(node, minKey, tmpMin);
    yaml_utils::parseValue(node, maxKey, tmpMax);
    updateRange(tmpMin, tmpMax, range);
}

template <typename T>
static void
parseInt(const std::map<std::string, std::string> &details, const char *key, T &i)
{
    const auto iter = details.find(key);
    if (iter == details.end()) {
        ERROR("Couldn't find key %s", key);
        return;
    }
    i = atoi(iter->second.c_str());
}

void SIPAccountBase::serialize(YAML::Emitter &out)
{
    using namespace Conf;

    Account::serialize(out);

    out << YAML::Key << AUDIO_PORT_MAX_KEY << YAML::Value << audioPortRange_.second;
    out << YAML::Key << AUDIO_PORT_MIN_KEY << YAML::Value << audioPortRange_.first;
    out << YAML::Key << DTMF_TYPE_KEY << YAML::Value << dtmfType_;
    out << YAML::Key << INTERFACE_KEY << YAML::Value << interface_;
    out << YAML::Key << PORT_KEY << YAML::Value << localPort_;
    out << YAML::Key << PUBLISH_ADDR_KEY << YAML::Value << publishedIpAddress_;
    out << YAML::Key << PUBLISH_PORT_KEY << YAML::Value << publishedPort_;
    out << YAML::Key << SAME_AS_LOCAL_KEY << YAML::Value << publishedSameasLocal_;

    // srtp submap
    out << YAML::Key << SRTP_KEY << YAML::Value << YAML::BeginMap;
    out << YAML::Key << SRTP_ENABLE_KEY << YAML::Value << srtpEnabled_;
    out << YAML::Key << KEY_EXCHANGE_KEY << YAML::Value << srtpKeyExchange_;
    out << YAML::Key << RTP_FALLBACK_KEY << YAML::Value << srtpFallback_;
    out << YAML::EndMap;

    out << YAML::Key << VIDEO_CODECS_KEY << YAML::Value << videoCodecList_;
    out << YAML::Key << VIDEO_ENABLED_KEY << YAML::Value << videoEnabled_;
    out << YAML::Key << VIDEO_PORT_MAX_KEY << YAML::Value << videoPortRange_.second;
    out << YAML::Key << VIDEO_PORT_MIN_KEY << YAML::Value << videoPortRange_.first;
}


void SIPAccountBase::unserialize(const YAML::Node &node)
{
    using namespace Conf;
    using namespace yaml_utils;

    Account::unserialize(node);

    parseValue(node, VIDEO_ENABLED_KEY, videoEnabled_);
    const auto &vCodecNode = node[VIDEO_CODECS_KEY];
    auto tmp = parseVectorMap(vCodecNode, {VIDEO_CODEC_BITRATE, VIDEO_CODEC_ENABLED, VIDEO_CODEC_NAME, VIDEO_CODEC_PARAMETERS});
#ifdef SFL_VIDEO
    if (tmp.empty()) {
        // Video codecs are an empty list
        WARN("Loading default video codecs");
        tmp = libav_utils::getDefaultCodecs();
    }
#endif
    // validate it
    setVideoCodecs(tmp);

    parseValue(node, INTERFACE_KEY, interface_);
    int port = DEFAULT_SIP_PORT;
    parseValue(node, PORT_KEY, port);
    localPort_ = port;
    parseValue(node, PUBLISH_ADDR_KEY, publishedIpAddress_);
    parseValue(node, PUBLISH_PORT_KEY, port);
    publishedPort_ = port;
    parseValue(node, SAME_AS_LOCAL_KEY, publishedSameasLocal_);

    parseValue(node, DTMF_TYPE_KEY, dtmfType_);

    // get srtp submap
    const auto &srtpMap = node[SRTP_KEY];
    parseValue(srtpMap, SRTP_ENABLE_KEY, srtpEnabled_);

    std::string tmpKey;
    parseValue(srtpMap, KEY_EXCHANGE_KEY, tmpKey);
    validate(srtpKeyExchange_, tmpKey, VALID_SRTP_KEY_EXCHANGES);
    parseValue(srtpMap, RTP_FALLBACK_KEY, srtpFallback_);

    unserializeRange(node, AUDIO_PORT_MIN_KEY, AUDIO_PORT_MAX_KEY, audioPortRange_);
    unserializeRange(node, VIDEO_PORT_MIN_KEY, VIDEO_PORT_MAX_KEY, videoPortRange_);
}


void SIPAccountBase::setAccountDetails(const std::map<std::string, std::string> &details)
{
    Account::setAccountDetails(details);

    parseBool(details, CONFIG_VIDEO_ENABLED, videoEnabled_);

    // general sip settings
    parseString(details, CONFIG_LOCAL_INTERFACE, interface_);
    parseBool(details, CONFIG_PUBLISHED_SAMEAS_LOCAL, publishedSameasLocal_);
    parseString(details, CONFIG_PUBLISHED_ADDRESS, publishedIpAddress_);
    parseInt(details, CONFIG_LOCAL_PORT, localPort_);
    parseInt(details, CONFIG_PUBLISHED_PORT, publishedPort_);

    parseString(details, CONFIG_ACCOUNT_DTMF_TYPE, dtmfType_);

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

    // srtp settings
    parseBool(details, CONFIG_SRTP_ENABLE, srtpEnabled_);
    parseBool(details, CONFIG_SRTP_RTP_FALLBACK, srtpFallback_);
    auto iter = details.find(CONFIG_SRTP_KEY_EXCHANGE);
    if (iter != details.end())
        validate(srtpKeyExchange_, iter->second, VALID_SRTP_KEY_EXCHANGES);
}

std::map<std::string, std::string>
SIPAccountBase::getAccountDetails() const
{
    std::map<std::string, std::string> a = Account::getAccountDetails();

    // note: The IP2IP profile will always have IP2IP as an alias
    a[CONFIG_VIDEO_ENABLED] = videoEnabled_ ? TRUE_STR : FALSE_STR;
    a[CONFIG_ACCOUNT_REGISTRATION_STATUS] = isIP2IP() ? "READY" : mapStateNumberToString(registrationState_);

    // Add sip specific details

    addRangeToDetails(a, CONFIG_ACCOUNT_AUDIO_PORT_MIN, CONFIG_ACCOUNT_AUDIO_PORT_MAX, audioPortRange_);
#ifdef SFL_VIDEO
    addRangeToDetails(a, CONFIG_ACCOUNT_VIDEO_PORT_MIN, CONFIG_ACCOUNT_VIDEO_PORT_MAX, videoPortRange_);
#endif

    a[CONFIG_ACCOUNT_DTMF_TYPE] = dtmfType_;
    a[CONFIG_LOCAL_INTERFACE] = interface_;
    a[CONFIG_PUBLISHED_SAMEAS_LOCAL] = publishedSameasLocal_ ? TRUE_STR : FALSE_STR;
    a[CONFIG_PUBLISHED_ADDRESS] = publishedIpAddress_;

    std::stringstream localport;
    localport << localPort_;
    a[CONFIG_LOCAL_PORT] = localport.str();
    std::stringstream publishedport;
    publishedport << publishedPort_;
    a[CONFIG_PUBLISHED_PORT] = publishedport.str();

    a[CONFIG_SRTP_KEY_EXCHANGE] = srtpKeyExchange_;
    a[CONFIG_SRTP_ENABLE] = srtpEnabled_ ? TRUE_STR : FALSE_STR;
    a[CONFIG_SRTP_RTP_FALLBACK] = srtpFallback_ ? TRUE_STR : FALSE_STR;
    return a;
}

void
SIPAccountBase::onTransportStateChanged(pjsip_transport_state state, const pjsip_transport_state_info *info)
{
    DEBUG("Transport state changed to %d for account %s !", state, accountID_.c_str());
    if (state == PJSIP_TP_STATE_DISCONNECTED
#if PJ_VERSION_NUM > (2 << 24 | 1 << 16)
    || state == PJSIP_TP_STATE_SHUTDOWN
    || state == PJSIP_TP_STATE_DESTROY
#else
    || !transport_ || !transport_->get()
    || transport_->get()->is_shutdown
    || transport_->get()->is_destroying
#endif
    ) {
        if (info) {
            char err_msg[128];
            err_msg[0] = '\0';
            pj_str_t descr = pjsip_strerror(info->status, err_msg, sizeof(err_msg));
            ERROR("Transport disconnected: %.*s", descr.slen, descr.ptr);
        }
        setRegistrationState(RegistrationState::ERROR_GENERIC);
        setTransport();
    }
}

void
SIPAccountBase::setTransport(const std::shared_ptr<SipTransport>& t)
{
    using namespace std::placeholders;
    if (t == transport_)
        return;
    if (transport_) {
        DEBUG("Removing transport from account");
        transport_->removeStateListener(reinterpret_cast<uintptr_t>(this));
    }

    transport_ = t;

    if (transport_)
        transport_->addStateListener(reinterpret_cast<uintptr_t>(this), std::bind(&SIPAccountBase::onTransportStateChanged, this, _1, _2));
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
