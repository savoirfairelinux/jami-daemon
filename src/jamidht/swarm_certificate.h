#pragma once

#include "git_utils.h"

#include <opendht/crypto.h>
#include <gnutls/x509.h>

#include <initializer_list>
#include <string_view>

namespace jami {

inline bool
isCertificateOfDevice(const dht::crypto::Certificate& cert, std::string_view deviceId)
{
    return cert.getLongId().to_view() == deviceId;
}

inline bool
isDeviceOfMember(const dht::crypto::Certificate& deviceCert,
                 const dht::crypto::Certificate& memberCert,
                 std::string_view memberUri)
{
    if (not deviceCert.cert or not memberCert.cert)
        return false;
    if (memberCert.getId().toString() != memberUri)
        return false;
    unsigned result = 0;
    auto err = gnutls_x509_crt_verify(deviceCert.cert, &memberCert.cert, 1, GNUTLS_VERIFY_DISABLE_TIME_CHECKS, &result);
    return err == GNUTLS_E_SUCCESS and not(result & GNUTLS_CERT_INVALID);
}

struct DeviceIdentity
{
    // Its issuer is the verified member certificate, never an embedded chain.
    std::shared_ptr<dht::crypto::Certificate> device;
    std::shared_ptr<dht::crypto::Certificate> member;
    std::string deviceId;
    std::string memberId;
    std::string deviceData;
    std::string memberData;
};

/**
 * Reads devices/<deviceId>.crt and the certificate of the member it belongs to from the first of
 * memberDirs holding it, and returns them only if the member key issued the device certificate.
 * An empty memberId uses the device certificate's issuer UID.
 */
std::optional<DeviceIdentity> readDeviceIdentity(git_repository* repo,
                                                 const git_tree* tree,
                                                 std::string_view deviceId,
                                                 std::string_view memberId,
                                                 std::initializer_list<std::string_view> memberDirs,
                                                 size_t directoryLimit = git::UNLIMITED,
                                                 size_t certificateLimit = git::UNLIMITED);

} // namespace jami
