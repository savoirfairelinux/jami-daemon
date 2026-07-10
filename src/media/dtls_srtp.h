#pragma once

#include "media_codec.h"

#include <opendht/crypto.h>

#include <memory>
#include <string>
#include <string_view>

namespace dhtnet {
class IceSocket;
}

namespace jami {

struct DtlsSrtpContext
{
    std::string suite {};
    std::string outboundKeyInfo {};
    std::string inboundKeyInfo {};
};

dht::crypto::Identity generateDtlsSrtpIdentity();

std::string getDtlsFingerprint(const dht::crypto::Certificate& certificate,
                               std::string_view hash = "SHA-256");

// RFC 5764 §5.1.2: returns true if the packet must be demultiplexed to the
// DTLS stack (first byte in [20..63]) rather than SRTP/STUN.
bool isDtlsPacket(const uint8_t* buf, size_t len);

// Runs a blocking DTLS-SRTP handshake on the given ICE socket. The optional
// abort flag makes the wait cancellable from another thread (e.g. call
// teardown), in which case a runtime_error is thrown.
DtlsSrtpContext negotiateDtlsSrtp(dhtnet::IceSocket& rtpSocket,
                                  DtlsSetup localSetup,
                                  std::string_view remoteFingerprintHash,
                                  std::string_view remoteFingerprint,
                                  const std::shared_ptr<dht::crypto::Certificate>& localCertificate,
                                  const std::shared_ptr<dht::crypto::PrivateKey>& localPrivateKey,
                                  const std::shared_ptr<std::atomic_bool>& abort = {});

} // namespace jami
