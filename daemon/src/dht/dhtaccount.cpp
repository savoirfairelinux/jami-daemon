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
#include <dirent.h>

#include <algorithm>
#include <array>
#include <memory>
#include <sstream>
#include <cstdlib>

constexpr const char * const DHTAccount::ACCOUNT_TYPE;

DHTAccount::DHTAccount(const std::string& accountID, bool /* presenceEnabled */)
    : SIPAccountBase(accountID)
{
    fileutils::check_dir(fileutils::get_cache_dir().c_str());
    nodePath_ = fileutils::get_cache_dir()+DIR_SEPARATOR_STR+getAccountID();

    /*  ~/.local/{appname}    */
    fileutils::check_dir(fileutils::get_data_dir().c_str());

    /*  ~/.local/{appname}/{accountID}    */
    idPath_ = fileutils::get_data_dir()+DIR_SEPARATOR_STR+getAccountID();
    fileutils::check_dir(idPath_.c_str());
    dataPath_ = idPath_ + DIR_SEPARATOR_STR "values";
    checkIdentityPath();

    int rc = gnutls_global_init();
    if (rc != GNUTLS_E_SUCCESS) {
        SFL_ERR("Error initializing GnuTLS : %s", gnutls_strerror(rc));
        throw VoipLinkException("Can't initialize GnuTLS.");
    }
}

DHTAccount::~DHTAccount()
{
    Manager::instance().unregisterEventHandler((uintptr_t)this);
    setTransport();
    dht_.join();
    gnutls_global_deinit();
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
    auto call = Manager::instance().callFactory.newCall<SIPCall, DHTAccount>(*this, id, Call::OUTGOING);
    auto dhtf = toUrl.find("dht:");
    dhtf = (dhtf == std::string::npos) ? 0 : dhtf+4;
    const std::string toUri = toUrl.substr(dhtf, 40);
    SFL_DBG("Calling DHT peer %s", toUri.c_str());
    call->setIPToIP(true);

    auto resolved = std::make_shared<bool>(false);
    dht_.get(dht::InfoHash(toUri),
        [this,call,toUri,resolved](const std::vector<std::shared_ptr<dht::Value>>& values) {
            if (*resolved)
                return false;
            for (const auto& v : values) {
                SFL_DBG("Resolved value : %s", v->toString().c_str());
                IpAddr peer;
                try {
                    peer = IpAddr{ dht::ServiceAnnouncement(v->data).getPeerAddr() };
                } catch (const std::exception& e) {
                    SFL_ERR("Exception while reading value : %s", e.what());
                    continue;
                }
                *resolved = true;
                std::string toip = getToUri(toUri+"@"+peer.toString(true, true));
                SFL_DBG("Got DHT peer IP: %s", toip.c_str());
                createOutgoingCall(call, toUri, toip, peer);
                return false;
            }
            return true;
        },
        [call,resolved] (bool ok){
            if (not *resolved) {
                call->setConnectionState(Call::DISCONNECTED);
                call->setState(Call::MERROR);
            }
        },
        dht::Value::TypeFilter(dht::ServiceAnnouncement::TYPE));
    return call;
}

void
DHTAccount::createOutgoingCall(const std::shared_ptr<SIPCall>& call, const std::string& to, const std::string& toUri, const IpAddr& peer)
{
    SFL_WARN("DHTAccount::createOutgoingCall to: %s toUri: %s tlsListener: %d", to.c_str(), toUri.c_str(), tlsListener_?1:0);
    std::shared_ptr<SipTransport> t = link_->sipTransport->getTlsTransport(tlsListener_, getToUri(peer.toString(true, true)));
    setTransport(t);
    call->setTransport(t);
    call->setIPToIP(true);
    call->setPeerNumber(toUri);
    call->initRecFilename(to);

    const auto localAddress = ip_utils::getInterfaceAddr(getLocalInterface(), peer.getFamily());
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
DHTAccount::SIPStartCall(const std::shared_ptr<SIPCall>& call)
{
    std::string toUri(call->getPeerNumber()); // expecting a fully well formed sip uri

    pj_str_t pjTo = pj_str((char*) toUri.c_str());

    // Create the from header
    std::string from(getFromUri());
    pj_str_t pjFrom = pj_str((char*) from.c_str());

    pj_str_t pjContact;
    {
        auto transport = call->getTransport();
        pjContact = getContactHeader(transport ? transport->get() : nullptr);
    }

    const std::string debugContactHeader(pj_strbuf(&pjContact), pj_strlen(&pjContact));
    SFL_DBG("contact header: %s / %s -> %s",
          debugContactHeader.c_str(), from.c_str(), toUri.c_str());

    pjsip_dialog *dialog = NULL;

    if (pjsip_dlg_create_uac(pjsip_ua_instance(), &pjFrom, &pjContact, &pjTo, NULL, &dialog) != PJ_SUCCESS) {
        SFL_ERR("Unable to create SIP dialogs for user agent client when "
              "calling %s", toUri.c_str());
        return false;
    }

    pj_str_t subj_hdr_name = CONST_PJ_STR("Subject");
    pjsip_hdr* subj_hdr = (pjsip_hdr*) pjsip_parse_hdr(dialog->pool, &subj_hdr_name, (char *) "Phone call", 10, NULL);

    pj_list_push_back(&dialog->inv_hdr, subj_hdr);

    pjsip_inv_session* inv = nullptr;
    if (pjsip_inv_create_uac(dialog, call->getLocalSDP().getLocalSdpSession(), 0, &inv) != PJ_SUCCESS) {
        SFL_ERR("Unable to create invite session for user agent client");
        return false;
    }

    if (!inv) {
        SFL_ERR("Call invite is not initialized");
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
        SFL_ERR("Could not initialize invite messager for this call");
        return false;
    }

    const pjsip_tpselector tp_sel = getTransportSelector();
    if (pjsip_dlg_set_transport(dialog, &tp_sel) != PJ_SUCCESS) {
        SFL_ERR("Unable to associate transport for invite session dialog");
        return false;
    }

    if (pjsip_inv_send_msg(call->inv.get(), tdata) != PJ_SUCCESS) {
        call->inv.reset();
        SFL_ERR("Unable to send invite message for this call");
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

void DHTAccount::unserialize(const YAML::Node &node)
{
    using namespace yaml_utils;
    SIPAccountBase::unserialize(node);
    in_port_t port {DHT_DEFAULT_PORT};
    parseValue(node, Conf::DHT_PORT_KEY, port);
    dhtPort_ = port;
    if (dhtPort_ == 0)
        dhtPort_ = DHT_DEFAULT_PORT;
    parseValue(node, Conf::DHT_PRIVKEY_PATH_KEY, privkeyPath_);
    parseValue(node, Conf::DHT_CERT_PATH_KEY, certPath_);
    parseValue(node, Conf::DHT_CA_CERT_PATH_KEY, cacertPath_);
    checkIdentityPath();
}

void
DHTAccount::checkIdentityPath()
{
    if (not privkeyPath_.empty() and not dataPath_.empty())
        return;

    const auto idPath = fileutils::get_data_dir()+DIR_SEPARATOR_STR+getAccountID();
    privkeyPath_ = idPath + DIR_SEPARATOR_STR "dht.pem";
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
        SFL_ERR("Could not write key to %s", path.c_str());
        return;
    }
    file.write((char*)data.data(), data.size());
}

dht::crypto::Identity
DHTAccount::loadIdentity()
{
    dht::crypto::Certificate ca_cert;

    dht::crypto::Certificate dht_cert;
    dht::crypto::PrivateKey dht_key;

    try {
        ca_cert = dht::crypto::Certificate(loadFile(cacertPath_));
        dht_cert = dht::crypto::Certificate(loadFile(certPath_));
        dht_key = dht::crypto::PrivateKey(loadFile(privkeyPath_));
    }
    catch (const std::exception& e) {
        SFL_ERR("Error loading identity: %s", e.what());
        auto ca = dht::crypto::generateIdentity("Ring CA");
        if (!ca.first || !ca.second) {
            throw VoipLinkException("Can't generate CA for this account.");
        }
        auto id = dht::crypto::generateIdentity("Ring", ca);
        if (!id.first || !id.second) {
            throw VoipLinkException("Can't generate identity for this account.");
        }
        idPath_ = fileutils::get_data_dir()+DIR_SEPARATOR_STR+getAccountID();
        fileutils::check_dir(idPath_.c_str());

        saveIdentity(ca, idPath_ + DIR_SEPARATOR_STR "ca");
        cacertPath_ = idPath_ + DIR_SEPARATOR_STR "ca.crt";

        saveIdentity(id, idPath_ + DIR_SEPARATOR_STR "dht");
        certPath_ = idPath_ + DIR_SEPARATOR_STR "dht.crt";
        privkeyPath_ = idPath_ + DIR_SEPARATOR_STR "dht.pem";

        return id;
    }

    return {std::make_shared<dht::crypto::PrivateKey>(std::move(dht_key)), std::make_shared<dht::crypto::Certificate>(std::move(dht_cert))};
}

void
DHTAccount::saveIdentity(const dht::crypto::Identity id, const std::string& path) const
{
    if (id.first)
        saveFile(path+".pem", id.first->serialize());
    if (id.second)
        saveFile(path+".crt", id.second->getPacked());
}

template <typename T>
static void
parseInt(const std::map<std::string, std::string> &details, const char *key, T &i)
{
    const auto iter = details.find(key);
    if (iter == details.end()) {
        SFL_ERR("Couldn't find key %s", key);
        return;
    }
    i = atoi(iter->second.c_str());
}

void DHTAccount::setAccountDetails(const std::map<std::string, std::string> &details)
{
    SIPAccountBase::setAccountDetails(details);
    parseInt(details, CONFIG_DHT_PORT, dhtPort_);
    if (dhtPort_ == 0)
        dhtPort_ = DHT_DEFAULT_PORT;
    parseString(details, CONFIG_DHT_PRIVKEY_PATH, privkeyPath_);
    parseString(details, CONFIG_DHT_CERT_PATH, certPath_);
    checkIdentityPath();
}

std::map<std::string, std::string> DHTAccount::getAccountDetails() const
{
    std::map<std::string, std::string> a = SIPAccountBase::getAccountDetails();

    std::stringstream dhtport;
    dhtport << dhtPort_;
    a[CONFIG_DHT_PORT] = dhtport.str();
    a[CONFIG_DHT_PRIVKEY_PATH] = privkeyPath_;
    a[CONFIG_DHT_CERT_PATH] = certPath_;
    return a;
}

void DHTAccount::doRegister()
{
    if (not isEnabled()) {
        SFL_WARN("Account must be enabled to register, ignoring");
        return;
    }

    try {
        if (dht_.isRunning()) {
            SFL_ERR("DHT already running (stopping it first).");
            dht_.join();
        }
        dht_.run(dhtPort_, loadIdentity(), [=](dht::Dht::Status s4, dht::Dht::Status s6) {
            SFL_WARN("Dht status : %d %d", (int)s4, (int)s6);
            auto status = std::max(s4, s6);
            switch(status) {
            case dht::Dht::Status::Connecting:
            case dht::Dht::Status::Connected:
                setRegistrationState(status == dht::Dht::Status::Connected ? RegistrationState::REGISTERED : RegistrationState::TRYING);
                if (!tlsListener_) {
                    initTlsConfiguration();
                    tlsListener_ = link_->sipTransport->getTlsListener(
                        SipTransportDescr {getTransportType(), getTlsListenerPort(), getLocalInterface()},
                        getTlsSetting());
                    if (!tlsListener_) {
                        setRegistrationState(RegistrationState::ERROR_GENERIC);
                        SFL_ERR("Error creating TLS listener.");
                        return;
                    }
                }
                break;
            case dht::Dht::Status::Disconnected:
            default:
                setRegistrationState(status == dht::Dht::Status::Disconnected ? RegistrationState::UNREGISTERED : RegistrationState::ERROR_GENERIC);
                tlsListener_.reset();
                break;
            }
        });

        dht_.importValues(loadValues());

        dht_.put(dht_.getId(), dht::Value{dht::ServiceAnnouncement::TYPE.id, dht::ServiceAnnouncement(getTlsListenerPort())}, [](bool ok) {
            SFL_DBG("Peer announce callback ! %d", ok);
        });

        username_ = dht_.getId().toString();

        Manager::instance().registerEventHandler((uintptr_t)this, std::bind(&dht::DhtRunner::loop, &dht_));
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
                SFL_DBG("Bootstrap node: %s", IpAddr(ip).toString(true).c_str());
            dht_.bootstrap(bootstrap);
        }
    }
    catch (const std::exception& e) {
        SFL_ERR("Error registering DHT account: %s", e.what());
        setRegistrationState(RegistrationState::ERROR_GENERIC);
    }
}


void DHTAccount::doUnregister(std::function<void(bool)> released_cb)
{
    Manager::instance().unregisterEventHandler((uintptr_t)this);
    saveNodes(dht_.exportNodes());
    saveValues(dht_.exportValues());
    dht_.join();
    tlsListener_.reset();
    setRegistrationState(RegistrationState::UNREGISTERED);
    if (released_cb)
        released_cb(false);
}

void DHTAccount::saveNodes(const std::vector<dht::Dht::NodeExport>& nodes) const
{
    if (nodes.empty())
        return;
    fileutils::check_dir(nodePath_.c_str());
    std::string nodesPath = nodePath_+DIR_SEPARATOR_STR "nodes";
    {
        std::ofstream file(nodesPath, std::ios::trunc);
        if (!file.is_open()) {
            SFL_ERR("Could not save nodes to %s", nodesPath.c_str());
            return;
        }
        for (auto& n : nodes)
            file << n.id << " " << IpAddr(n.ss).toString(true) << "\n";
    }
}

void DHTAccount::saveValues(const std::vector<dht::Dht::ValuesExport>& values) const
{
    fileutils::check_dir(dataPath_.c_str());
    for (const auto& v : values) {
        const std::string fname = dataPath_ + DIR_SEPARATOR_STR + v.first.toString();
        std::ofstream file(fname, std::ios::trunc | std::ios::out | std::ios::binary);
        file.write((const char*)v.second.data(), v.second.size());
    }
}

std::vector<dht::Dht::NodeExport>
DHTAccount::loadNodes() const
{
    std::vector<dht::Dht::NodeExport> nodes;
    std::string nodesPath = nodePath_+DIR_SEPARATOR_STR "nodes";
    {
        std::ifstream file(nodesPath);
        if (!file.is_open()) {
            SFL_ERR("Could not load nodes from %s", nodesPath.c_str());
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
DHTAccount::loadValues() const
{
    struct dirent *entry;
    DIR *dp = opendir(dataPath_.c_str());
    if (!dp) {
        SFL_ERR("Could not load values from %s", dataPath_.c_str());
        return {};
    }

    std::vector<dht::Dht::ValuesExport> values;
    while ((entry = readdir(dp))) {
        try {
            const std::string fname {entry->d_name};
            if (fname == "." || fname == "..")
                continue;
            std::ifstream ifs(dataPath_+DIR_SEPARATOR_STR+fname, std::ifstream::in | std::ifstream::binary);
            std::istreambuf_iterator<char> begin(ifs), end;
            values.push_back({{fname}, std::vector<uint8_t>{begin, end}});
        } catch (const std::exception& e) {
            SFL_ERR("Error reading value: %s", e.what());
            continue;
        }
    }
    closedir(dp);

    return values;
}

void DHTAccount::initTlsConfiguration()
{
    // TLS listener is unique and should be only modified through IP2IP_PROFILE
    pjsip_tls_setting_default(&tlsSetting_);

    pj_cstr(&tlsSetting_.ca_list_file, cacertPath_.c_str());
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

void DHTAccount::loadConfig()
{
    SFL_WARN("DHTAccount::loadConfig()");
    initTlsConfiguration();
    transportType_ = PJSIP_TRANSPORT_TLS;
}

bool DHTAccount::userMatch(const std::string& username) const
{
    return !username.empty() and username == dht_.getId().toString();
}

MatchRank
DHTAccount::matches(const std::string &userName, const std::string &server) const
{
    if (userMatch(userName)) {
        SFL_DBG("Matching account id in request with username %s", userName.c_str());
        return MatchRank::FULL;
    } else {
        return MatchRank::NONE;
    }
}

std::string DHTAccount::getFromUri() const
{
    return "dht:" + dht_.getId().toString() + "@dht.invalid";
}

std::string DHTAccount::getToUri(const std::string& to) const
{
    const std::string transport {pjsip_transport_get_type_name(transportType_)};
    return "<sips:" + to + ";transport=" + transport + ">";
}

pj_str_t
DHTAccount::getContactHeader(pjsip_transport* t)
{
    if (!t && transport_)
        t = transport_->get();
    if (!t)
        SFL_ERR("Transport not created yet");

    // The transport type must be specified, in our case START_OTHER refers to stun transport
    pjsip_transport_type_e transportType = transportType_;

    if (transportType == PJSIP_TRANSPORT_START_OTHER)
        transportType = PJSIP_TRANSPORT_UDP;

    // Else we determine this infor based on transport information
    std::string address = "dht.invalid";
    pj_uint16_t port = getTlsListenerPort();

    link_->sipTransport->findLocalAddressFromTransport(t, transportType, hostname_, address, port);

#if HAVE_IPV6
    /* Enclose IPv6 address in square brackets */
    if (IpAddr::isIpv6(address)) {
        address = IpAddr(address).toString(false, true);
    }
#endif

    SFL_WARN("getContactHeader %s@%s:%d", username_.c_str(), address.c_str(), port);
    contact_.slen = pj_ansi_snprintf(contact_.ptr, PJSIP_MAX_URL_SIZE,
                                     "<sips:%s%s%s:%d;transport=%s>",
                                     username_.c_str(),
                                     (username_.empty() ? "" : "@"),
                                     address.c_str(),
                                     port,
                                     pjsip_transport_get_type_name(transportType));
    return contact_;
}

#ifdef SFL_PRESENCE
/**
 *  Enable the presence module
 */
void
DHTAccount::enablePresence(const bool& /* enabled */)
{
}

/**
 *  Set the presence (PUBLISH/SUBSCRIBE) support flags
 *  and process the change.
 */
void
DHTAccount::supportPresence(int /* function */, bool /* enabled*/)
{
}
#endif

/*
void DHTAccount::updateDialogViaSentBy(pjsip_dialog *dlg)
{
    if (allowViaRewrite_ && via_addr_.host.slen > 0)
        pjsip_dlg_set_via_sent_by(dlg, &via_addr_, via_tp_);
}
*/
