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

#include "ringaccount.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sip/sdp.h"
#include "sip/sipvoiplink.h"
#include "sip/sipcall.h"
#include "sip/siptransport.h"

#include "sip_transport_ice.h"
#include "ice_transport.h"

#include <opendht/securedht.h>

#include "array_size.h"

#include "client/configurationmanager.h"

#include "account_schema.h"
#include "logger.h"
#include "manager.h"

#ifdef RING_VIDEO
#include "libav_utils.h"
#endif
#include "fileutils.h"

#include "config/yamlparser.h"
#include <yaml-cpp/yaml.h>

#include "upnp/upnp.h"

#include <algorithm>
#include <array>
#include <memory>
#include <sstream>
#include <cctype>

static constexpr int ICE_COMPONENTS {1};
static constexpr int ICE_COMP_SIP_TRANSPORT {0};
static constexpr int ICE_INIT_TIMEOUT {5};
static constexpr int ICE_NEGOTIATION_TIMEOUT {60};

constexpr const char * const RingAccount::ACCOUNT_TYPE;

RingAccount::RingAccount(const std::string& accountID, bool /* presenceEnabled */)
    : SIPAccountBase(accountID), tlsSetting_(), via_addr_()
{
    fileutils::check_dir(fileutils::get_cache_dir().c_str());
    cachePath_ = fileutils::get_cache_dir()+DIR_SEPARATOR_STR+getAccountID();

    /*  ~/.local/{appname}    */
    fileutils::check_dir(fileutils::get_data_dir().c_str());

    /*  ~/.local/{appname}/{accountID}    */
    idPath_ = fileutils::get_data_dir()+DIR_SEPARATOR_STR+getAccountID();
    fileutils::check_dir(idPath_.c_str());
    dataPath_ = idPath_ + DIR_SEPARATOR_STR "values";
    caPath_ = idPath_ + DIR_SEPARATOR_STR "certs";
    caListPath_ = idPath_ + DIR_SEPARATOR_STR "ca_list.pem";
    checkIdentityPath();

    int rc = gnutls_global_init();
    if (rc != GNUTLS_E_SUCCESS) {
        RING_ERR("Error initializing GnuTLS : %s", gnutls_strerror(rc));
        throw VoipLinkException("Can't initialize GnuTLS.");
    }
}

RingAccount::~RingAccount()
{
    Manager::instance().unregisterEventHandler((uintptr_t)this);
    setTransport();
    dht_.join();
    gnutls_global_deinit();
}

std::shared_ptr<SIPCall>
RingAccount::newIncomingCall(const std::string& from)
{
    std::lock_guard<std::mutex> lock(callsMutex_);
    auto call_it = pendingSipCalls_.begin();
    while (call_it != pendingSipCalls_.end()) {
        if (std::get<2>(*call_it)->getPeerNumber() == from) {
            auto call = std::get<2>(*call_it);
            pendingSipCalls_.erase(call_it);
            RING_WARN("Found matching call for %s", from.c_str());
            return call;
        } else {
            ++call_it;
        }
    }
    RING_ERR("Can't find matching call for %s", from.c_str());
    return nullptr;
}

template <>
std::shared_ptr<SIPCall>
RingAccount::newOutgoingCall(const std::string& id, const std::string& toUrl)
{
    auto dhtf = toUrl.find("ring:");
    if (dhtf != std::string::npos) {
        dhtf = dhtf+5;
    } else {
        dhtf = toUrl.find("sip:");
        dhtf = (dhtf == std::string::npos) ? 0 : dhtf+4;
    }
    if (toUrl.length() - dhtf < 40)
        throw std::invalid_argument("id must be a ring infohash");
    const std::string toUri = toUrl.substr(dhtf, 40);
    if (std::find_if_not(toUri.cbegin(), toUri.cend(), ::isxdigit) != toUri.cend())
        throw std::invalid_argument("id must be a ring infohash");

    RING_DBG("Calling DHT peer %s", toUri.c_str());
    auto toH = dht::InfoHash(toUri);

    auto call = Manager::instance().callFactory.newCall<SIPCall, RingAccount>(*this, id, Call::OUTGOING);
    call->setIPToIP(true);

    auto& iceTransportFactory = Manager::instance().getIceTransportFactory();
    auto ice = iceTransportFactory.createTransport(
        ("sip:"+call->getCallId()).c_str(),
        ICE_COMPONENTS,
        true,
        getUseUPnP()
    );
    if (not ice or ice->waitForInitialization(ICE_INIT_TIMEOUT) <= 0) {
        call->setConnectionState(Call::DISCONNECTED);
        call->setState(Call::MERROR);
        return call;
    }

    call->setState(Call::INACTIVE);
    call->setConnectionState(Call::TRYING);

    auto shared = shared_from_this();
    const auto callkey = dht::InfoHash::get("callto:"+toUri);

    const dht::Value::Id callvid  = std::uniform_int_distribution<dht::Value::Id>{}(rand_);
    const dht::Value::Id replyvid = callvid+1;
    dht_.putEncrypted(
        callkey,
        toH,
        dht::Value {
            ICE_ANNOUCEMENT_TYPE.id,
            ice->getLocalAttributesAndCandidates(),
            callvid
        },
        [callkey, callvid, call, shared](bool ok) {
            auto& this_ = *std::static_pointer_cast<RingAccount>(shared).get();
            if (!ok) {
                call->setConnectionState(Call::DISCONNECTED);
                Manager::instance().callFailure(*call);
            }
            this_.dht_.cancelPut(callkey, callvid);
        }
    );

    dht_.listen(
        callkey,
        [shared, call, callkey, ice, toH, replyvid] (const std::vector<std::shared_ptr<dht::Value>>& vals) {
            auto& this_ = *std::static_pointer_cast<RingAccount>(shared).get();
            RING_DBG("Outcall listen callback (%d values)", vals.size());
            for (const auto& v : vals) {
                if (v->recipient != this_.dht_.getId() || v->type != this_.ICE_ANNOUCEMENT_TYPE.id) {
                    RING_DBG("Ignoring non encrypted or bad type value %s.", v->toString().c_str());
                    continue;
                }
                if (v->id != replyvid)
                    continue;
                RING_WARN("Got a DHT reply from %s !", toH.toString().c_str());
                RING_WARN("Performing ICE negotiation.");
                ice->start(v->data);
                return false;
            }
            return true;
        }
    );
    {
        std::lock_guard<std::mutex> lock(callsMutex_);
        pendingCalls_.emplace_back(std::chrono::steady_clock::now(), ice, call, toH);
    }
    return call;
}

void
RingAccount::createOutgoingCall(const std::shared_ptr<SIPCall>& call, const std::string& to_id, IpAddr target)
{
    RING_WARN("RingAccount::createOutgoingCall to: %s target: %s tlsListener: %d", to_id.c_str(), target.toString(true).c_str(), tlsListener_?1:0);
    call->initIceTransport(true);
    call->setIPToIP(true);
    call->setPeerNumber(getToUri(to_id+"@"+target.toString(true).c_str()));
    call->initRecFilename(to_id);

    const auto localAddress = ip_utils::getInterfaceAddr(getLocalInterface());
    call->setCallMediaLocal(call->getIceTransport()->getDefaultLocalAddress());

    IpAddr addrSdp;
    if (getUseUPnP()) {
        /* use UPnP addr, or published addr if its set */
        addrSdp = getPublishedSameasLocal() ?
            getUPnPIpAddress() : getPublishedIpAddress();
    } else {
        addrSdp = isStunEnabled() or (not getPublishedSameasLocal()) ?
            getPublishedIpAddress() : localAddress;
    }

    // Initialize the session using ULAW as default codec in case of early media
    // The session should be ready to receive media once the first INVITE is sent, before
    // the session initialization is completed
    ring::AudioCodec* ac = Manager::instance().audioCodecFactory.instantiateCodec(PAYLOAD_CODEC_ULAW);
    if (!ac)
        throw VoipLinkException("Could not instantiate codec for early media");

    std::vector<ring::AudioCodec *> audioCodecs;
    audioCodecs.push_back(ac);

#if USE_CCRTP
    try {
        call->getAudioRtp().initConfig();
        call->getAudioRtp().initSession();

        if (isStunEnabled())
            call->updateSDPFromSTUN();

        call->getAudioRtp().initLocalCryptoInfo();
        call->getAudioRtp().start(audioCodecs);
    } catch (...) {
        throw VoipLinkException("Could not start rtp session for early media");
    }
#endif

    // Building the local SDP offer
    auto& sdp = call->getSDP();

    sdp.setPublishedIP(addrSdp);
    const bool created = sdp.createOffer(getActiveAudioCodecs(), getActiveVideoCodecs());

    if (not created or not SIPStartCall(call, target))
        throw VoipLinkException("Could not send outgoing INVITE request for new call");
}

std::shared_ptr<Call>
RingAccount::newOutgoingCall(const std::string& id, const std::string& toUrl)
{
    return newOutgoingCall<SIPCall>(id, toUrl);
}

bool
RingAccount::SIPStartCall(const std::shared_ptr<SIPCall>& call, IpAddr target)
{
    call->setupLocalSDPFromIce();
    std::string toUri(call->getPeerNumber()); // expecting a fully well formed sip uri

    pj_str_t pjTo = pj_str((char*) toUri.c_str());

    // Create the from header
    std::string from(getFromUri());
    pj_str_t pjFrom = pj_str((char*) from.c_str());

    std::string targetStr = getToUri(target.toString(true)/*+";transport=ICE"*/);
    pj_str_t pjTarget = pj_str((char*) targetStr.c_str());

    pj_str_t pjContact;
    {
        auto transport = call->getTransport();
        pjContact = getContactHeader(transport ? transport->get() : nullptr);
    }

    RING_DBG("contact header: %.*s / %s -> %s / %.*s",
          pjContact.slen, pjContact.ptr, from.c_str(), toUri.c_str(), pjTarget.slen, pjTarget.ptr);

    pjsip_dialog *dialog = NULL;
    if (pjsip_dlg_create_uac(pjsip_ua_instance(), &pjFrom, &pjContact, &pjTo, &pjTarget, &dialog) != PJ_SUCCESS) {
        RING_ERR("Unable to create SIP dialogs for user agent client when "
              "calling %s", toUri.c_str());
        return false;
    }

    pj_str_t subj_hdr_name = CONST_PJ_STR("Subject");
    pjsip_hdr* subj_hdr = (pjsip_hdr*) pjsip_parse_hdr(dialog->pool, &subj_hdr_name, (char *) "Phone call", 10, NULL);

    pj_list_push_back(&dialog->inv_hdr, subj_hdr);

    pjsip_inv_session* inv = nullptr;
    if (pjsip_inv_create_uac(dialog, call->getSDP().getLocalSdpSession(), 0, &inv) != PJ_SUCCESS) {
        RING_ERR("Unable to create invite session for user agent client");
        return false;
    }

    if (!inv) {
        RING_ERR("Call invite is not initialized");
        return PJ_FALSE;
    }

    call->inv.reset(inv);

/*
    updateDialogViaSentBy(dialog);
    if (hasServiceRoute())
        pjsip_dlg_set_route_set(dialog, sip_utils::createRouteSet(getServiceRoute(), call->inv->pool));
*/
    call->inv->mod_data[link_->getModId()] = (void*)call.get();

    pjsip_tx_data *tdata;

    if (pjsip_inv_invite(call->inv.get(), &tdata) != PJ_SUCCESS) {
        RING_ERR("Could not initialize invite messager for this call");
        return false;
    }

    //const pjsip_tpselector tp_sel = getTransportSelector();
    const pjsip_tpselector tp_sel = {PJSIP_TPSELECTOR_TRANSPORT, {call->getTransport()->get()}};
    if (pjsip_dlg_set_transport(dialog, &tp_sel) != PJ_SUCCESS) {
        RING_ERR("Unable to associate transport for invite session dialog");
        return false;
    }

    if (pjsip_inv_send_msg(call->inv.get(), tdata) != PJ_SUCCESS) {
        call->inv.reset();
        RING_ERR("Unable to send invite message for this call");
        return false;
    }

    call->setConnectionState(Call::PROGRESSING);
    call->setState(Call::ACTIVE);

    return true;
}

void RingAccount::serialize(YAML::Emitter &out)
{
    using namespace Conf;

    out << YAML::BeginMap;
    SIPAccountBase::serialize(out);
    out << YAML::Key << DHT_PORT_KEY << YAML::Value << dhtPort_;
    out << YAML::Key << DHT_PRIVKEY_PATH_KEY << YAML::Value << privkeyPath_;
    out << YAML::Key << DHT_CERT_PATH_KEY << YAML::Value << certPath_;
    out << YAML::Key << DHT_CA_CERT_PATH_KEY << YAML::Value << cacertPath_;

    // tls submap
    out << YAML::Key << TLS_KEY << YAML::Value << YAML::BeginMap;
    SIPAccountBase::serializeTls(out);
    out << YAML::EndMap;

    out << YAML::EndMap;
}

void RingAccount::unserialize(const YAML::Node &node)
{
    using namespace yaml_utils;
    SIPAccountBase::unserialize(node);
    in_port_t port {DHT_DEFAULT_PORT};
    parseValue(node, Conf::DHT_PORT_KEY, port);
    dhtPort_ = port ? port : DHT_DEFAULT_PORT;
    dhtPortUsed_ = dhtPort_;
    parseValue(node, Conf::DHT_PRIVKEY_PATH_KEY, privkeyPath_);
    parseValue(node, Conf::DHT_CERT_PATH_KEY, certPath_);
    parseValue(node, Conf::DHT_CA_CERT_PATH_KEY, cacertPath_);
    checkIdentityPath();
}

void
RingAccount::checkIdentityPath()
{
    if (not privkeyPath_.empty() and not dataPath_.empty())
        return;

    const auto idPath = fileutils::get_data_dir()+DIR_SEPARATOR_STR+getAccountID();
    privkeyPath_ = idPath + DIR_SEPARATOR_STR "dht.key";
    certPath_ = idPath + DIR_SEPARATOR_STR "dht.crt";
    cacertPath_ = idPath + DIR_SEPARATOR_STR "ca.crt";
}

std::vector<uint8_t>
loadFile(const std::string& path)
{
    std::vector<uint8_t> buffer;
    std::ifstream file(path, std::ios::binary);
    if (!file)
        throw std::runtime_error("Can't read file: "+path);
    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    if (size > std::numeric_limits<unsigned>::max())
        throw std::runtime_error("File is too big: "+path);
    buffer.resize(size);
    file.seekg(0, std::ios::beg);
    if (!file.read((char*)buffer.data(), size))
        throw std::runtime_error("Can't load file: "+path);
    return buffer;
}

void
saveFile(const std::string& path, const std::vector<uint8_t>& data)
{
    std::ofstream file(path, std::ios::trunc | std::ios::binary);
    if (!file.is_open()) {
        RING_ERR("Could not write data to %s", path.c_str());
        return;
    }
    file.write((char*)data.data(), data.size());
}

std::pair<std::shared_ptr<dht::crypto::Certificate>, dht::crypto::Identity>
RingAccount::loadIdentity()
{
    dht::crypto::Certificate ca_cert;

    dht::crypto::Certificate dht_cert;
    dht::crypto::PrivateKey dht_key;

    try {
        ca_cert = dht::crypto::Certificate(fileutils::loadFile(cacertPath_));
        dht_cert = dht::crypto::Certificate(fileutils::loadFile(certPath_));
        dht_key = dht::crypto::PrivateKey(fileutils::loadFile(privkeyPath_));
    }
    catch (const std::exception& e) {
        RING_ERR("Error loading identity: %s", e.what());
        auto ca = dht::crypto::generateIdentity("Ring CA");
        if (!ca.first || !ca.second) {
            throw VoipLinkException("Can't generate CA for this account.");
        }
        auto id = dht::crypto::generateIdentity("Ring", ca);
        if (!id.first || !id.second) {
            throw VoipLinkException("Can't generate identity for this account.");
        }
        idPath_ = fileutils::get_data_dir() + DIR_SEPARATOR_STR + getAccountID();
        fileutils::check_dir(idPath_.c_str());

        saveIdentity(ca, idPath_ + DIR_SEPARATOR_STR "ca");
        cacertPath_ = idPath_ + DIR_SEPARATOR_STR "ca.crt";

        saveIdentity(id, idPath_ + DIR_SEPARATOR_STR "dht");
        certPath_ = idPath_ + DIR_SEPARATOR_STR "dht.crt";
        privkeyPath_ = idPath_ + DIR_SEPARATOR_STR "dht.key";

        return {ca.second, id};
    }

    return {
        std::make_shared<dht::crypto::Certificate>(std::move(ca_cert)),
        {
            std::make_shared<dht::crypto::PrivateKey>(std::move(dht_key)),
            std::make_shared<dht::crypto::Certificate>(std::move(dht_cert))
        }
    };
}

void
RingAccount::saveIdentity(const dht::crypto::Identity id, const std::string& path) const
{
    if (id.first)
        fileutils::saveFile(path + ".key", id.first->serialize());
    if (id.second)
        fileutils::saveFile(path + ".crt", id.second->getPacked());
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

void RingAccount::setAccountDetails(const std::map<std::string, std::string> &details)
{
    SIPAccountBase::setAccountDetails(details);
    if (hostname_ == "")
        hostname_ = DHT_DEFAULT_BOOTSTRAP;
    parseInt(details, CONFIG_DHT_PORT, dhtPort_);
    if (dhtPort_ == 0)
        dhtPort_ = DHT_DEFAULT_PORT;
    dhtPortUsed_ = dhtPort_;
    parseString(details, CONFIG_DHT_PRIVKEY_PATH, privkeyPath_);
    parseString(details, CONFIG_DHT_CERT_PATH, certPath_);
    checkIdentityPath();
}

std::map<std::string, std::string> RingAccount::getAccountDetails() const
{
    std::map<std::string, std::string> a = SIPAccountBase::getAccountDetails();

    std::stringstream dhtport;
    dhtport << dhtPort_;
    a[CONFIG_DHT_PORT] = dhtport.str();
    a[CONFIG_DHT_PRIVKEY_PATH] = privkeyPath_;
    a[CONFIG_DHT_CERT_PATH] = certPath_;
    return a;
}

void
RingAccount::handleEvents()
{
    dht_.loop();

    std::lock_guard<std::mutex> lock(callsMutex_);
    auto now = std::chrono::steady_clock::now();
    auto c = pendingCalls_.begin();
    while (c != pendingCalls_.end()) {
        auto ice = std::get<1>(*c);
        auto call = std::get<2>(*c);
        if (ice->isRunning()) {
            call->setTransport(link_->sipTransport->getIceTransport(ice, ICE_COMP_SIP_TRANSPORT));
            call->setConnectionState(Call::PROGRESSING);
            auto id = std::get<3>(*c);
            if (id == dht::InfoHash()) {
                RING_WARN("ICE succeeded : moving incomming call to pending sip call");
                auto in = c;
                ++c;
                pendingSipCalls_.splice(pendingSipCalls_.begin(), pendingCalls_, in, c);
            } else {
                RING_WARN("ICE succeeded : removing pending outgoing call");
                createOutgoingCall(call, id.toString(), ice->getRemoteAddress(ICE_COMP_SIP_TRANSPORT));
                c = pendingCalls_.erase(c);
            }
        } else if (ice->isFailed() || now - std::get<0>(*c) > std::chrono::seconds(ICE_NEGOTIATION_TIMEOUT)) {
            RING_WARN("ICE timeout : removing pending outgoing call");
            call->setConnectionState(Call::DISCONNECTED);
            Manager::instance().callFailure(*call);
            c = pendingCalls_.erase(c);
        } else
            ++c;
    }
}

bool RingAccount::mapPortUPnP()
{
    if (useUPnP_) {
        /* create port mapping from published port to local port to the local IP
         * note that since different RING accounts can use the same port,
         * it may already be open, thats OK
         *
         * if the desired port is taken by another client, then it will try to map
         * a different port, if succesfull, then we have to use that port for DHT
         */
        uint16_t port_used;
        if (upnp_.addAnyMapping(dhtPort_, upnp::PortType::UDP, false, &port_used)) {
            if (port_used != dhtPort_)
                RING_DBG("UPnP could not map port %u for DHT, using %u instead", dhtPort_, port_used);
            dhtPortUsed_ = port_used;
            return true;
        } else {
            /* failed to map any port */
            return false;
        }
    } else {
        /* not using UPnP, so return true */
        return true;
    }
}

void RingAccount::doRegister()
{
    if (not isEnabled()) {
        RING_WARN("Account must be enabled to register, ignoring");
        return;
    }

    if (not mapPortUPnP())
        RING_WARN("Could not successfully map DHT port with UPnP, continuing with account registration anyways.");

    try {
        loadTreatedCalls();
        if (dht_.isRunning()) {
            RING_ERR("DHT already running (stopping it first).");
            dht_.join();
        }
        auto identity = loadIdentity();
        dht_.run(dhtPortUsed_, identity.second, false, [=](dht::Dht::Status s4, dht::Dht::Status s6) {
            RING_WARN("Dht status : %d %d", (int)s4, (int)s6);
            auto status = std::max(s4, s6);
            switch(status) {
            case dht::Dht::Status::Connecting:
            case dht::Dht::Status::Connected:
                setRegistrationState(status == dht::Dht::Status::Connected ? RegistrationState::REGISTERED : RegistrationState::TRYING);
                /*if (!tlsListener_) {
                    initTlsConfiguration();
                    tlsListener_ = link_->sipTransport->getTlsListener(
                        SipTransportDescr {getTransportType(), getTlsListenerPort(), getLocalInterface()},
                        getTlsSetting());
                    if (!tlsListener_) {
                        setRegistrationState(RegistrationState::ERROR_GENERIC);
                        RING_ERR("Error creating TLS listener.");
                        return;
                    }
                }*/
                break;
            case dht::Dht::Status::Disconnected:
            default:
                setRegistrationState(status == dht::Dht::Status::Disconnected ? RegistrationState::UNREGISTERED : RegistrationState::ERROR_GENERIC);
                tlsListener_.reset();
                break;
            }
        });

#if 0
        dht_.setLoggers(
            [](char const* m, va_list args){ vlogger(LOG_ERR, m, args); },
            [](char const* m, va_list args){ vlogger(LOG_WARNING, m, args); },
            [](char const* m, va_list args){ vlogger(LOG_DEBUG, m, args); }
        );
#endif

        dht_.registerType(USER_PROFILE_TYPE);
        dht_.registerType(ICE_ANNOUCEMENT_TYPE);

        dht_.importValues(loadValues());

        // Publish our own CA
        dht_.put(identity.first->getPublicKey().getId(), dht::Value {
            dht::CERTIFICATE_TYPE,
            *identity.first,
            1
        });

        username_ = dht_.getId().toString();

        Manager::instance().registerEventHandler((uintptr_t)this, [this]{ handleEvents(); });
        setRegistrationState(RegistrationState::TRYING);

        dht_.bootstrap(loadNodes());
        if (!hostname_.empty()) {
            std::stringstream ss(hostname_);
            std::vector<sockaddr_storage> bootstrap;
            std::string node_addr;
            while (std::getline(ss, node_addr, ';')) {
                auto ips = ip_utils::getAddrList(node_addr);
                if (ips.empty()) {
                    IpAddr resolved(node_addr);
                    if (resolved) {
                        if (resolved.getPort() == 0)
                            resolved.setPort(DHT_DEFAULT_PORT);
                        bootstrap.push_back(resolved);
                    }
                } else {
                    for (auto& ip : ips)
                        if (ip.getPort() == 0)
                            ip.setPort(DHT_DEFAULT_PORT);
                    bootstrap.insert(bootstrap.end(), ips.begin(), ips.end());
                }
            }
            for (auto ip : bootstrap)
                RING_DBG("Bootstrap node: %s", IpAddr(ip).toString(true).c_str());
            dht_.bootstrap(bootstrap);
        }

        // Listen for incoming calls
        auto shared = shared_from_this();
        auto listenKey = "callto:"+dht_.getId().toString();
        RING_WARN("Listening on %s : %s", listenKey.c_str(), dht::InfoHash::get(listenKey).toString().c_str());
        dht_.listen (
            listenKey,
            [shared,listenKey] (const std::vector<std::shared_ptr<dht::Value>>& vals) {
                auto& this_ = *std::static_pointer_cast<RingAccount>(shared).get();
                for (const auto& v : vals) {
                    std::shared_ptr<SIPCall> call;
                    try {
                        if (v->recipient != this_.dht_.getId() || v->type != this_.ICE_ANNOUCEMENT_TYPE.id) {
                            RING_DBG("Ignoring non encrypted or bad type value %s.", v->toString().c_str());
                            continue;
                        }
                        if (v->owner.getId() == this_.dht_.getId())
                            continue;
                        auto res = this_.treatedCalls_.insert(v->id);
                        this_.saveTreatedCalls();
                        if (!res.second)
                            continue;
                        auto from = v->owner.getId().toString();
                        auto from_vid = v->id;
                        auto reply_vid = from_vid+1;
                        RING_WARN("Received incomming DHT call request from %s (vid %llx) !!", from.c_str(), from_vid);
                        call = Manager::instance().callFactory.newCall<SIPCall, RingAccount>(this_, Manager::instance().getNewCallID(), Call::INCOMING);
                        auto& iceTransportFactory = Manager::instance().getIceTransportFactory();
                        auto ice = iceTransportFactory.createTransport(
                            ("sip:"+call->getCallId()).c_str(),
                            ICE_COMPONENTS,
                            false,
                            this_.getUseUPnP()
                        );
                        if (ice->waitForInitialization(ICE_INIT_TIMEOUT) <= 0)
                            throw std::runtime_error("Can't initialize ICE..");

                        this_.dht_.putEncrypted(
                            listenKey,
                            v->owner.getId(),
                            dht::Value {
                                this_.ICE_ANNOUCEMENT_TYPE.id,
                                ice->getLocalAttributesAndCandidates(),
                                reply_vid
                            },
                            [call,ice,shared,listenKey,reply_vid](bool ok) {
                                auto& this_ = *std::static_pointer_cast<RingAccount>(shared).get();
                                this_.dht_.cancelPut(listenKey, reply_vid);
                                if (!ok) {
                                    RING_ERR("ICE exchange failed");
                                    call->setConnectionState(Call::DISCONNECTED);
                                    Manager::instance().callFailure(*call);
                                }
                            }
                        );
                        ice->start(v->data);
                        call->setPeerNumber(from);
                        call->initRecFilename(from);
                        {
                            std::lock_guard<std::mutex> lock(this_.callsMutex_);
                            this_.pendingCalls_.emplace_back(std::chrono::steady_clock::now(), ice, call, dht::InfoHash());
                        }
                        return true;
                    } catch (const std::exception& e) {
                        RING_ERR("ICE/DHT error: %s", e.what());
                        if (call) {
                            call->setConnectionState(Call::DISCONNECTED);
                            Manager::instance().callFailure(*call);
                        }
                    }
                }
                return true;
            }
        );
    }
    catch (const std::exception& e) {
        RING_ERR("Error registering DHT account: %s", e.what());
        setRegistrationState(RegistrationState::ERROR_GENERIC);
    }
}


void RingAccount::doUnregister(std::function<void(bool)> released_cb)
{
    {
        std::lock_guard<std::mutex> lock(callsMutex_);
        pendingCalls_.clear();
        pendingSipCalls_.clear();
    }
    Manager::instance().unregisterEventHandler((uintptr_t)this);
    saveNodes(dht_.exportNodes());
    saveValues(dht_.exportValues());
    dht_.join();
    tlsListener_.reset();
    setRegistrationState(RegistrationState::UNREGISTERED);
    if (released_cb)
        released_cb(false);
    /* reset the UPnP controller to remove any port mappings */
    upnp_ = upnp::Controller(useUPnP_);
}

void
RingAccount::registerCA(const dht::crypto::Certificate& crt)
{
    saveFile(caPath_ + DIR_SEPARATOR_STR + crt.getPublicKey().getId().toString(), crt.getPacked());
}

bool
RingAccount::unregisterCA(const dht::InfoHash& crt_id)
{
    auto cas = getRegistredCAs();
    bool deleted = false;
    for (const auto& ca_path : cas) {
        try {
            dht::crypto::Certificate tmp_crt(loadFile(ca_path));
            if (tmp_crt.getPublicKey().getId() == crt_id)
                deleted &= remove(ca_path.c_str()) == 0;
        } catch (const std::exception&) {}
    }
    return deleted;
}

void
RingAccount::loadTreatedCalls()
{
    std::string treatedcallPath = cachePath_+DIR_SEPARATOR_STR "treatedCalls";
    {
        std::ifstream file(treatedcallPath);
        if (!file.is_open()) {
            RING_WARN("Could not load treated calls from %s", treatedcallPath.c_str());
            return;
        }
        std::string line;
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            dht::Value::Id vid;
            if (!(iss >> std::hex >> vid)) { break; }
            treatedCalls_.insert(vid);
        }
    }
}

void
RingAccount::saveTreatedCalls() const
{
    fileutils::check_dir(cachePath_.c_str());
    std::string treatedcallPath = cachePath_+DIR_SEPARATOR_STR "treatedCalls";
    {
        std::ofstream file(treatedcallPath, std::ios::trunc);
        if (!file.is_open()) {
            RING_ERR("Could not save treated calls to %s", treatedcallPath.c_str());
            return;
        }
        for (auto& c : treatedCalls_)
            file << std::hex << c << "\n";
    }
}

std::vector<std::string>
RingAccount::getRegistredCAs()
{
    return fileutils::readDirectory(caPath_);
}

void
RingAccount::regenerateCAList()
{
    std::ofstream list(caListPath_, std::ios::trunc | std::ios::binary);
    if (!list.is_open()) {
        RING_ERR("Could write CA list");
        return;
    }
    auto cas = getRegistredCAs();
    {
        std::ifstream file(cacertPath_, std::ios::binary);
        list << file.rdbuf();
    }
    for (const auto& ca : cas) {
        std::ifstream file(ca, std::ios::binary);
        if (!file)
            continue;
        list << file.rdbuf();
    }
}

void RingAccount::saveNodes(const std::vector<dht::Dht::NodeExport>& nodes) const
{
    if (nodes.empty())
        return;
    fileutils::check_dir(cachePath_.c_str());
    std::string nodesPath = cachePath_+DIR_SEPARATOR_STR "nodes";
    {
        std::ofstream file(nodesPath, std::ios::trunc);
        if (!file.is_open()) {
            RING_ERR("Could not save nodes to %s", nodesPath.c_str());
            return;
        }
        for (auto& n : nodes)
            file << n.id << " " << IpAddr(n.ss).toString(true) << "\n";
    }
}

void RingAccount::saveValues(const std::vector<dht::Dht::ValuesExport>& values) const
{
    fileutils::check_dir(dataPath_.c_str());
    for (const auto& v : values) {
        const std::string fname = dataPath_ + DIR_SEPARATOR_STR + v.first.toString();
        std::ofstream file(fname, std::ios::trunc | std::ios::out | std::ios::binary);
        file.write((const char*)v.second.data(), v.second.size());
    }
}

std::vector<dht::Dht::NodeExport>
RingAccount::loadNodes() const
{
    std::vector<dht::Dht::NodeExport> nodes;
    std::string nodesPath = cachePath_+DIR_SEPARATOR_STR "nodes";
    {
        std::ifstream file(nodesPath);
        if (!file.is_open()) {
            RING_ERR("Could not load nodes from %s", nodesPath.c_str());
            return nodes;
        }
        std::string line;
        while (std::getline(file, line))
        {
            std::istringstream iss(line);
            std::string id, ipstr;
            if (!(iss >> id >> ipstr)) { break; }
            IpAddr ip {ipstr};
            dht::Dht::NodeExport e {{id}, ip, ip.getLength()};
            nodes.push_back(e);
        }
    }
    return nodes;
}

std::vector<dht::Dht::ValuesExport>
RingAccount::loadValues() const
{
    std::vector<dht::Dht::ValuesExport> values;
    const auto dircontent(fileutils::readDirectory(dataPath_));
    for (const auto& fname : dircontent) {
        try {
            std::ifstream ifs(dataPath_+DIR_SEPARATOR_STR+fname, std::ifstream::in | std::ifstream::binary);
            std::istreambuf_iterator<char> begin(ifs), end;
            values.push_back({{fname}, std::vector<uint8_t>{begin, end}});
        } catch (const std::exception& e) {
            RING_ERR("Error reading value: %s", e.what());
            continue;
        }
    }
    return values;
}

void RingAccount::initTlsConfiguration()
{
    // TLS listener is unique and should be only modified through IP2IP_PROFILE
    pjsip_tls_setting_default(&tlsSetting_);
    regenerateCAList();

    pj_cstr(&tlsSetting_.ca_list_file, caListPath_.c_str());
    pj_cstr(&tlsSetting_.cert_file, certPath_.c_str());
    pj_cstr(&tlsSetting_.privkey_file, privkeyPath_.c_str());
    pj_cstr(&tlsSetting_.password, "");
    tlsSetting_.method = PJSIP_TLSV1_METHOD;
    tlsSetting_.ciphers_num = 0;
    tlsSetting_.ciphers = nullptr;
    tlsSetting_.verify_server = false;
    tlsSetting_.verify_client = false;
    tlsSetting_.require_client_cert = false;
    tlsSetting_.timeout.sec = 2;
    tlsSetting_.qos_type = PJ_QOS_TYPE_BEST_EFFORT;
    tlsSetting_.qos_ignore_error = PJ_TRUE;
}

void RingAccount::loadConfig()
{
    RING_WARN("RingAccount::loadConfig()");
    initTlsConfiguration();
    transportType_ = PJSIP_TRANSPORT_TLS;
}

MatchRank
RingAccount::matches(const std::string &userName, const std::string &server) const
{
    auto dhtId = dht_.getId().toString();
    if (userName == dhtId || server == dhtId) {
        RING_DBG("Matching account id in request with username %s", userName.c_str());
        return MatchRank::FULL;
    } else {
        RING_DBG("No match for account %s in request with username %s", dht_.getId().toString().c_str(), userName.c_str());
        return MatchRank::NONE;
    }
}

std::string RingAccount::getFromUri() const
{
    return "<sip:" + dht_.getId().toString() + "@ring.dht>";
}

std::string RingAccount::getToUri(const std::string& to) const
{
    const std::string transport {pjsip_transport_get_type_name(transportType_)};
    return "<sip:" + to + ">";
    //return "<sips:" + to + ";transport=" + transport + ">";
}

pj_str_t
RingAccount::getContactHeader(pjsip_transport* t)
{
    if (!t && transport_)
        t = transport_->get();
    if (!t) {
        RING_ERR("Transport not created yet");
        pj_cstr(&contact_, "<sip:>");
        return contact_;
    }

    auto ice = reinterpret_cast<SipIceTransport*>(t);

    // The transport type must be specified, in our case START_OTHER refers to stun transport
    /*pjsip_transport_type_e transportType = transportType_;

    if (transportType == PJSIP_TRANSPORT_START_OTHER)
        transportType = PJSIP_TRANSPORT_UDP;*/

    // Else we determine this infor based on transport information
    //std::string address = "ring.dht";
    //pj_uint16_t port = getTlsListenerPort();

    //link_->sipTransport->findLocalAddressFromTransport(t, transportType, hostname_, address, port);
    auto address = ice->getLocalAddress();
    /*if (addr) {
        address = addr;
        port =
    }*/
    /*auto ports = ice->getLocalPorts();
    if (not ports.empty())
        port = ports[0];*/
/*
#if HAVE_IPV6
    // Enclose IPv6 address in square brackets
    if (IpAddr::isIpv6(address)) {
        address = IpAddr(address);//.toString(false, true);
    }
#endif
*/
    RING_WARN("getContactHeader %s@%s", username_.c_str(), address.toString(true).c_str());
    contact_.slen = pj_ansi_snprintf(contact_.ptr, PJSIP_MAX_URL_SIZE,
                                     "<sip:%s%s%s>",
                                     username_.c_str(),
                                     (username_.empty() ? "" : "@"),
                                     address.toString(true).c_str());    /*
    contact_.slen = pj_ansi_snprintf(contact_.ptr, PJSIP_MAX_URL_SIZE,
                                     "<sips:%s%s%s:%d;transport=%s>",
                                     username_.c_str(),
                                     (username_.empty() ? "" : "@"),
                                     address.c_str(),
                                     port,
                                     pjsip_transport_get_type_name(transportType));*/
    return contact_;
}

/**
 *  Enable the presence module
 */
void
RingAccount::enablePresence(const bool& /* enabled */)
{
}

/**
 *  Set the presence (PUBLISH/SUBSCRIBE) support flags
 *  and process the change.
 */
void
RingAccount::supportPresence(int /* function */, bool /* enabled*/)
{
}

/*
void RingAccount::updateDialogViaSentBy(pjsip_dialog *dlg)
{
    if (allowViaRewrite_ && via_addr_.host.slen > 0)
        pjsip_dlg_set_via_sent_by(dlg, &via_addr_, via_tp_);
}
*/
