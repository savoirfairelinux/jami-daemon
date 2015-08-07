/*
 *  Copyright (C) 2014-2015 Savoir-Faire Linux Inc.
 *
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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

#include "client/ring_signal.h"
#include "dring/call_const.h"
#include "dring/account_const.h"

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

#include "security/certstore.h"

#include <opendht/securedht.h>

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <array>
#include <memory>
#include <sstream>
#include <cctype>
#include <cstdarg>

namespace ring {

static constexpr int ICE_COMPONENTS {1};
static constexpr int ICE_COMP_SIP_TRANSPORT {0};
static constexpr int ICE_INIT_TIMEOUT {5};
static constexpr int ICE_NEGOTIATION_TIMEOUT {60};

constexpr const char * const RingAccount::ACCOUNT_TYPE;
/* constexpr */ const std::pair<uint16_t, uint16_t> RingAccount::DHT_PORT_RANGE {4000, 8888};

/**
 * Local ICE Transport factory helper
 *
 * RingAccount must use this helper than direct IceTranportFactory.
 */
template <class... Args>
std::shared_ptr<IceTransport>
RingAccount::createIceTransport(Args... args)
{
    // We need a public address in case of NAT'ed network
    // Trying to use one discovered by DHT service
    if (getPublishedAddress().empty()) {
        const auto& addresses = dht_.getPublicAddress();
        if (addresses.size())
            setPublishedAddress(IpAddr{addresses[0].first});
    }

    return Manager::instance().getIceTransportFactory().createTransport(args...);
}

RingAccount::RingAccount(const std::string& accountID, bool /* presenceEnabled */)
    : SIPAccountBase(accountID), via_addr_()
{
    cachePath_ = fileutils::get_cache_dir()+DIR_SEPARATOR_STR+getAccountID();
    dataPath_ = cachePath_ + DIR_SEPARATOR_STR "values";
    idPath_ = fileutils::get_data_dir()+DIR_SEPARATOR_STR+getAccountID();
}

RingAccount::~RingAccount()
{
    Manager::instance().unregisterEventHandler((uintptr_t)this);
    dht_.join();
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

    auto& manager = Manager::instance();
    auto call = manager.callFactory.newCall<SIPCall, RingAccount>(*this, manager.getNewCallID(),
                                                                  Call::CallType::OUTGOING);
    call->setIPToIP(true);
    call->setSecure(isTlsEnabled());

    // Create an ICE transport for SIP channel
    auto ice = createIceTransport(("sip:" + call->getCallId()).c_str(),
                                  ICE_COMPONENTS, true, getIceOptions());
    if (not ice) {
        call->removeCall();
        return nullptr;
    }

    auto shared_this = std::static_pointer_cast<RingAccount>(shared_from_this());
    auto iceInitTimeout = std::chrono::steady_clock::now() + std::chrono::seconds {ICE_INIT_TIMEOUT};

    // TODO: for now, we automatically trust all explicitly called peers
    setCertificateStatus(toUri, tls::TrustStore::Status::ALLOWED);

    std::weak_ptr<SIPCall> weak_call = call;
    manager.addTask([=] {
        static std::uniform_int_distribution<dht::Value::Id> udist;
        auto call = weak_call.lock();

        if (not call)
            return false;

        /* First step: wait for an initialized ICE transport for SIP channel */
        if (ice->isFailed() or std::chrono::steady_clock::now() >= iceInitTimeout) {
            RING_DBG("ice init failed (or timeout)");
            call->onFailure();
            return false;
        }

        if (not ice->isInitialized())
            return true;

        /* Next step: sent the ICE data to peer through DHT */
        const dht::Value::Id callvid  = udist(shared_this->rand_);
        const dht::Value::Id replyvid = callvid + 1;
        const auto toH = dht::InfoHash(toUri);
        const auto callkey = dht::InfoHash::get("callto:" + toUri);

        call->setState(Call::ConnectionState::TRYING);
        shared_this->dht_.putEncrypted(
            callkey, toH,
            dht::Value {
                dht::IceCandidates(ice->getLocalAttributesAndCandidates()),
                callvid
            },
            [=](bool ok) { // Put complete callback
                if (!ok) {
                    RING_WARN("Can't put ICE descriptor on DHT");
                    if (auto call = weak_call.lock())
                        call->onFailure();
                } else
                    RING_DBG("Succesfully put ICE descriptor on DHT");
                shared_this->dht_.cancelPut(callkey, callvid);
            }
        );

        auto listenKey = shared_this->dht_.listen<dht::IceCandidates>(
            callkey,
            [=] (dht::IceCandidates&& msg) {
                if (msg.id != replyvid)
                    return true;
                RING_WARN("ICE request replied from DHT peer %s\n%s", toH.toString().c_str(),
                          std::string(msg.ice_data.cbegin(), msg.ice_data.cend()).c_str());
                if (auto call = weak_call.lock())
                    call->setState(Call::ConnectionState::PROGRESSING);
                ice->start(msg.ice_data);
                return false;
            }
        );

        shared_this->pendingCalls_.emplace_back(PendingCall{
            std::chrono::steady_clock::now(),
            ice, weak_call,
            std::move(listenKey),
            callkey, toH
        });
        return false;
    });

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
                            getActiveAccountCodecInfoList(MEDIA_AUDIO),
                            getActiveAccountCodecInfoList(videoEnabled_ ? MEDIA_VIDEO : MEDIA_NONE),
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

    pjsip_dlg_inc_lock(inv->dlg);
    inv->mod_data[link_->getModId()] = call.get();
    call->inv.reset(inv);

/*
    updateDialogViaSentBy(dialog);
    if (hasServiceRoute())
        pjsip_dlg_set_route_set(dialog, sip_utils::createRouteSet(getServiceRoute(), call->inv->pool));
*/

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
        RING_ERR("Unable to send invite message for this call");
        return false;
    }

    call->setState(Call::CallState::ACTIVE, Call::ConnectionState::PROGRESSING);

    return true;
}

void RingAccount::serialize(YAML::Emitter &out)
{
    out << YAML::BeginMap;
    SIPAccountBase::serialize(out);
    out << YAML::Key << Conf::DHT_PORT_KEY << YAML::Value << dhtPort_;
    out << YAML::Key << Conf::DHT_PUBLIC_IN_CALLS << YAML::Value << dhtPublicInCalls_;
    out << YAML::Key << Conf::DHT_ALLOW_PEERS_FROM_HISTORY << YAML::Value << allowPeersFromHistory_;
    out << YAML::Key << Conf::DHT_ALLOW_PEERS_FROM_CONTACT << YAML::Value << allowPeersFromContact_;
    out << YAML::Key << Conf::DHT_ALLOW_PEERS_FROM_TRUSTED << YAML::Value << allowPeersFromTrusted_;

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
    parseValue(node, Conf::DHT_PORT_KEY, dhtPort_);
    parseValue(node, Conf::DHT_ALLOW_PEERS_FROM_HISTORY, allowPeersFromHistory_);
    parseValue(node, Conf::DHT_ALLOW_PEERS_FROM_CONTACT, allowPeersFromContact_);
    parseValue(node, Conf::DHT_ALLOW_PEERS_FROM_TRUSTED, allowPeersFromTrusted_);
    if (not dhtPort_)
        dhtPort_ = getRandomEvenPort(DHT_PORT_RANGE);
    dhtPortUsed_ = dhtPort_;

    parseValue(node, Conf::DHT_PUBLIC_IN_CALLS, dhtPublicInCalls_);

    checkIdentityPath();
}

void
RingAccount::checkIdentityPath()
{
    if (not tlsPrivateKeyFile_.empty() and not tlsCertificateFile_.empty()) {
        loadIdentity();
        return;
    }

    const auto idPath = fileutils::get_data_dir()+DIR_SEPARATOR_STR+getAccountID();
    tlsPrivateKeyFile_ = idPath + DIR_SEPARATOR_STR "dht.key";
    tlsCertificateFile_ = idPath + DIR_SEPARATOR_STR "dht.crt";
    loadIdentity();
}

dht::crypto::Identity
RingAccount::loadIdentity()
{
    dht::crypto::Certificate dht_cert;
    dht::crypto::PrivateKey dht_key;

    try {
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
        fileutils::check_dir(idPath_.c_str(), 0700);

        fileutils::saveFile(idPath_ + DIR_SEPARATOR_STR "ca.key", ca.first->serialize(), 0600);

        // save the chain including CA
        saveIdentity(id, idPath_ + DIR_SEPARATOR_STR "dht");
        tlsCertificateFile_ = idPath_ + DIR_SEPARATOR_STR "dht.crt";
        tlsPrivateKeyFile_ = idPath_ + DIR_SEPARATOR_STR "dht.key";

        username_ = id.second->getId().toString();
        return id;
    }

    username_ = dht_cert.getId().toString();
    return {
        std::make_shared<dht::crypto::PrivateKey>(std::move(dht_key)),
        std::make_shared<dht::crypto::Certificate>(std::move(dht_cert))
    };
}

void
RingAccount::saveIdentity(const dht::crypto::Identity id, const std::string& path) const
{
    if (id.first)
        fileutils::saveFile(path + ".key", id.first->serialize(), 0600);
    if (id.second)
        fileutils::saveFile(path + ".crt", id.second->getPacked(), 0600);
}

void RingAccount::setAccountDetails(const std::map<std::string, std::string> &details)
{
    SIPAccountBase::setAccountDetails(details);
    if (hostname_.empty())
        hostname_ = DHT_DEFAULT_BOOTSTRAP;
    parseInt(details, Conf::CONFIG_DHT_PORT, dhtPort_);
    parseBool(details, Conf::CONFIG_DHT_PUBLIC_IN_CALLS, dhtPublicInCalls_);
    parseBool(details, DRing::Account::ConfProperties::ALLOW_CERT_FROM_HISTORY, allowPeersFromHistory_);
    parseBool(details, DRing::Account::ConfProperties::ALLOW_CERT_FROM_CONTACT, allowPeersFromContact_);
    parseBool(details, DRing::Account::ConfProperties::ALLOW_CERT_FROM_TRUSTED, allowPeersFromTrusted_);
    if (not dhtPort_)
        dhtPort_ = getRandomEvenPort(DHT_PORT_RANGE);
    dhtPortUsed_ = dhtPort_;
    checkIdentityPath();
}

std::map<std::string, std::string> RingAccount::getAccountDetails() const
{
    std::map<std::string, std::string> a = SIPAccountBase::getAccountDetails();
    a.emplace(Conf::CONFIG_DHT_PORT, ring::to_string(dhtPort_));
    a.emplace(Conf::CONFIG_DHT_PUBLIC_IN_CALLS, dhtPublicInCalls_ ? TRUE_STR : FALSE_STR);

    /* these settings cannot be changed (read only), but clients should still be
     * able to read what they are */
    a.emplace(Conf::CONFIG_SRTP_KEY_EXCHANGE, sip_utils::getKeyExchangeName(getSrtpKeyExchange()));
    a.emplace(Conf::CONFIG_SRTP_ENABLE,       isSrtpEnabled() ? TRUE_STR : FALSE_STR);
    a.emplace(Conf::CONFIG_SRTP_RTP_FALLBACK, getSrtpFallback() ? TRUE_STR : FALSE_STR);

    a.emplace(Conf::CONFIG_TLS_METHOD,                     "Automatic");
    a.emplace(Conf::CONFIG_TLS_CIPHERS,                    "");
    a.emplace(Conf::CONFIG_TLS_SERVER_NAME,                "");
    a.emplace(Conf::CONFIG_TLS_VERIFY_SERVER,              TRUE_STR);
    a.emplace(Conf::CONFIG_TLS_VERIFY_CLIENT,              TRUE_STR);
    a.emplace(Conf::CONFIG_TLS_REQUIRE_CLIENT_CERTIFICATE, TRUE_STR);
    a.emplace(DRing::Account::ConfProperties::ALLOW_CERT_FROM_HISTORY, allowPeersFromHistory_?TRUE_STR:FALSE_STR);
    a.emplace(DRing::Account::ConfProperties::ALLOW_CERT_FROM_CONTACT, allowPeersFromContact_?TRUE_STR:FALSE_STR);
    a.emplace(DRing::Account::ConfProperties::ALLOW_CERT_FROM_TRUSTED, allowPeersFromTrusted_?TRUE_STR:FALSE_STR);
    /* GNUTLS_DEFAULT_HANDSHAKE_TIMEOUT is defined as -1 */
    a.emplace(Conf::CONFIG_TLS_NEGOTIATION_TIMEOUT_SEC,    "-1");

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
            RING_DBG("Removing deleted call from pending calls");
            if (c->call_key != dht::InfoHash())
                dht_.cancelListen(c->call_key, c->listen_key.get());
            c = pendingCalls_.erase(c);
            continue;
        }
        auto ice = c->ice.get();
        if (ice->isRunning()) {
            auto id = loadIdentity();
            auto remote_h = c->from;
            tls::TlsParams tlsParams {
                .ca_list = "",
                .id = id,
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
                            RING_ERR("Certificate UID must be the public key ID");
                            return PJ_SSL_CERT_EUNTRUSTED;
                        }

                        if (tls_id != remote_h) {
                            RING_ERR("Certificate public key (ID %s) doesn't match expectation (%s)",
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
            call->setState(Call::ConnectionState::PROGRESSING);
            if (c->call_key == dht::InfoHash()) {
                RING_DBG("[call:%s] ICE succeeded : moving incoming call to pending sip call",
                         call->getCallId().c_str());
                auto in = c;
                ++c;
                pendingSipCalls_.splice(pendingSipCalls_.begin(), pendingCalls_, in, c);
            } else {
                RING_DBG("[call:%s] ICE succeeded : removing pending outgoing call",
                         call->getCallId().c_str());
                createOutgoingCall(call, remote_h.toString(), ice->getRemoteAddress(ICE_COMP_SIP_TRANSPORT));
                dht_.cancelListen(c->call_key, c->listen_key.get());
                c = pendingCalls_.erase(c);
            }
        } else if (ice->isFailed() || now - c->start > std::chrono::seconds(ICE_NEGOTIATION_TIMEOUT)) {
            RING_WARN("[call:%s] ICE timeout : removing pending call (%d)",
                      call->getCallId().c_str(), call.use_count());
            if (c->call_key != dht::InfoHash())
                dht_.cancelListen(c->call_key, c->listen_key.get());
            call->onFailure();
            c = pendingCalls_.erase(c);
        } else
            ++c;
    }
}

bool RingAccount::mapPortUPnP()
{
    // return true if not using UPnP
    bool added = true;

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
        added = upnp_->addAnyMapping(dhtPort_, ring::upnp::PortType::UDP, false, &port_used);
        if (added) {
            if (port_used != dhtPort_)
                RING_DBG("UPnP could not map port %u for DHT, using %u instead", dhtPort_, port_used);
            dhtPortUsed_ = port_used;
        }
    }

    std::weak_ptr<RingAccount> w = std::static_pointer_cast<RingAccount>(shared_from_this());
    upnp_->setIGDListener([w] {
        if (auto shared = w.lock())
            shared->connectivityChanged();
    });
    return added;
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

static constexpr const char*
dhtStatusStr(dht::Dht::Status status) {
    return status == dht::Dht::Status::Connected  ? "connected"  : (
           status == dht::Dht::Status::Connecting ? "connecting" :
                                                    "disconnected");
}

void
RingAccount::doRegister_()
{
    try {
        loadTreatedCalls();
        if (dht_.isRunning()) {
            RING_ERR("DHT already running (stopping it first).");
            dht_.join();
        }
        auto identity = loadIdentity();

        dht_.setOnStatusChanged([this](dht::Dht::Status s4, dht::Dht::Status s6) {
                RING_WARN("Dht status : IPv4 %s; IPv6 %s", dhtStatusStr(s4), dhtStatusStr(s6));
                RegistrationState state;
                switch (std::max(s4, s6)) {
                    case dht::Dht::Status::Connecting:
                        state = RegistrationState::TRYING;
                        break;
                    case dht::Dht::Status::Connected:
                        state = RegistrationState::REGISTERED;
                        break;
                    case dht::Dht::Status::Disconnected:
                        state = RegistrationState::UNREGISTERED;
                        break;
                    default:
                        state = RegistrationState::ERROR_GENERIC;
                        break;
                }
                setRegistrationState(state);
            });

        dht_.run((in_port_t)dhtPortUsed_, identity, false);

        dht_.setLocalCertificateStore([](const dht::InfoHash& pk_id) {
            auto& store = tls::CertificateStore::instance();
            auto cert = store.getCertificate(pk_id.toString());
            std::vector<std::shared_ptr<dht::crypto::Certificate>> ret;
            if (cert)
                ret.emplace_back(std::move(cert));
            RING_DBG("Query for local certificate store: %s: %d found.", pk_id.toString().c_str(), ret.size());
            return ret;
        });

#if 0 // enable if dht_ logging is needed
        dht_.setLoggers(
            [](char const* m, va_list args){ vlogger(LOG_ERR, m, args); },
            [](char const* m, va_list args){ vlogger(LOG_WARNING, m, args); },
            [](char const* m, va_list args){ vlogger(LOG_DEBUG, m, args); }
        );
#endif

        dht_.importValues(loadValues());

        username_ = dht_.getId().toString();

        Manager::instance().registerEventHandler((uintptr_t)this, [this]{ handleEvents(); });
        setRegistrationState(RegistrationState::TRYING);

        dht_.bootstrap(loadNodes());
        if (!hostname_.empty()) {
            std::stringstream ss(hostname_);
            std::vector<std::pair<sockaddr_storage, socklen_t>> bootstrap;
            std::string node_addr;
            while (std::getline(ss, node_addr, ';')) {
                auto ips = ip_utils::getAddrList(node_addr);
                if (ips.empty()) {
                    IpAddr resolved(node_addr);
                    if (resolved) {
                        if (resolved.getPort() == 0)
                            resolved.setPort(DHT_DEFAULT_PORT);
                        bootstrap.emplace_back(resolved, resolved.getLength());
                    }
                } else {
                    for (auto& ip : ips) {
                        if (ip.getPort() == 0)
                            ip.setPort(DHT_DEFAULT_PORT);
                        bootstrap.emplace_back(ip, ip.getLength());
                    }
                }
            }
            for (auto ip : bootstrap)
                RING_DBG("Bootstrap node: %s", IpAddr(ip.first).toString(true).c_str());
            dht_.bootstrap(bootstrap);
        }

        // Listen for incoming calls
        auto shared = std::static_pointer_cast<RingAccount>(shared_from_this());
        callKey_ = dht::InfoHash::get("callto:"+dht_.getId().toString());
        RING_DBG("Listening on callto:%s : %s", dht_.getId().toString().c_str(), callKey_.toString().c_str());
        dht_.listen<dht::IceCandidates>(
            callKey_,
            [shared] (dht::IceCandidates&& msg) {
                // callback for incoming call
                auto& this_ = *shared;
                if (msg.from == this_.dht_.getId())
                    return true;

                // quick check in case we already explicilty banned this public key
                auto trustStatus = this_.trust_.getCertificateStatus(msg.from.toString());
                if (trustStatus == tls::TrustStore::Status::BANNED) {
                    RING_WARN("Discarding incoming DHT call request from banned peer %s", msg.from.toString().c_str());
                    return true;
                }

                auto res = this_.treatedCalls_.insert(msg.id);
                this_.saveTreatedCalls();
                if (!res.second)
                    return true;

                if (not this_.dhtPublicInCalls_ and trustStatus != tls::TrustStore::Status::ALLOWED) {
                    this_.findCertificate(
                        msg.from,
                        [shared, msg](const std::shared_ptr<dht::crypto::Certificate> cert) mutable {
                            if (!cert) {
                                RING_WARN("Can't find certificate of %s for incoming call.",
                                          msg.from.toString().c_str());
                                return;
                            }

                            tls::CertificateStore::instance().pinCertificate(cert);

                            auto& this_ = *shared;
                            if (!this_.trust_.isTrusted(*cert) or cert->getId() != msg.from) {
                                RING_WARN("Discarding incoming DHT call from untrusted peer %s.",
                                          msg.from.toString().c_str());
                                return;
                            }

                            this_.pushIncomingCallTask(std::move(msg));
                        }
                    );
                    return true;
                }
                else if (this_.dhtPublicInCalls_ and trustStatus != tls::TrustStore::Status::BANNED) {
                    this_.findCertificate(msg.from.toString().c_str());
                }
                // public incoming calls allowed or we explicitly authorised this public key
                this_.pushIncomingCallTask(std::move(msg));
                return true;
            }
        );

        auto inboxKey = dht::InfoHash::get("inbox:"+dht_.getId().toString());
        dht_.listen<dht::TrustRequest>(
            inboxKey,
            [shared](dht::TrustRequest&& v) {
                auto& this_ = *shared.get();
                if (v.service != DHT_TYPE_NS)
                    return true;
                // if the invite exists, update it
                auto req = this_.trustRequests_.begin();
                for (;req != this_.trustRequests_.end(); ++req)
                    if (req->from == v.from) {
                        req->received = std::chrono::system_clock::now();
                        break;
                    }
                if (req == this_.trustRequests_.end()) {
                    this_.trustRequests_.emplace_back(TrustRequest{
                        .from = v.from,
                        .received = std::chrono::system_clock::now(),
                        .payload = v.payload
                    });
                    req = std::prev(this_.trustRequests_.end());
                }
                emitSignal<DRing::ConfigurationSignal::IncomingTrustRequest>(
                    this_.getAccountID(),
                    req->from.toString(),
                    req->payload,
                    std::chrono::system_clock::to_time_t(req->received)
                );
                return true;
            }
        );
    }
    catch (const std::exception& e) {
        RING_ERR("Error registering DHT account: %s", e.what());
        setRegistrationState(RegistrationState::ERROR_GENERIC);
    }
}

void
RingAccount::pushIncomingCallTask(dht::IceCandidates&& msg)
{
    auto acc = std::static_pointer_cast<RingAccount>(shared_from_this());
    runOnMainThread([acc, msg]() {acc->incomingCall(msg); });
}

void
RingAccount::incomingCall(const dht::IceCandidates& msg)
{
    auto from = msg.from.toString();
    auto reply_vid = msg.id+1;
    RING_WARN("ICE incoming from DHT peer %s\n%s", from.c_str(),
              std::string(msg.ice_data.cbegin(), msg.ice_data.cend()).c_str());
    auto call = Manager::instance().callFactory.newCall<SIPCall, RingAccount>(*this, Manager::instance().getNewCallID(), Call::CallType::INCOMING);
    auto ice = createIceTransport(("sip:"+call->getCallId()).c_str(), ICE_COMPONENTS, false, getIceOptions());
    if (ice->waitForInitialization(ICE_INIT_TIMEOUT) <= 0)
        throw std::runtime_error("Can't initialize ICE..");

    std::weak_ptr<SIPCall> weak_call = call;
    auto shared = std::static_pointer_cast<RingAccount>(shared_from_this());
    dht_.putEncrypted(
        callKey_,
        msg.from,
        dht::Value {
            dht::IceCandidates(ice->getLocalAttributesAndCandidates()),
            reply_vid
        },
        [weak_call,shared,reply_vid](bool ok) {
            auto& this_ = *shared.get();
            if (!ok) {
                RING_WARN("Can't put ICE descriptor reply on DHT");
                if (auto call = weak_call.lock())
                    call->onFailure();
            } else
                RING_DBG("Successfully put ICE descriptor reply on DHT");
            this_.dht_.cancelPut(this_.callKey_, reply_vid);
        }
    );
    ice->start(msg.ice_data);
    call->setPeerNumber(from);
    call->initRecFilename(from);
    {
        std::lock_guard<std::mutex> lock(callsMutex_);
        pendingCalls_.emplace_back(PendingCall{std::chrono::steady_clock::now(), ice, weak_call, {}, {}, msg.from});
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
    upnp_->setIGDListener();
    upnp_->removeMappings();

    Manager::instance().unregisterEventHandler((uintptr_t)this);
    saveNodes(dht_.exportNodes());
    saveValues(dht_.exportValues());
    dht_.join();
    setRegistrationState(RegistrationState::UNREGISTERED);
    if (released_cb)
        released_cb(false);
}

bool
RingAccount::findCertificate(const dht::InfoHash& h, std::function<void(const std::shared_ptr<dht::crypto::Certificate>)> cb)
{
    if (auto cert = tls::CertificateStore::instance().getCertificate(h.toString())) {
        if (cb)
            cb(cert);
    } else {
        dht_.findCertificate(h, [=](const std::shared_ptr<dht::crypto::Certificate> crt) {
            if (crt)
                tls::CertificateStore::instance().pinCertificate(std::move(crt));
            if (cb)
                cb(crt);
        });
    }
    return true;
}

bool
RingAccount::findCertificate(const std::string& crt_id)
{
    findCertificate(dht::InfoHash(crt_id));
    return true;
}

bool
RingAccount::setCertificateStatus(const std::string& cert_id, tls::TrustStore::Status status)
{
    findCertificate(cert_id);
    bool done = trust_.setCertificateStatus(cert_id, status);
    if (done)
        emitSignal<DRing::ConfigurationSignal::CertificateStateChanged>(getAccountID(), cert_id, tls::TrustStore::statusToStr(status));
    return done;
}

std::vector<std::string>
RingAccount::getCertificatesByStatus(tls::TrustStore::Status status)
{
    return trust_.getCertificatesByStatus(status);
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
            dht::Dht::NodeExport e {dht::InfoHash(id), ip, ip.getLength()};
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
        const auto file = dataPath_+DIR_SEPARATOR_STR+fname;
        try {
            std::ifstream ifs(file, std::ifstream::in | std::ifstream::binary);
            std::istreambuf_iterator<char> begin(ifs), end;
            values.emplace_back(dht::Dht::ValuesExport{dht::InfoHash(fname), std::vector<uint8_t>{begin, end}});
        } catch (const std::exception& e) {
            RING_ERR("Error reading value: %s", e.what());
        }
        remove(file.c_str());
    }
    RING_DBG("Loaded %lu values", values.size());
    return values;
}

static std::unique_ptr<gnutls_dh_params_int, decltype(gnutls_dh_params_deinit)&>
getNewDhParams()
{
    using namespace std::chrono;
    auto bits = gnutls_sec_param_to_pk_bits(GNUTLS_PK_DH, /* GNUTLS_SEC_PARAM_HIGH */ GNUTLS_SEC_PARAM_NORMAL);
    RING_DBG("Generating DH params with %u bits", bits);
    auto t1 = high_resolution_clock::now();

    gnutls_dh_params_t new_params_;
    int ret = gnutls_dh_params_init(&new_params_);
    if (ret != GNUTLS_E_SUCCESS) {
        RING_ERR("Error initializing DH params: %s", gnutls_strerror(ret));
        return {nullptr, gnutls_dh_params_deinit};
    }

    ret = gnutls_dh_params_generate2(new_params_, bits);
    if (ret != GNUTLS_E_SUCCESS) {
        RING_ERR("Error generating DH params: %s", gnutls_strerror(ret));
        return {nullptr, gnutls_dh_params_deinit};
    }

    auto time_span = duration_cast<duration<double>>(high_resolution_clock::now() - t1);
    RING_DBG("Generated DH params with %u bits in %lfs", bits, time_span.count());
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

MatchRank
RingAccount::matches(const std::string &userName, const std::string &server) const
{
    auto dhtId = dht_.getId().toString();
    if (userName == dhtId || server == dhtId) {
        RING_DBG("Matching account id in request with username %s", userName.c_str());
        return MatchRank::FULL;
    } else {
        return MatchRank::NONE;
    }
}

std::string RingAccount::getFromUri() const
{
    return "<sip:" + dht_.getId().toString() + "@ring.dht>";
}

std::string RingAccount::getToUri(const std::string& to) const
{
    return "<sips:" + to + ";transport=tls>";
}

pj_str_t
RingAccount::getContactHeader(pjsip_transport* t)
{
    if (!t) {
        RING_ERR("getContactHeader: no SIP transport provided");
        contact_.slen = pj_ansi_snprintf(contact_.ptr, PJSIP_MAX_URL_SIZE,
                                         "%s%s<sips:%s@ring.dht>",
                                         displayName_.c_str(),
                                         (displayName_.empty() ? "" : " "),
                                         username_.c_str());
        return contact_;
    }

    // FIXME: be sure that given transport is from SipIceTransport
    auto tlsTr = reinterpret_cast<tls::SipsIceTransport::TransportData*>(t)->self;
    auto address = tlsTr->getLocalAddress();
    contact_.slen = pj_ansi_snprintf(contact_.ptr, PJSIP_MAX_URL_SIZE,
                                     "%s%s<sips:%s%s%s;transport=tls>",
                                     displayName_.c_str(),
                                     (displayName_.empty() ? "" : " "),
                                     username_.c_str(),
                                     (username_.empty() ? "" : "@"),
                                     address.toString(true).c_str());
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

/* trust requests */
std::map<std::string, std::string>
RingAccount::getTrustRequests() const
{
    std::map<std::string, std::string> ret;
    for (const auto& r : trustRequests_)
        ret.emplace(r.from.toString(), ring::to_string(std::chrono::system_clock::to_time_t(r.received)));
    return ret;
}


bool
RingAccount::acceptTrustRequest(const std::string& from)
{
    dht::InfoHash f(from);
    for (auto i = std::begin(trustRequests_); i != std::end(trustRequests_); ++i) {
        if (i->from == f) {
            trust_.setCertificateStatus(from, tls::TrustStore::Status::ALLOWED);
            trustRequests_.erase(i);
            return true;
        }
    }
    return false;
}

bool
RingAccount::discardTrustRequest(const std::string& from)
{
    dht::InfoHash f(from);
    for (auto i = std::begin(trustRequests_); i != std::end(trustRequests_); ++i) {
        if (i->from == f) {
            trustRequests_.erase(i);
            return true;
        }
    }
    return false;
}

void
RingAccount::sendTrustRequest(const std::string& to, const std::vector<uint8_t>& payload)
{
    setCertificateStatus(to, tls::TrustStore::Status::ALLOWED);
    dht_.putEncrypted(dht::InfoHash::get("inbox:"+to),
                      dht::InfoHash(to),
                      dht::TrustRequest(DHT_TYPE_NS, payload));
}

void
RingAccount::connectivityChanged()
{
    if (not dht_.isRunning())
        return;
    if ( upnpEnabled_ ) {
        auto shared = std::static_pointer_cast<RingAccount>(shared_from_this());
        std::thread{[shared] {
            auto& this_ = *shared.get();
            auto oldPort = static_cast<in_port_t>(this_.dhtPortUsed_);
            if (not this_.mapPortUPnP())
                RING_WARN("UPnP: Could not map DHT port");
            auto newPort = static_cast<in_port_t>(this_.dhtPortUsed_);
            if (oldPort != newPort) {
                RING_WARN("DHT port changed: restarting network");
                this_.doRegister_();
            } else
                this_.dht_.connectivityChanged();
        }}.detach();
    } else
        dht_.connectivityChanged();
}

} // namespace ring
