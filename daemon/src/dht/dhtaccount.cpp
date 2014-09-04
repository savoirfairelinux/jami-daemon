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

#include "dhtaccount.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sip/sdp.h"
#include "sip/sipvoiplink.h"
#include "sip/sipcall.h"
#include "sip/sip_utils.h"

#include "dhtcpp/securedht.h"

#include "array_size.h"

#include "call_factory.h"

#ifdef SFL_PRESENCE
//#include "sippresence.h"
#include "client/configurationmanager.h"
#endif

#include "account_schema.h"
#include "logger.h"
#include "manager.h"

#ifdef SFL_VIDEO
#include "video/libav_utils.h"
#endif
#include "fileutils.h"

#include "config/yamlparser.h"
#include <yaml-cpp/yaml.h>

#include <unistd.h>
#include <pwd.h>

#include <algorithm>
#include <array>
#include <memory>
#include <sstream>
#include <cstdlib>

static const char *const VALID_TLS_METHODS[] = {"Default", "TLSv1", "SSLv3", "SSLv23"};
static const char *const VALID_SRTP_KEY_EXCHANGES[] = {"", "sdes", "zrtp"};
constexpr const char * const DHTAccount::ACCOUNT_TYPE;

DHTAccount::DHTAccount(const std::string& accountID, bool presenceEnabled)
    : SIPAccountBase(accountID)
    , tlsSetting_()
    , ciphers_(100)
    , tlsCaListFile_()
    , tlsCertificateFile_()
    , tlsPrivateKeyFile_()
    , tlsPassword_()
    , tlsMethod_("TLSv1")
    , tlsCiphers_()
    , tlsServerName_(0, 0)
    , tlsVerifyServer_(false)
    , tlsVerifyClient_(true)
    , tlsRequireClientCertificate_(true)
    , tlsNegotiationTimeoutSec_("2")
    , zrtpDisplaySas_(true)
    , zrtpDisplaySasOnce_(false)
    , zrtpHelloHash_(true)
    , zrtpNotSuppWarning_(true)
    , receivedParameter_("")
    , rPort_(-1)
    , via_addr_()
    , contactBuffer_()
    , contact_{contactBuffer_, 0}
    , via_tp_(nullptr)
{
    if (nodePath_.empty()) {
        fileutils::check_dir(fileutils::get_cache_dir().c_str());
        nodePath_ = fileutils::get_cache_dir()+DIR_SEPARATOR_STR+getAccountID();
        ERROR("nodePath_ %s", nodePath_.c_str());
    }
    if (mkdir(nodePath_.c_str(), 0700) != 0) {
        perror(nodePath_.c_str());
    }

    if (identityPath_.empty()) {
        fileutils::check_dir(fileutils::get_data_dir().c_str());
        identityPath_ = fileutils::get_data_dir()+DIR_SEPARATOR_STR "id_rsa";
        ERROR("identityPath_ %s", identityPath_.c_str());
    }
}

DHTAccount::~DHTAccount()
{
    // ensure that no registration callbacks survive past this point
    setTransport();
/*
#ifdef SFL_PRESENCE
    delete presence_;
#endif
*/
    dht_.join();
}

std::shared_ptr<SIPCall>
DHTAccount::newIncomingCall(const std::string& id)
{
    return Manager::instance().callFactory.newCall<SIPCall, DHTAccount>(*this, id, Call::INCOMING);
}

template <>
std::shared_ptr<SIPCall>
DHTAccount::newOutgoingCall(const std::string& id, const std::string& toUrl)
{
    std::string to;
    std::string toUri;

    auto call = Manager::instance().callFactory.newCall<SIPCall, DHTAccount>(*this, id, Call::OUTGOING);

    auto dhtf = toUrl.find("dht:");
    dhtf = (dhtf == std::string::npos) ? 0 : dhtf+4;
    toUri = toUrl.substr(dhtf, 40);
    DEBUG("Calling DHT peer %s", toUri.c_str());
    call->setIPToIP(true);
    dht_.get(Dht::InfoHash(toUri), [=](const std::vector<std::shared_ptr<Dht::Value>>& values) {
        for (const auto& v : values) {
            if (v->type != Dht::Value::Type::Peer)
                continue;
            IpAddr peer;
            try {
                peer = IpAddr{ Dht::ServiceAnnouncement(v->data).getPeerAddr() };
            } catch (const std::exception& e) {
                continue;
            }
            std::string toip = getToUri(toUri+"@"+peer.toString(true, true));
            DEBUG("Got DHT peer IP: %s", toip.c_str());
            createOutgoingCall(call, to, toip, peer.getFamily());
            return false;
        }
        return true;
    });
    return call;
}

void
DHTAccount::createOutgoingCall (std::shared_ptr<SIPCall> call, const std::string& to, const std::string& toUri, int family) {
    call->setIPToIP(isIP2IP());
    call->setPeerNumber(toUri);
    call->initRecFilename(to);

    const auto localAddress = ip_utils::getInterfaceAddr(getLocalInterface(), family);
    call->setCallMediaLocal(localAddress);

    // May use the published address as well
    const auto addrSdp = isStunEnabled() or (not getPublishedSameasLocal()) ?
        getPublishedIpAddress() : localAddress;

    // Initialize the session using ULAW as default codec in case of early media
    // The session should be ready to receive media once the first INVITE is sent, before
    // the session initialization is completed
    sfl::AudioCodec* ac = Manager::instance().audioCodecFactory.instantiateCodec(PAYLOAD_CODEC_ULAW);
    if (!ac)
        throw VoipLinkException("Could not instantiate codec for early media");

    std::vector<sfl::AudioCodec *> audioCodecs;
    audioCodecs.push_back(ac);

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

    // Building the local SDP offer
    auto& localSDP = call->getLocalSDP();

    if (getPublishedSameasLocal())
        localSDP.setPublishedIP(addrSdp);
    else
        localSDP.setPublishedIP(getPublishedAddress());

    const bool created = localSDP.createOffer(getActiveAudioCodecs(), getActiveVideoCodecs());

    if (not created or not SIPStartCall(call))
        throw VoipLinkException("Could not send outgoing INVITE request for new call");
}

std::shared_ptr<Call>
DHTAccount::newOutgoingCall(const std::string& id, const std::string& toUrl)
{
    return newOutgoingCall<SIPCall>(id, toUrl);
}

bool
DHTAccount::SIPStartCall(std::shared_ptr<SIPCall>& call)
{
    std::string toUri(call->getPeerNumber()); // expecting a fully well formed sip uri

    pj_str_t pjTo = pj_str((char*) toUri.c_str());

    // Create the from header
    std::string from(getFromUri());
    pj_str_t pjFrom = pj_str((char*) from.c_str());

    pj_str_t pjContact(getContactHeader());

    const std::string debugContactHeader(pj_strbuf(&pjContact), pj_strlen(&pjContact));
    DEBUG("contact header: %s / %s -> %s",
          debugContactHeader.c_str(), from.c_str(), toUri.c_str());

    pjsip_dialog *dialog = NULL;

    if (pjsip_dlg_create_uac(pjsip_ua_instance(), &pjFrom, &pjContact, &pjTo, NULL, &dialog) != PJ_SUCCESS) {
        ERROR("Unable to create SIP dialogs for user agent client when "
              "calling %s", toUri.c_str());
        return false;
    }

    pj_str_t subj_hdr_name = CONST_PJ_STR("Subject");
    pjsip_hdr* subj_hdr = (pjsip_hdr*) pjsip_parse_hdr(dialog->pool, &subj_hdr_name, (char *) "Phone call", 10, NULL);

    pj_list_push_back(&dialog->inv_hdr, subj_hdr);

    pjsip_inv_session* inv = nullptr;
    if (pjsip_inv_create_uac(dialog, call->getLocalSDP().getLocalSdpSession(), 0, &inv) != PJ_SUCCESS) {
        ERROR("Unable to create invite session for user agent client");
        return false;
    }

    if (!inv) {
        ERROR("Call invite is not initialized");
        return PJ_FALSE;
    }

    call->inv.reset(inv);

/*
    updateDialogViaSentBy(dialog);
    if (hasServiceRoute())
        pjsip_dlg_set_route_set(dialog, sip_utils::createRouteSet(getServiceRoute(), call->inv->pool));
*/
/*
    if (hasCredentials() and pjsip_auth_clt_set_credentials(&dialog->auth_sess, getCredentialCount(), getCredInfo()) != PJ_SUCCESS) {
        ERROR("Could not initialize credentials for invite session authentication");
        return false;
    }
*/
    call->inv->mod_data[link_->getModId()] = call.get();

    pjsip_tx_data *tdata;

    if (pjsip_inv_invite(call->inv.get(), &tdata) != PJ_SUCCESS) {
        ERROR("Could not initialize invite messager for this call");
        return false;
    }

    const pjsip_tpselector tp_sel = getTransportSelector();
    if (pjsip_dlg_set_transport(dialog, &tp_sel) != PJ_SUCCESS) {
        ERROR("Unable to associate transport for invite session dialog");
        return false;
    }

    if (pjsip_inv_send_msg(call->inv.get(), tdata) != PJ_SUCCESS) {
        call->inv.reset();
        ERROR("Unable to send invite message for this call");
        return false;
    }

    call->setConnectionState(Call::PROGRESSING);
    call->setState(Call::ACTIVE);

    return true;
}

void DHTAccount::serialize(YAML::Emitter &out)
{
    using namespace Conf;

    out << YAML::BeginMap;
    SIPAccountBase::serialize(out);
    out << YAML::Key << DHT_NODE_PATH_KEY << YAML::Value << nodePath_;
    out << YAML::Key << DHT_PRIVKEY_PATH_KEY << YAML::Value << identityPath_;
    out << YAML::EndMap;
}

void DHTAccount::unserialize(const YAML::Node &node)
{
    using namespace yaml_utils;
    SIPAccountBase::unserialize(node);
    parseValue(node, Conf::DHT_NODE_PATH_KEY, nodePath_);
    parseValue(node, Conf::DHT_PRIVKEY_PATH_KEY, identityPath_);
}

Dht::SecureDht::Identity
DHTAccount::loadIdentity() const
{
    auto path = identityPath_;
    std::vector<char> buffer;
    {
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            if (file.is_open()) file.close();
            auto id = Dht::SecureDht::generateIdentity();
            saveIdentity(id);
            return id;
        }
        file.seekg(0, std::ios::end);
        std::streamsize size = file.tellg();
        if (size > std::numeric_limits<unsigned>::max())
            return {};
        buffer.resize(size);
        file.seekg(0, std::ios::beg);
        if (!file.read(buffer.data(), size)) {
            ERROR("Can't read private key !");
            return {};
        }
    }
    std::vector<char> buffer_crt;
    {
        std::string certPath = path + ".pub";
        std::ifstream file(certPath, std::ios::binary);
        if (!file) {
            if (file.is_open()) file.close();
            auto id = Dht::SecureDht::generateIdentity();
            saveIdentity(id);
            return id;
        }
        file.seekg(0, std::ios::end);
        std::streamsize size = file.tellg();
        if (size > std::numeric_limits<unsigned>::max())
            return {};
        buffer_crt.resize(size);
        file.seekg(0, std::ios::beg);
        if (!file.read(buffer_crt.data(), size)) {
            ERROR("Can't read certificate !");
            return {};
        }
    }

    gnutls_datum_t dt {reinterpret_cast<uint8_t*>(buffer.data()), static_cast<unsigned>(buffer.size())};
    gnutls_x509_privkey_t x509_key;
    gnutls_x509_privkey_init(&x509_key);
    int err = gnutls_x509_privkey_import(x509_key, &dt, GNUTLS_X509_FMT_PEM);
    if (err != GNUTLS_E_SUCCESS) {
        ERROR("Could not read PEM key - %s", gnutls_strerror(err));
        err = gnutls_x509_privkey_import(x509_key, &dt, GNUTLS_X509_FMT_DER);
    }
    if (err != GNUTLS_E_SUCCESS) {
        gnutls_x509_privkey_deinit(x509_key);
        ERROR("Could not read key - %s", gnutls_strerror(err));
        return {};
    }

    gnutls_x509_crt_t certificate;
    gnutls_x509_crt_init(&certificate);

    gnutls_datum_t crt_dt {reinterpret_cast<uint8_t*>(buffer_crt.data()), static_cast<unsigned>(buffer_crt.size())};
    err = gnutls_x509_crt_import(certificate, &crt_dt, GNUTLS_X509_FMT_PEM);
    if (err != GNUTLS_E_SUCCESS) {
        ERROR("Could not read PEM certificate - %s", gnutls_strerror(err));
        err = gnutls_x509_crt_import(certificate, &crt_dt, GNUTLS_X509_FMT_DER);
    }
    if (err != GNUTLS_E_SUCCESS) {
        gnutls_x509_privkey_deinit(x509_key);
        gnutls_x509_crt_deinit(certificate);
        ERROR("Could not read key - %s", gnutls_strerror(err));
        return {};
    }

    return {std::make_shared<Dht::SecureDht::PrivateKey>(x509_key), std::make_shared<Dht::SecureDht::Certificate>(certificate)};
}

void
DHTAccount::saveIdentity(const Dht::SecureDht::Identity id) const
{
    {
        auto buffer = id.first->serialize();
        std::ofstream file(identityPath_, std::ios::trunc | std::ios::binary);
        if (!file.is_open()) {
            ERROR("Could not write key to %s", identityPath_.c_str());
            return;
        }
        file.write((char*)buffer.data(), buffer.size());
    }

    std::string certPath = identityPath_ + ".pub";
    {
        auto buffer = id.second->getPacked();
        std::ofstream file(certPath, std::ios::trunc | std::ios::binary);
        if (!file.is_open()) {
            ERROR("Could not write key to %s", identityPath_.c_str());
            return;
        }
        file.write((char*)buffer.data(), buffer.size());
    }

}

void DHTAccount::setAccountDetails(const std::map<std::string, std::string> &details)
{
    std::cout << "DHTAccount::setAccountDetails" << std::endl;
    for (const auto& i : details) {
        std::cout << i.first << " - " << i.second << std::endl;
    }
    SIPAccountBase::setAccountDetails(details);
}

std::map<std::string, std::string> DHTAccount::getAccountDetails() const
{
    std::map<std::string, std::string> a = SIPAccountBase::getAccountDetails();
    return a;
}

void DHTAccount::doRegister()
{
    DEBUG("doRegister");
    if (dht_.isRunning()) {
        ERROR("DHT already running");
    }
    dht_.run(getLocalPort()+5, loadIdentity(), [=](Dht::Dht::Status s4, Dht::Dht::Status s6) {
        DEBUG("Dht status : %d %d", (int)s4, (int)s6);
        auto status = std::max(s4, s6);
        switch(status) {
        case Dht::Dht::Status::Disconnected:
            setRegistrationState(RegistrationState::UNREGISTERED);
            break;
        case Dht::Dht::Status::Connecting:
            setRegistrationState(RegistrationState::TRYING);
            break;
        case Dht::Dht::Status::Connected:
            setRegistrationState(RegistrationState::REGISTERED);
            break;
        default:
            setRegistrationState(RegistrationState::ERROR_GENERIC);
            break;
        }
    });
    dht_.put(dht_.getId(), Dht::Value{Dht::Value::Type::Peer, Dht::ServiceAnnouncement(getLocalPort())}, [](bool ok) {
        DEBUG("Peer announce callback ! %d", ok);
    });

    username_ = dht_.getId().toString();

    Manager::instance().registerEventHandler((uintptr_t)this, std::bind(&Dht::DhtRunner::loop, &dht_));
    setRegistrationState(RegistrationState::TRYING);

    dht_.bootstrap(loadNodes());
    if (!hostname_.empty()) {
        std::stringstream ss(hostname_);
        std::vector<sockaddr_storage> bootstrap;
        std::string node_addr;
        while (std::getline(ss, node_addr, ';')) {
            auto ips = ip_utils::getAddrList(node_addr);
            bootstrap.insert(bootstrap.end(), ips.begin(), ips.end());
        }
        for (auto ip : bootstrap)
            DEBUG("Bootstrap node: %s", IpAddr(ip).toString(true).c_str());
        dht_.bootstrap(bootstrap);
    }
}


void DHTAccount::doUnregister(std::function<void(bool)> released_cb)
{
    Manager::instance().unregisterEventHandler((uintptr_t)this);
    saveNodes(dht_.getNodes());
    dht_.join();
    setRegistrationState(RegistrationState::UNREGISTERED);
    if (released_cb)
        released_cb(false);
}

void DHTAccount::saveNodes(const std::vector<Dht::Dht::NodeExport>& nodes) const
{
    if (nodes.empty())
        return;
    std::string nodesPath = nodePath_+DIR_SEPARATOR_STR "nodes";
    {
        std::ofstream file(nodesPath, std::ios::trunc);
        if (!file.is_open()) {
            ERROR("Could not save nodes to %s", nodesPath.c_str());
            return;
        }
        for (auto& n : nodes)
            file << n.id << " " << IpAddr(n.ss).toString(true) << "\n";
    }
}

std::vector<Dht::Dht::NodeExport>
DHTAccount::loadNodes() const
{
    std::vector<Dht::Dht::NodeExport> nodes;
    std::string nodesPath = nodePath_+DIR_SEPARATOR_STR "nodes";
    {
        std::ifstream file(nodesPath);
        if (!file.is_open()) {
            ERROR("Could not load nodes from %s", nodesPath.c_str());
            return nodes;
        }
        std::string line;
        while (std::getline(file, line))
        {
            std::istringstream iss(line);
            std::string id, ipstr;
            if (!(iss >> id >> ipstr)) { break; }
            IpAddr ip {ipstr};
            Dht::Dht::NodeExport e {{id}, ip, ip.getLength()};
            nodes.push_back(e);
        }
    }
}

#if HAVE_TLS
pjsip_ssl_method DHTAccount::sslMethodStringToPjEnum(const std::string& method)
{
    if (method == "Default")
        return PJSIP_SSL_UNSPECIFIED_METHOD;

    if (method == "TLSv1")
        return PJSIP_TLSV1_METHOD;

    if (method == "SSLv3")
        return PJSIP_SSLV3_METHOD;

    if (method == "SSLv23")
        return PJSIP_SSLV23_METHOD;

    return PJSIP_SSL_UNSPECIFIED_METHOD;
}

void DHTAccount::trimCiphers()
{
    int sum = 0;
    int count = 0;

    // PJSIP aborts if our cipher list exceeds 1000 characters
    static const int MAX_CIPHERS_STRLEN = 1000;

    for (const auto &item : ciphers_) {
        sum += strlen(pj_ssl_cipher_name(item));

        if (sum > MAX_CIPHERS_STRLEN)
            break;

        ++count;
    }

    ciphers_.resize(count);
    DEBUG("Using %u ciphers", ciphers_.size());
}

void DHTAccount::initTlsConfiguration()
{
    unsigned cipherNum;

    // Determine the cipher list supported on this machine
    cipherNum = ciphers_.size();

    if (pj_ssl_cipher_get_availables(&ciphers_.front(), &cipherNum) != PJ_SUCCESS)
        ERROR("Could not determine cipher list on this system");

    ciphers_.resize(cipherNum);

    trimCiphers();

    // TLS listener is unique and should be only modified through IP2IP_PROFILE
    pjsip_tls_setting_default(&tlsSetting_);

    pj_cstr(&tlsSetting_.ca_list_file, tlsCaListFile_.c_str());
    pj_cstr(&tlsSetting_.cert_file, tlsCertificateFile_.c_str());
    pj_cstr(&tlsSetting_.privkey_file, tlsPrivateKeyFile_.c_str());
    pj_cstr(&tlsSetting_.password, tlsPassword_.c_str());
    tlsSetting_.method = sslMethodStringToPjEnum(tlsMethod_);
    tlsSetting_.ciphers_num = ciphers_.size();
    tlsSetting_.ciphers = &ciphers_.front();

    tlsSetting_.verify_server = tlsVerifyServer_;
    tlsSetting_.verify_client = tlsVerifyClient_;
    tlsSetting_.require_client_cert = tlsRequireClientCertificate_;

    tlsSetting_.timeout.sec = atol(tlsNegotiationTimeoutSec_.c_str());

    tlsSetting_.qos_type = PJ_QOS_TYPE_BEST_EFFORT;
    tlsSetting_.qos_ignore_error = PJ_TRUE;
}

#endif

void DHTAccount::loadConfig()
{
#if HAVE_TLS

    if (tlsEnable_) {
        initTlsConfiguration();
        transportType_ = PJSIP_TRANSPORT_TLS;
    } else
#endif
        transportType_ = PJSIP_TRANSPORT_UDP;
}

bool DHTAccount::fullMatch(const std::string& username, const std::string& hostname, pjsip_endpoint *endpt, pj_pool_t *pool) const
{
    return userMatch(username);
}

bool DHTAccount::userMatch(const std::string& username) const
{
    return !username.empty() and username == username_;
}

std::string DHTAccount::getLoginName()
{
    struct passwd * user_info = getpwuid(getuid());
    return user_info ? user_info->pw_name : "";
}

std::string DHTAccount::getFromUri() const
{
    std::string scheme;
    std::string transport;
    std::string username(username_);
    std::string node = dht_.getId().toString();

    // UDP does not require the transport specification
    if (transportType_ == PJSIP_TRANSPORT_TLS || transportType_ == PJSIP_TRANSPORT_TLS6) {
        scheme = "sips:";
        transport = ";transport=" + std::string(pjsip_transport_get_type_name(transportType_));
    } else
        scheme = "sip:";

    // Get login name if username is not specified
    if (username_.empty())
        username = getLoginName();

    return username + "<" + scheme + node + "@dht.invalid" + transport + ">";
}

std::string DHTAccount::getToUri(const std::string& username) const
{
    std::string scheme;
    std::string transport;
    std::string hostname;

    // UDP does not require the transport specification
    if (transportType_ == PJSIP_TRANSPORT_TLS || transportType_ == PJSIP_TRANSPORT_TLS6) {
        scheme = "sips:";
        transport = ";transport=" + std::string(pjsip_transport_get_type_name(transportType_));
    } else
        scheme = "sip:";

    // Check if scheme is already specified
    if (username.find("sip") == 0)
        scheme = "";

    // Check if hostname is already specified
/*    if (username.find("@") == std::string::npos)
        hostname = dht_.getId().toString();*/
/*
#if HAVE_IPV6
    if (not hostname.empty() and IpAddr::isIpv6(hostname))
        hostname = IpAddr(hostname).toString(false, true);
#endif
*/
    return "<" + scheme + username + (hostname.empty() ? "" : "@") + hostname + transport + ">";
}

pj_str_t
DHTAccount::getContactHeader()
{
    if (transport_ == nullptr)
        ERROR("Transport not created yet");

    // The transport type must be specified, in our case START_OTHER refers to stun transport
    pjsip_transport_type_e transportType = transportType_;

    if (transportType == PJSIP_TRANSPORT_START_OTHER)
        transportType = PJSIP_TRANSPORT_UDP;

    // Else we determine this infor based on transport information
    std::string address;
    pj_uint16_t port;

    link_->sipTransport->findLocalAddressFromTransport(transport_, transportType, hostname_, address, port);

    if (not publishedSameasLocal_) {
        address = publishedIpAddress_;
        port = publishedPort_;
        DEBUG("Using published address %s and port %d", address.c_str(), port);
    } else {
        if (!receivedParameter_.empty()) {
            address = receivedParameter_;
            DEBUG("Using received address %s", address.c_str());
        }

        if (rPort_ != -1 and rPort_ != 0) {
            port = rPort_;
            DEBUG("Using received port %d", port);
        }
    }

    // UDP does not require the transport specification
    std::string scheme;
    std::string transport;

#if HAVE_IPV6
    /* Enclose IPv6 address in square brackets */
    if (IpAddr::isIpv6(address)) {
        address = IpAddr(address).toString(false, true);
    }
#endif

    if (transportType != PJSIP_TRANSPORT_UDP and transportType != PJSIP_TRANSPORT_UDP6) {
        scheme = "sips:";
        transport = ";transport=" + std::string(pjsip_transport_get_type_name(transportType));
    } else
        scheme = "sip:";

    contact_.slen = pj_ansi_snprintf(contact_.ptr, PJSIP_MAX_URL_SIZE,
                                     "%s%s<%s%s%s%s:%d%s>",
                                     displayName_.c_str(),
                                     (displayName_.empty() ? "" : " "),
                                     scheme.c_str(),
                                     username_.c_str(),
                                     (username_.empty() ? "" : "@"),
                                     address.c_str(),
                                     port,
                                     transport.c_str());
    return contact_;
}

#ifdef SFL_PRESENCE
/**
 *  Enable the presence module
 */
void
DHTAccount::enablePresence(const bool& enabled)
{
}

/**
 *  Set the presence (PUBLISH/SUBSCRIBE) support flags
 *  and process the change.
 */
void
DHTAccount::supportPresence(int function, bool enabled)
{
}
#endif

MatchRank
DHTAccount::matches(const std::string &userName, const std::string &server,
                    pjsip_endpoint *endpt, pj_pool_t *pool) const
{
    if (fullMatch(userName, server, endpt, pool)) {
        DEBUG("Matching account id in request is a fullmatch %s@%s", userName.c_str(), server.c_str());
        return MatchRank::FULL;
    } else if (userMatch(userName)) {
        DEBUG("Matching account id in request with username %s", userName.c_str());
        return MatchRank::PARTIAL;
    } else {
        return MatchRank::NONE;
    }
}
/*
void DHTAccount::updateDialogViaSentBy(pjsip_dialog *dlg)
{
    if (allowViaRewrite_ && via_addr_.host.slen > 0)
        pjsip_dlg_set_via_sent_by(dlg, &via_addr_, via_tp_);
}
*/