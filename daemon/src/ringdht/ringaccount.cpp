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

#include "sips_transport_ice.h"
#include "ice_transport.h"

#include "client/signal.h"

#include "upnp/upnp_control.h"
#include "system_codec_container.h"

#include "account_schema.h"
#include "logger.h"
#include "manager.h"

#ifdef RING_VIDEO
#include "libav_utils.h"
#endif
#include "fileutils.h"
#include "string_utils.h"
#include "array_size.h"

#include "config/yamlparser.h"

#include <opendht/securedht.h>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <array>
#include <memory>
#include <sstream>
#include <cctype>

namespace ring {

static constexpr int ICE_COMPONENTS {1};
static constexpr int ICE_COMP_SIP_TRANSPORT {0};
static constexpr int ICE_INIT_TIMEOUT {5};
static constexpr int ICE_NEGOTIATION_TIMEOUT {60};

constexpr const char * const RingAccount::ACCOUNT_TYPE;

RingAccount::RingAccount(const std::string& accountID, bool /* presenceEnabled */)
    : SIPAccountBase(accountID), via_addr_()
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
        auto call = call_it->call.lock();
        if (not call) {
            RING_WARN("newIncomingCall: discarding deleted call");
            call_it = pendingSipCalls_.erase(call_it);
        } else if (call->getPeerNumber() == from) {
            pendingSipCalls_.erase(call_it);
            RING_DBG("newIncomingCall: found matching call for %s", from.c_str());
            return call;
        } else {
            ++call_it;
        }
    }
    RING_ERR("newIncomingCall: can't find matching call for %s", from.c_str());
    return nullptr;
}

template <>
std::shared_ptr<SIPCall>
RingAccount::newOutgoingCall(const std::string& toUrl)
{
    auto& manager = Manager::instance();
    auto dhtf = toUrl.find("ring:");
    if (dhtf != std::string::npos) {
        dhtf = dhtf+5;
    } else {
        dhtf = toUrl.find("sips:");
        dhtf = (dhtf == std::string::npos) ? 0 : dhtf+5;
    }
    if (toUrl.length() - dhtf < 40)
        throw std::invalid_argument("id must be a ring infohash");
    const std::string toUri = toUrl.substr(dhtf, 40);
    if (std::find_if_not(toUri.cbegin(), toUri.cend(), ::isxdigit) != toUri.cend())
        throw std::invalid_argument("id must be a ring infohash");

    RING_DBG("Calling DHT peer %s", toUri.c_str());
    auto toH = dht::InfoHash(toUri);

    auto call = manager.callFactory.newCall<SIPCall, RingAccount>(*this, manager.getNewCallID(),
                                                                  Call::OUTGOING);
    call->setIPToIP(true);
    call->setSecure(isTlsEnabled());

    auto& iceTransportFactory = manager.getIceTransportFactory();
    auto ice = iceTransportFactory.createTransport(
        ("sip:"+call->getCallId()).c_str(),
        ICE_COMPONENTS,
        true,
        getUPnPActive()
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

    std::weak_ptr<SIPCall> weak_call = call;

    dht_.putEncrypted(
        callkey,
        toH,
        dht::Value {
            dht::DhtMessage::TYPE,
            dht::DhtMessage(
                dht::DhtMessage::Service::ICE_CANDIDATES,
                ice->getLocalAttributesAndCandidates()
            ),
            callvid
        },
        [callkey, callvid, weak_call, shared](bool ok) {
            auto& this_ = *std::static_pointer_cast<RingAccount>(shared).get();
            if (!ok) {
                RING_WARN("Can't put ICE descriptor on DHT");
                if (auto call = weak_call.lock()) {
                    call->setConnectionState(Call::DISCONNECTED);
                    Manager::instance().callFailure(*call);
                    call->removeCall();
                }
            }
            this_.dht_.cancelPut(callkey, callvid);
        }
    );

    auto lk = dht_.listen(
        callkey,
        [shared, ice, toH, replyvid] (const std::vector<std::shared_ptr<dht::Value>>& vals) {
            auto& this_ = *std::static_pointer_cast<RingAccount>(shared).get();
            RING_DBG("Outcall listen callback (%d values)", vals.size());
            for (const auto& v : vals) {
                if (v->recipient != this_.dht_.getId()) {
                    RING_DBG("Ignoring non encrypted or bad type value %s.", v->toString().c_str());
                    continue;
                }
                if (v->id != replyvid)
                    continue;
                dht::DhtMessage msg {v->data};
                RING_WARN("Got a DHT reply from %s !", toH.toString().c_str());
                RING_WARN("Performing ICE negotiation.");
                ice->start(msg.getMessage());
                return false;
            }
            return true;
        },
        dht::DhtMessage::ServiceFilter(dht::DhtMessage::Service::ICE_CANDIDATES)
    );
    {
        std::lock_guard<std::mutex> lock(callsMutex_);
        pendingCalls_.emplace_back(PendingCall{std::chrono::steady_clock::now(), ice, weak_call, std::move(lk), callkey, toH});
    }
    return call;
}

void
RingAccount::createOutgoingCall(const std::shared_ptr<SIPCall>& call, const std::string& to_id, IpAddr target)
{
    RING_WARN("RingAccount::createOutgoingCall to: %s target: %s",
              to_id.c_str(), target.toString(true).c_str());
    call->initIceTransport(true);
    call->setIPToIP(true);
    call->setPeerNumber(getToUri(to_id+"@"+target.toString(true).c_str()));
    call->initRecFilename(to_id);

    const auto localAddress = ip_utils::getInterfaceAddr(getLocalInterface());
    call->setCallMediaLocal(call->getIceTransport()->getDefaultLocalAddress());

    IpAddr addrSdp;
    if (getUPnPActive()) {
        /* use UPnP addr, or published addr if its set */
        addrSdp = getPublishedSameasLocal() ?
            getUPnPIpAddress() : getPublishedIpAddress();
    } else {
        addrSdp = isStunEnabled() or (not getPublishedSameasLocal()) ?
            getPublishedIpAddress() : localAddress;
    }

    /* fallback on local address */
    if (not addrSdp) addrSdp = localAddress;

    // Initialize the session using ULAW as default codec in case of early media
    // The session should be ready to receive media once the first INVITE is sent, before
    // the session initialization is completed
    if (!getSystemCodecContainer()->searchCodecByName("PCMA", ring::MEDIA_AUDIO))
        throw VoipLinkException("Could not instantiate codec for early media");

    // Building the local SDP offer
    auto& sdp = call->getSDP();

    sdp.setPublishedIP(addrSdp);
    const bool created = sdp.createOffer(
                            getActiveAccountCodecInfoIdList(MEDIA_AUDIO),
                            getActiveAccountCodecInfoIdList(MEDIA_VIDEO),
                            getSrtpKeyExchange()
                         );

    if (not created or not SIPStartCall(call, target))
        throw VoipLinkException("Could not send outgoing INVITE request for new call");
}

std::shared_ptr<Call>
RingAccount::newOutgoingCall(const std::string& toUrl)
{
    return newOutgoingCall<SIPCall>(toUrl);
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
    out << YAML::BeginMap;
    SIPAccountBase::serialize(out);
    out << YAML::Key << Conf::DHT_PORT_KEY << YAML::Value << dhtPort_;

    // tls submap
    out << YAML::Key << Conf::TLS_KEY << YAML::Value << YAML::BeginMap;
    SIPAccountBase::serializeTls(out);
    out << YAML::EndMap;

    out << YAML::EndMap;
}

void RingAccount::unserialize(const YAML::Node &node)
{
    using yaml_utils::parseValue;

    SIPAccountBase::unserialize(node);
    in_port_t port {DHT_DEFAULT_PORT};
    parseValue(node, Conf::DHT_PORT_KEY, port);
    dhtPort_ = port ? port : DHT_DEFAULT_PORT;
    dhtPortUsed_ = dhtPort_;
    checkIdentityPath();
}

void
RingAccount::checkIdentityPath()
{
    if (not tlsPrivateKeyFile_.empty() and not tlsCertificateFile_.empty())
        return;

    const auto idPath = fileutils::get_data_dir()+DIR_SEPARATOR_STR+getAccountID();
    tlsPrivateKeyFile_ = idPath + DIR_SEPARATOR_STR "dht.key";
    tlsCertificateFile_ = idPath + DIR_SEPARATOR_STR "dht.crt";
    tlsCaListFile_ = idPath + DIR_SEPARATOR_STR "ca.crt";
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
        ca_cert = dht::crypto::Certificate(fileutils::loadFile(tlsCaListFile_));
        dht_cert = dht::crypto::Certificate(fileutils::loadFile(tlsCertificateFile_));
        dht_key = dht::crypto::PrivateKey(fileutils::loadFile(tlsPrivateKeyFile_));
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
        tlsCaListFile_ = idPath_ + DIR_SEPARATOR_STR "ca.crt";

        saveIdentity(id, idPath_ + DIR_SEPARATOR_STR "dht");
        tlsCertificateFile_ = idPath_ + DIR_SEPARATOR_STR "dht.crt";
        tlsPrivateKeyFile_ = idPath_ + DIR_SEPARATOR_STR "dht.key";

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
    parseInt(details, Conf::CONFIG_DHT_PORT, dhtPort_);
    if (dhtPort_ == 0)
        dhtPort_ = DHT_DEFAULT_PORT;
    dhtPortUsed_ = dhtPort_;
    checkIdentityPath();
}

std::map<std::string, std::string> RingAccount::getAccountDetails() const
{
    std::map<std::string, std::string> a = SIPAccountBase::getAccountDetails();
    a[Conf::CONFIG_DHT_PORT] = ring::to_string(dhtPort_);
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
        auto call = c->call.lock();
        if (not call) {
            RING_WARN("Removing deleted call from pending calls");
            if (c->call_key != dht::InfoHash())
                dht_.cancelListen(c->call_key, c->listen_key.get());
            c = pendingCalls_.erase(c);
            continue;
        }
        auto ice = c->ice.get();
        if (ice->isRunning()) {
            regenerateCAList();
            auto id = loadIdentity();
            auto remote_h = c->id;
            tls::TlsParams tlsParams {
                .ca_list = caListPath_,
                .id = id.second,
                .dh_params = dhParams_,
                .timeout = std::chrono::seconds(30),
                .cert_check = [remote_h](unsigned status,
                                         const gnutls_datum_t* cert_list,
                                         unsigned cert_num) -> pj_status_t {
                    RING_WARN("TLS certificate check for %s",
                              remote_h.toString().c_str());

                    if (status & GNUTLS_CERT_EXPIRED ||
                        status & GNUTLS_CERT_NOT_ACTIVATED)
                        return PJ_SSL_CERT_EVALIDITY_PERIOD;
                    else if (status & GNUTLS_CERT_INSECURE_ALGORITHM)
                        return PJ_SSL_CERT_EUNTRUSTED;

                    if (cert_num == 0)
                        return PJ_SSL_CERT_EUNKNOWN;

                    try {
                        std::vector<uint8_t> crt_blob(cert_list[0].data,
                                                      cert_list[0].data + cert_list[0].size);
                        dht::crypto::Certificate crt(crt_blob);
                        const auto tls_id = crt.getId();
                        if (crt.getUID() != tls_id.toString()) {
                            RING_WARN("Certificate UID must be the public key ID");
                            return PJ_SSL_CERT_EUNTRUSTED;
                        }

                        if (tls_id != remote_h) {
                            RING_WARN("Certificate public key (ID %s) doesn't match expectation (%s)",
                                      tls_id.toString().c_str(),
                                      remote_h.toString().c_str());
                            return PJ_SSL_CERT_EUNTRUSTED;
                        }
                    } catch (const std::exception& e) {
                        return PJ_SSL_CERT_EUNKNOWN;
                    }
                    return PJ_SUCCESS;
                }
            };
            auto tr = link_->sipTransportBroker->getTlsIceTransport(c->ice, ICE_COMP_SIP_TRANSPORT, tlsParams);
            call->setTransport(tr);
            call->setConnectionState(Call::PROGRESSING);
            if (c->call_key == dht::InfoHash()) {
                RING_WARN("ICE succeeded : moving incomming call to pending sip call");
                auto in = c;
                ++c;
                pendingSipCalls_.splice(pendingSipCalls_.begin(), pendingCalls_, in, c);
            } else {
                RING_WARN("ICE succeeded : removing pending outgoing call");
                createOutgoingCall(call, c->id.toString(), ice->getRemoteAddress(ICE_COMP_SIP_TRANSPORT));
                dht_.cancelListen(c->call_key, c->listen_key.get());
                c = pendingCalls_.erase(c);
            }
        } else if (ice->isFailed() || now - c->start > std::chrono::seconds(ICE_NEGOTIATION_TIMEOUT)) {
            RING_WARN("ICE timeout : removing pending call");
            if (c->call_key != dht::InfoHash())
                dht_.cancelListen(c->call_key, c->listen_key.get());
            call->setConnectionState(Call::DISCONNECTED);
            Manager::instance().callFailure(*call);
            c = pendingCalls_.erase(c);
            call->removeCall();
        } else
            ++c;
    }
}

bool RingAccount::mapPortUPnP()
{
    if (getUPnPActive()) {
        /* create port mapping from published port to local port to the local IP
         * note that since different RING accounts can use the same port,
         * it may already be open, thats OK
         *
         * if the desired port is taken by another client, then it will try to map
         * a different port, if succesfull, then we have to use that port for DHT
         */
        uint16_t port_used;
        std::lock_guard<std::mutex> lock(upnp_mtx);
        if (upnp_->addAnyMapping(dhtPort_, ring::upnp::PortType::UDP, false, &port_used)) {
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

    if (not dhParams_.valid()) {
        generateDhParams();
    }

    /* if UPnP is enabled, then wait for IGD to complete registration */
    if ( upnpEnabled_ ) {
        auto shared = shared_from_this();
        RING_DBG("UPnP: waiting for IGD to register RING account");
        setRegistrationState(RegistrationState::TRYING);
        std::thread{ [shared] {
            auto this_ = std::static_pointer_cast<RingAccount>(shared).get();
            if ( not this_->mapPortUPnP())
                RING_WARN("UPnP: Could not successfully map DHT port with UPnP, continuing with account registration anyways.");
            this_->doRegister_();
        }}.detach();
    } else
        doRegister_();

}
void RingAccount::doRegister_()
{
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
                break;
            case dht::Dht::Status::Disconnected:
            default:
                setRegistrationState(status == dht::Dht::Status::Disconnected ? RegistrationState::UNREGISTERED : RegistrationState::ERROR_GENERIC);
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
        auto shared = std::static_pointer_cast<RingAccount>(shared_from_this());
        auto listenKey = "callto:"+dht_.getId().toString();
        RING_WARN("Listening on %s : %s", listenKey.c_str(), dht::InfoHash::get(listenKey).toString().c_str());
        dht_.listen (
            listenKey,
            [shared,listenKey] (const std::vector<std::shared_ptr<dht::Value>>& vals) {
                auto& this_ = *shared.get();
                for (const auto& v : vals) {
                    std::shared_ptr<SIPCall> call;
                    try {
                        if (v->recipient != this_.dht_.getId()) {
                            RING_DBG("Ignoring non encrypted value %s.", v->toString().c_str());
                            continue;
                        }
                        auto remote_id = v->owner.getId();
                        if (remote_id == this_.dht_.getId())
                            continue;
                        dht::DhtMessage msg {v->data};
                        auto res = this_.treatedCalls_.insert(v->id);
                        this_.saveTreatedCalls();
                        if (!res.second)
                            continue;
                        auto from = remote_id.toString();
                        auto from_vid = v->id;
                        auto reply_vid = from_vid+1;
                        RING_WARN("Received incoming DHT call request from %s", from.c_str());
                        call = Manager::instance().callFactory.newCall<SIPCall, RingAccount>(this_, Manager::instance().getNewCallID(), Call::INCOMING);
                        auto& iceTransportFactory = Manager::instance().getIceTransportFactory();
                        auto ice = iceTransportFactory.createTransport(
                            ("sip:"+call->getCallId()).c_str(),
                            ICE_COMPONENTS,
                            false,
                            this_.getUPnPActive()
                        );
                        if (ice->waitForInitialization(ICE_INIT_TIMEOUT) <= 0)
                            throw std::runtime_error("Can't initialize ICE..");

                        std::weak_ptr<SIPCall> weak_call = call;

                        this_.dht_.putEncrypted(
                            listenKey,
                            remote_id,
                            dht::Value {
                                dht::DhtMessage::TYPE,
                                dht::DhtMessage(
                                    dht::DhtMessage::Service::ICE_CANDIDATES,
                                    ice->getLocalAttributesAndCandidates()
                                ),
                                reply_vid
                            },
                            [weak_call,ice,shared,listenKey,reply_vid](bool ok) {
                                auto& this_ = *std::static_pointer_cast<RingAccount>(shared).get();
                                if (!ok) {
                                    RING_WARN("Can't put ICE descriptor on DHT");
                                    if (auto call = weak_call.lock()) {
                                        call->setConnectionState(Call::DISCONNECTED);
                                        Manager::instance().callFailure(*call);
                                        call->removeCall();
                                    }
                                }
                                this_.dht_.cancelPut(listenKey, reply_vid);
                            }
                        );
                        ice->start(msg.getMessage());
                        call->setPeerNumber(from);
                        call->initRecFilename(from);
                        {
                            std::lock_guard<std::mutex> lock(this_.callsMutex_);
                            this_.pendingCalls_.emplace_back(PendingCall{std::chrono::steady_clock::now(), ice, weak_call, {}, {}, remote_id});
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
            },
            dht::DhtMessage::ServiceFilter(dht::DhtMessage::Service::ICE_CANDIDATES)
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

    /* RING_DBG("UPnP: removing port mapping for DHT account."); */
    upnp_->removeMappings();

    Manager::instance().unregisterEventHandler((uintptr_t)this);
    saveNodes(dht_.exportNodes());
    saveValues(dht_.exportValues());
    dht_.join();
    setRegistrationState(RegistrationState::UNREGISTERED);
    if (released_cb)
        released_cb(false);
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
        std::ifstream file(tlsCaListFile_, std::ios::binary);
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

void
RingAccount::initTlsConfiguration()
{
    regenerateCAList();

}

static std::unique_ptr<gnutls_dh_params_int, decltype(gnutls_dh_params_deinit)&>
getNewDhParams()
{
    using namespace std::chrono;
    auto bits = gnutls_sec_param_to_pk_bits(GNUTLS_PK_DH, GNUTLS_SEC_PARAM_HIGH/* GNUTLS_SEC_PARAM_NORMAL */);
    RING_DBG("Generating DH params with %u bits", bits);

    auto t1 = high_resolution_clock::now();
    gnutls_dh_params_t new_params_;
    gnutls_dh_params_init(&new_params_);
    gnutls_dh_params_generate2(new_params_, bits);
    auto time_span = duration_cast<duration<double>>(high_resolution_clock::now() - t1);

    RING_WARN("Generated DH params with %u bits in %lfs", bits, time_span.count());
    return {new_params_, gnutls_dh_params_deinit};
}

void
RingAccount::generateDhParams()
{
    std::packaged_task<decltype(getNewDhParams())()> task(&getNewDhParams);
    dhParams_ = task.get_future();
    std::thread task_td(std::move(task));
    task_td.detach();
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
    //return "<sip:" + to + ">";
    return "<sips:" + to + ";transport=" + transport + ">";
}

pj_str_t
RingAccount::getContactHeader(pjsip_transport* t)
{
    if (!t && transport_)
        t = transport_->get();
    if (!t) {
        RING_ERR("Transport not created yet");
        pj_cstr(&contact_, "<sips:>");
        return contact_;
    }

    // FIXME: be sure that given transport is from SipIceTransport
    auto tlsTr = reinterpret_cast<tls::SipsIceTransport::TransportData*>(t)->self;
    auto address = tlsTr->getLocalAddress();
    RING_WARN("getContactHeader %s@%s", username_.c_str(), address.toString(true).c_str());
    contact_.slen = pj_ansi_snprintf(contact_.ptr, PJSIP_MAX_URL_SIZE,
                                     "<sips:%s%s%s;transport=%s>",
                                     username_.c_str(),
                                     (username_.empty() ? "" : "@"),
                                     address.toString(true).c_str(),
                                     pjsip_transport_get_type_name(transportType_));
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

} // namespace ring
