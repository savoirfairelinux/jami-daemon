#include "dtls_srtp.h"

#include "base64.h"
#include "fileutils.h"
#include "logger.h"
#include "manager.h"

#include <dhtnet/certstore.h>
#include <dhtnet/diffie-hellman.h>
#include <dhtnet/generic_io.h>
#include <dhtnet/ice_socket.h>
#include <dhtnet/tls_session.h>

#include <opendht/crypto.h>
#include <opendht/log.h>

#include <gnutls/gnutls.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <condition_variable>
#include <cctype>
#include <filesystem>
#include <future>
#include <mutex>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace jami {
namespace {

static constexpr auto DTLS_SRTP_TIMEOUT = std::chrono::seconds(40);
static constexpr std::string_view DTLS_SRTP_PROFILES = "SRTP_AES128_CM_HMAC_SHA1_80:SRTP_AES128_CM_HMAC_SHA1_32";

class IceSocketDtlsTransport final : public dhtnet::GenericSocket<uint8_t>
{
public:
    IceSocketDtlsTransport(dhtnet::IceSocket& socket, bool initiator)
        : socket_(socket)
        , initiator_(initiator)
    {}

    ~IceSocketDtlsTransport() override { shutdown(); }

    void shutdown() override
    {
        if (!stopped_.exchange(true))
            socket_.setOnRecv(nullptr);
    }

    void setOnRecv(RecvCb&& cb) override
    {
        {
            std::lock_guard lk(cbMutex_);
            recvCb_ = std::move(cb);
        }

        if (!recvCb_) {
            socket_.setOnRecv(nullptr);
            return;
        }

        socket_.setOnRecv([this](unsigned char* buf, size_t len) {
            RecvCb cb;
            {
                std::lock_guard lk(cbMutex_);
                cb = recvCb_;
            }
            return cb ? cb(buf, len) : static_cast<ssize_t>(len);
        });
    }

    bool isReliable() const override { return false; }

    bool isInitiator() const override { return initiator_; }

    int maxPayload() const override
    {
        return std::max<int>(512, 1500 - static_cast<int>(socket_.getTransportOverhead()));
    }

    int waitForData(std::chrono::milliseconds timeout, std::error_code& ec) const override
    {
        if (stopped_) {
            ec = std::make_error_code(std::errc::broken_pipe);
            return -1;
        }

        const auto ready = socket_.waitForData(timeout);
        if (ready < 0) {
            ec.assign(errno, std::generic_category());
            return -1;
        }

        ec.clear();
        return ready > 0 ? 1 : 0;
    }

    std::size_t write(const ValueType* buf, std::size_t len, std::error_code& ec) override
    {
        if (stopped_) {
            ec = std::make_error_code(std::errc::broken_pipe);
            return 0;
        }

        const auto written = socket_.send(buf, len);
        if (written < 0) {
            ec.assign(errno, std::generic_category());
            return 0;
        }

        ec.clear();
        return static_cast<std::size_t>(written);
    }

    std::size_t read(ValueType*, std::size_t, std::error_code& ec) override
    {
        ec = std::make_error_code(std::errc::operation_not_supported);
        return 0;
    }

private:
    dhtnet::IceSocket& socket_;
    bool initiator_ {false};
    std::atomic_bool stopped_ {false};
    mutable std::mutex cbMutex_ {};
    RecvCb recvCb_ {};
};

gnutls_digest_algorithm_t
parseDigest(std::string_view hash)
{
    std::string normalized;
    normalized.reserve(hash.size());
    std::transform(hash.begin(), hash.end(), std::back_inserter(normalized), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });

    if (normalized == "SHA-256")
        return GNUTLS_DIG_SHA256;
    if (normalized == "SHA-1")
        return GNUTLS_DIG_SHA1;
    return GNUTLS_DIG_UNKNOWN;
}

std::string
normalizeFingerprint(std::string_view fingerprint)
{
    std::string normalized;
    normalized.reserve(fingerprint.size());
    for (const auto c : fingerprint) {
        if (std::isspace(static_cast<unsigned char>(c)))
            continue;
        normalized.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }
    return normalized;
}

std::string
formatFingerprint(gnutls_x509_crt_t certificate, gnutls_digest_algorithm_t digest)
{
    std::array<unsigned char, 64> raw {};
    size_t rawSize = raw.size();
    const auto ret = gnutls_x509_crt_get_fingerprint(certificate, digest, raw.data(), &rawSize);
    if (ret != GNUTLS_E_SUCCESS)
        throw std::runtime_error(gnutls_strerror(ret));

    static constexpr char HEX[] = "0123456789ABCDEF";
    std::string formatted;
    formatted.reserve(rawSize * 3);
    for (size_t i = 0; i < rawSize; ++i) {
        if (i != 0)
            formatted.push_back(':');
        formatted.push_back(HEX[raw[i] >> 4]);
        formatted.push_back(HEX[raw[i] & 0x0F]);
    }
    return formatted;
}

int
verifyRemoteFingerprint(gnutls_session_t session,
                        std::string_view expectedHash,
                        std::string_view expectedFingerprint)
{
    const auto digest = parseDigest(expectedHash);
    if (digest == GNUTLS_DIG_UNKNOWN)
        return GNUTLS_E_CERTIFICATE_ERROR;

    unsigned int remoteCount = 0;
    const auto* remote = gnutls_certificate_get_peers(session, &remoteCount);
    if (!remote || remoteCount == 0)
        return GNUTLS_E_CERTIFICATE_ERROR;

    std::vector<std::pair<uint8_t*, uint8_t*>> chain;
    chain.reserve(remoteCount);
    for (unsigned int i = 0; i < remoteCount; ++i)
        chain.emplace_back(remote[i].data, remote[i].data + remote[i].size);

    dht::crypto::Certificate certificate {chain};
    const auto actual = formatFingerprint(certificate.cert, digest);
    return normalizeFingerprint(actual) == normalizeFingerprint(expectedFingerprint) ? GNUTLS_E_SUCCESS
                                                                                     : GNUTLS_E_CERTIFICATE_ERROR;
}

std::string
buildKeyInfo(const std::vector<uint8_t>& key, const std::vector<uint8_t>& salt)
{
    std::vector<uint8_t> data;
    data.reserve(key.size() + salt.size());
    data.insert(data.end(), key.begin(), key.end());
    data.insert(data.end(), salt.begin(), salt.end());
    return base64::encode(data);
}

const std::shared_future<dhtnet::tls::DhParams>&
mediaDhParams()
{
    static const auto future = std::async(std::launch::async, [] { return dhtnet::tls::DhParams::generate(); }).share();
    return future;
}

std::shared_ptr<dht::log::Logger>
mediaDtlsLogger()
{
    static const auto logger = [] {
        if (auto parent = dht::log::getStdLogger())
            return parent->createChild("media.dtls");
        return std::shared_ptr<dht::log::Logger> {};
    }();
    return logger;
}

dhtnet::tls::CertificateStore&
mediaDtlsCertStore()
{
    static auto store = std::make_unique<dhtnet::tls::CertificateStore>(
        std::filesystem::temp_directory_path() / "jami-media-dtls", mediaDtlsLogger());
    return *store;
}

} // namespace

std::string
getDtlsFingerprint(const dht::crypto::Certificate& certificate, std::string_view hash)
{
    const auto digest = parseDigest(hash);
    if (digest == GNUTLS_DIG_UNKNOWN)
        throw std::runtime_error("Unsupported DTLS fingerprint hash");
    return formatFingerprint(certificate.cert, digest);
}

DtlsSrtpContext
negotiateDtlsSrtp(dhtnet::IceSocket& rtpSocket,
                  DtlsSetup localSetup,
                  std::string_view remoteFingerprintHash,
                  std::string_view remoteFingerprint,
                  const std::shared_ptr<dht::crypto::Certificate>& localCertificate,
                  const std::shared_ptr<dht::crypto::PrivateKey>& localPrivateKey)
{
    if (!localCertificate || !localPrivateKey)
        throw std::runtime_error("Missing DTLS-SRTP local identity");
    if (remoteFingerprint.empty())
        throw std::runtime_error("Missing DTLS-SRTP remote fingerprint");
    if (localSetup != DtlsSetup::ACTIVE && localSetup != DtlsSetup::PASSIVE)
        throw std::runtime_error("Invalid negotiated DTLS setup role");

    std::mutex stateMutex;
    std::condition_variable stateCv;
    dhtnet::tls::TlsSessionState finalState {dhtnet::tls::TlsSessionState::NONE};
    bool completed = false;

    dhtnet::tls::TlsSession::TlsSessionCallbacks callbacks {
        [&](dhtnet::tls::TlsSessionState state) {
            if (state == dhtnet::tls::TlsSessionState::ESTABLISHED || state == dhtnet::tls::TlsSessionState::SHUTDOWN) {
                std::lock_guard lk(stateMutex);
                finalState = state;
                completed = true;
                stateCv.notify_one();
            }
        },
        {},
        {},
        [&](gnutls_session_t session) {
            return verifyRemoteFingerprint(session, remoteFingerprintHash, remoteFingerprint);
        },
    };

    dhtnet::tls::TlsParams params {
        {},
        nullptr,
        localCertificate,
        localPrivateKey,
        mediaDhParams(),
        mediaDtlsCertStore(),
        DTLS_SRTP_TIMEOUT,
        nullptr,
        Manager::instance().ioContext(),
        mediaDtlsLogger(),
        std::string(DTLS_SRTP_PROFILES),
    };

    auto transport = std::make_unique<IceSocketDtlsTransport>(rtpSocket, localSetup == DtlsSetup::ACTIVE);
    dhtnet::tls::TlsSession session(std::move(transport), params, callbacks, false);

    {
        std::unique_lock lk(stateMutex);
        if (!stateCv.wait_for(lk, DTLS_SRTP_TIMEOUT, [&] { return completed; }))
            throw std::runtime_error("DTLS-SRTP handshake timed out");
    }

    if (finalState != dhtnet::tls::TlsSessionState::ESTABLISHED)
        throw std::runtime_error("DTLS-SRTP handshake failed");

    const auto keyMaterial = session.srtpKeyMaterial();
    if (!keyMaterial)
        throw std::runtime_error("DTLS-SRTP key export failed");

    const auto* profileName = gnutls_srtp_get_profile_name(keyMaterial->profile);
    if (!profileName)
        throw std::runtime_error("Unsupported DTLS-SRTP profile");

    const auto localIsClient = session.isInitiator();
    if (localIsClient) {
        return {profileName,
                buildKeyInfo(keyMaterial->client_key, keyMaterial->client_salt),
                buildKeyInfo(keyMaterial->server_key, keyMaterial->server_salt)};
    }

    return {profileName,
            buildKeyInfo(keyMaterial->server_key, keyMaterial->server_salt),
            buildKeyInfo(keyMaterial->client_key, keyMaterial->client_salt)};
}

} // namespace jami