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

#ifdef RING_VIDEO
#include "libav_utils.h"
#endif

#include "account_schema.h"
#include "manager.h"

#include "config/yamlparser.h"
#include <yaml-cpp/yaml.h>

#include "client/signal.h"

namespace ring {

bool SIPAccountBase::portsInUse_[HALF_MAX_PORT];

SIPAccountBase::SIPAccountBase(const std::string& accountID)
    : Account(accountID), link_(getSIPVoIPLink())
{}

SIPAccountBase::~SIPAccountBase() {
    setTransport();
}

template <typename T>
static void
validate(std::string &member, const std::string &param, const T& valid)
{
    const auto begin = std::begin(valid);
    const auto end = std::end(valid);
    if (find(begin, end, param) != end)
        member = param;
    else
        RING_ERR("Invalid parameter \"%s\"", param.c_str());
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
        RING_ERR("Couldn't find key %s", key);
        return;
    }
    i = atoi(iter->second.c_str());
}

void SIPAccountBase::serialize(YAML::Emitter &out)
{
    Account::serialize(out);

    out << YAML::Key << Conf::AUDIO_PORT_MAX_KEY << YAML::Value << audioPortRange_.second;
    out << YAML::Key << Conf::AUDIO_PORT_MIN_KEY << YAML::Value << audioPortRange_.first;
    out << YAML::Key << Conf::DTMF_TYPE_KEY << YAML::Value << dtmfType_;
    out << YAML::Key << Conf::INTERFACE_KEY << YAML::Value << interface_;
    out << YAML::Key << Conf::PORT_KEY << YAML::Value << localPort_;
    out << YAML::Key << Conf::PUBLISH_ADDR_KEY << YAML::Value << publishedIpAddress_;
    out << YAML::Key << Conf::PUBLISH_PORT_KEY << YAML::Value << publishedPort_;
    out << YAML::Key << Conf::SAME_AS_LOCAL_KEY << YAML::Value << publishedSameasLocal_;

    out << YAML::Key << VIDEO_ENABLED_KEY << YAML::Value << videoEnabled_;
    out << YAML::Key << Conf::VIDEO_PORT_MAX_KEY << YAML::Value << videoPortRange_.second;
    out << YAML::Key << Conf::VIDEO_PORT_MIN_KEY << YAML::Value << videoPortRange_.first;
}

void SIPAccountBase::serializeTls(YAML::Emitter &out)
{
    out << YAML::Key << Conf::TLS_PORT_KEY << YAML::Value << tlsListenerPort_;
    out << YAML::Key << Conf::CALIST_KEY << YAML::Value << tlsCaListFile_;
    out << YAML::Key << Conf::CERTIFICATE_KEY << YAML::Value << tlsCertificateFile_;
    out << YAML::Key << Conf::TLS_PASSWORD_KEY << YAML::Value << tlsPassword_;
    out << YAML::Key << Conf::PRIVATE_KEY_KEY << YAML::Value << tlsPrivateKeyFile_;
}

void SIPAccountBase::unserialize(const YAML::Node &node)
{
    using yaml_utils::parseValue;
    using yaml_utils::parseVectorMap;

    Account::unserialize(node);

    parseValue(node, VIDEO_ENABLED_KEY, videoEnabled_);
    parseValue(node, Conf::INTERFACE_KEY, interface_);
    int port = DEFAULT_SIP_PORT;
    parseValue(node, Conf::PORT_KEY, port);
    localPort_ = port;

    parseValue(node, Conf::SAME_AS_LOCAL_KEY, publishedSameasLocal_);
    std::string publishedIpAddress;
    parseValue(node, Conf::PUBLISH_ADDR_KEY, publishedIpAddress);
    IpAddr publishedIp = publishedIpAddress;
    if (publishedIp and not publishedSameasLocal_)
        setPublishedAddress(publishedIp);

    parseValue(node, Conf::PUBLISH_PORT_KEY, port);
    publishedPort_ = port;

    parseValue(node, Conf::DTMF_TYPE_KEY, dtmfType_);

    // get tls submap
    const auto &tlsMap = node[Conf::TLS_KEY];
    parseValue(tlsMap, Conf::TLS_PORT_KEY, tlsListenerPort_);
    parseValue(tlsMap, Conf::CERTIFICATE_KEY, tlsCertificateFile_);
    parseValue(tlsMap, Conf::CALIST_KEY, tlsCaListFile_);
    parseValue(tlsMap, Conf::TLS_PASSWORD_KEY, tlsPassword_);
    parseValue(tlsMap, Conf::PRIVATE_KEY_KEY, tlsPrivateKeyFile_);

    unserializeRange(node, Conf::AUDIO_PORT_MIN_KEY, Conf::AUDIO_PORT_MAX_KEY, audioPortRange_);
    unserializeRange(node, Conf::VIDEO_PORT_MIN_KEY, Conf::VIDEO_PORT_MAX_KEY, videoPortRange_);
}


void SIPAccountBase::setAccountDetails(const std::map<std::string, std::string> &details)
{
    Account::setAccountDetails(details);

    parseBool(details, Conf::CONFIG_VIDEO_ENABLED, videoEnabled_);

    // general sip settings
    parseString(details, Conf::CONFIG_LOCAL_INTERFACE, interface_);
    parseBool(details, Conf::CONFIG_PUBLISHED_SAMEAS_LOCAL, publishedSameasLocal_);
    parseString(details, Conf::CONFIG_PUBLISHED_ADDRESS, publishedIpAddress_);
    parseInt(details, Conf::CONFIG_LOCAL_PORT, localPort_);
    parseInt(details, Conf::CONFIG_PUBLISHED_PORT, publishedPort_);

    parseString(details, Conf::CONFIG_ACCOUNT_DTMF_TYPE, dtmfType_);

    int tmpMin = -1;
    parseInt(details, Conf::CONFIG_ACCOUNT_AUDIO_PORT_MIN, tmpMin);
    int tmpMax = -1;
    parseInt(details, Conf::CONFIG_ACCOUNT_AUDIO_PORT_MAX, tmpMax);
    updateRange(tmpMin, tmpMax, audioPortRange_);
#ifdef RING_VIDEO
    tmpMin = -1;
    parseInt(details, Conf::CONFIG_ACCOUNT_VIDEO_PORT_MIN, tmpMin);
    tmpMax = -1;
    parseInt(details, Conf::CONFIG_ACCOUNT_VIDEO_PORT_MAX, tmpMax);
    updateRange(tmpMin, tmpMax, videoPortRange_);
#endif

    // TLS
    parseInt(details, Conf::CONFIG_TLS_LISTENER_PORT, tlsListenerPort_);
    parseString(details, Conf::CONFIG_TLS_CA_LIST_FILE, tlsCaListFile_);
    parseString(details, Conf::CONFIG_TLS_CERTIFICATE_FILE, tlsCertificateFile_);
    parseString(details, Conf::CONFIG_TLS_PRIVATE_KEY_FILE, tlsPrivateKeyFile_);
    parseString(details, Conf::CONFIG_TLS_PASSWORD, tlsPassword_);
}

std::map<std::string, std::string>
SIPAccountBase::getAccountDetails() const
{
    std::map<std::string, std::string> a = Account::getAccountDetails();

    // note: The IP2IP profile will always have IP2IP as an alias
    a[Conf::CONFIG_VIDEO_ENABLED] = videoEnabled_ ? TRUE_STR : FALSE_STR;
    a[Conf::CONFIG_ACCOUNT_REGISTRATION_STATUS] = isIP2IP() ? "READY" : mapStateNumberToString(registrationState_);

    // Add sip specific details

    addRangeToDetails(a, Conf::CONFIG_ACCOUNT_AUDIO_PORT_MIN, Conf::CONFIG_ACCOUNT_AUDIO_PORT_MAX, audioPortRange_);
#ifdef RING_VIDEO
    addRangeToDetails(a, Conf::CONFIG_ACCOUNT_VIDEO_PORT_MIN, Conf::CONFIG_ACCOUNT_VIDEO_PORT_MAX, videoPortRange_);
#endif

    a[Conf::CONFIG_ACCOUNT_DTMF_TYPE] = dtmfType_;
    a[Conf::CONFIG_LOCAL_INTERFACE] = interface_;
    a[Conf::CONFIG_PUBLISHED_SAMEAS_LOCAL] = publishedSameasLocal_ ? TRUE_STR : FALSE_STR;
    a[Conf::CONFIG_PUBLISHED_ADDRESS] = publishedIpAddress_;

    std::stringstream localport;
    localport << localPort_;
    a[Conf::CONFIG_LOCAL_PORT] = localport.str();
    std::stringstream publishedport;
    publishedport << publishedPort_;
    a[Conf::CONFIG_PUBLISHED_PORT] = publishedport.str();

    std::stringstream tlslistenerport;
    tlslistenerport << tlsListenerPort_;
    a[Conf::CONFIG_TLS_LISTENER_PORT] = tlslistenerport.str();
    a[Conf::CONFIG_TLS_CA_LIST_FILE] = tlsCaListFile_;
    a[Conf::CONFIG_TLS_CERTIFICATE_FILE] = tlsCertificateFile_;
    a[Conf::CONFIG_TLS_PRIVATE_KEY_FILE] = tlsPrivateKeyFile_;
    a[Conf::CONFIG_TLS_PASSWORD] = tlsPassword_;
    return a;
}

std::map<std::string, std::string>
SIPAccountBase::getVolatileAccountDetails() const
{
    std::map<std::string, std::string> a = Account::getVolatileAccountDetails();
    a[Conf::CONFIG_ACCOUNT_REGISTRATION_STATUS] = isIP2IP() ? "READY" : mapStateNumberToString(registrationState_);
    std::stringstream codestream;
    codestream << transportStatus_;
    a[Conf::CONFIG_TRANSPORT_STATE_CODE] = codestream.str();
    a[Conf::CONFIG_TRANSPORT_STATE_DESC] = transportError_ ;

    return a;
}

void
SIPAccountBase::onTransportStateChanged(pjsip_transport_state state, const pjsip_transport_state_info *info)
{
    pj_status_t currentStatus = transportStatus_;
    RING_DBG("Transport state changed to %s for account %s !", SipTransport::stateToStr(state), accountID_.c_str());
    if (!SipTransport::isAlive(transport_, state)) {
        if (info) {
            char err_msg[128];
            err_msg[0] = '\0';
            pj_str_t descr = pj_strerror(info->status, err_msg, sizeof(err_msg));
            transportStatus_ = info->status;
            transportError_  = std::string(descr.ptr, descr.slen);
            RING_ERR("Transport disconnected: %.*s", descr.slen, descr.ptr);
        }
        else {
            // This is already the generic error used by pjsip.
            transportStatus_ = PJSIP_SC_SERVICE_UNAVAILABLE;
            transportError_  = "";
        }
        setRegistrationState(RegistrationState::ERROR_GENERIC);
        setTransport();
    }
    else {
        // The status can be '0', this is the same as OK
        transportStatus_ = info && info->status ? info->status : PJSIP_SC_OK;
        transportError_  = "";
    }

    // Notify the client of the new transport state
    if (currentStatus != transportStatus_)
        emitSignal<DRing::ConfigurationSignal::VolatileDetailsChanged>(accountID_, getVolatileAccountDetails());
}

void
SIPAccountBase::setTransport(const std::shared_ptr<SipTransport>& t)
{
    if (t == transport_)
        return;
    if (transport_) {
        RING_DBG("Removing transport from account");
        transport_->removeStateListener(reinterpret_cast<uintptr_t>(this));
    }

    transport_ = t;

    if (transport_)
        transport_->addStateListener(reinterpret_cast<uintptr_t>(this), std::bind(&SIPAccountBase::onTransportStateChanged, this, std::placeholders::_1, std::placeholders::_2));
}

// returns even number in range [lower, upper]
uint16_t
SIPAccountBase::acquireRandomEvenPort(const std::pair<uint16_t, uint16_t>& range) const
{
    std::uniform_int_distribution<uint16_t> dist(range.first/2, range.second/2);
    uint16_t result;
    do {
        result = 2 * dist(rand_);
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
    return acquireRandomEvenPort(audioPortRange_);
}

#ifdef RING_VIDEO
uint16_t
SIPAccountBase::generateVideoPort() const
{
    return acquireRandomEvenPort(videoPortRange_);
}
#endif

pjsip_tpselector
SIPAccountBase::getTransportSelector() {
    if (!transport_)
        return SIPVoIPLink::getTransportSelector(nullptr);
    return SIPVoIPLink::getTransportSelector(transport_->get());
}

} // namespace ring
