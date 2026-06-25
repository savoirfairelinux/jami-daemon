/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <opendht/crypto.h>
#include <dhtnet/certstore.h>

#include <gnutls/gnutls.h>

#include <filesystem>
#include <memory>
#include <mutex>

#include "test_runner.h"

namespace jami {
namespace test {

/*
 * Regression test for organization-CA based trust.
 *
 * Managed (JAMS) accounts have their account certificate issued by an
 * organization CA shared by every member. JamiAccount::updateTrustedCa() pins
 * that CA as ALLOWED in the peer TrustStore so that any device whose
 * certificate chains up to it is accepted for incoming connections (e.g. to
 * call a rendezvous point) even when it is not a contact and public incoming is
 * disabled.
 *
 * These tests exercise the exact dhtnet TrustStore mechanism the daemon relies
 * on, without requiring any network or DHT activity.
 */
class TrustStoreTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "trust"; }

    void setUp() override;
    void tearDown() override;

private:
    void testCaPinAllowsSameOrgPeer();
    void testBannedTakesPrecedence();
    void testUnpinRevokesTrust();

    CPPUNIT_TEST_SUITE(TrustStoreTest);
    CPPUNIT_TEST(testCaPinAllowsSameOrgPeer);
    CPPUNIT_TEST(testBannedTakesPrecedence);
    CPPUNIT_TEST(testUnpinRevokesTrust);
    CPPUNIT_TEST_SUITE_END();

    using PermissionStatus = dhtnet::tls::TrustStore::PermissionStatus;

    std::filesystem::path testDir_;
    // Organization PKI: a self-signed CA that signs an account certificate,
    // which in turn signs a device certificate (device -> account -> org CA).
    dht::crypto::Identity ca_;
    dht::crypto::Identity peerAccount_;
    dht::crypto::Identity peerDevice_;
    std::shared_ptr<dht::crypto::Certificate> peerDeviceCert_;
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(TrustStoreTest, TrustStoreTest::name());

void
TrustStoreTest::setUp()
{
    static std::once_flag gnutlsInit;
    std::call_once(gnutlsInit, [] { gnutls_global_init(); });

    testDir_ = std::filesystem::temp_directory_path() / "jami_trust_unittest";
    std::error_code ec;
    std::filesystem::remove_all(testDir_, ec);

    // Build the organization certificate chain, mirroring the JAMS topology.
    ca_ = dht::crypto::generateEcIdentity("org CA", {}, true);
    peerAccount_ = dht::crypto::generateEcIdentity("peer account", ca_, true);
    peerDevice_ = dht::crypto::generateEcIdentity("peer device", peerAccount_, false);

    // Link the issuer chain so the TrustStore can walk device -> account -> CA.
    peerAccount_.second->issuer = ca_.second;
    peerDevice_.second->issuer = peerAccount_.second;
    peerDeviceCert_ = peerDevice_.second;
}

void
TrustStoreTest::tearDown()
{
    std::error_code ec;
    std::filesystem::remove_all(testDir_, ec);
}

void
TrustStoreTest::testCaPinAllowsSameOrgPeer()
{
    dhtnet::tls::CertificateStore certStore(testDir_ / "certs", nullptr);
    dhtnet::tls::TrustStore trustStore(certStore);

    // With public incoming disabled and nothing pinned, a non-contact peer from
    // the same organization is refused. This is the regression: such peers were
    // silently discarded once dhtPublicInCalls defaulted to false.
    CPPUNIT_ASSERT(!trustStore.isAllowed(*peerDeviceCert_, false));

    // Pinning the organization CA as ALLOWED is exactly what
    // JamiAccount::updateTrustedCa() does when allowPeersFromTrusted is on.
    CPPUNIT_ASSERT(trustStore.setCertificateStatus(ca_.second, PermissionStatus::ALLOWED, false));

    // Any device whose certificate chains up to that CA is now allowed, even
    // though it is not a contact and public incoming is still disabled. The
    // chain is still cryptographically verified by the TrustStore.
    CPPUNIT_ASSERT(trustStore.isAllowed(*peerDeviceCert_, false));
}

void
TrustStoreTest::testBannedTakesPrecedence()
{
    dhtnet::tls::CertificateStore certStore(testDir_ / "certs", nullptr);
    dhtnet::tls::TrustStore trustStore(certStore);

    CPPUNIT_ASSERT(trustStore.setCertificateStatus(ca_.second, PermissionStatus::ALLOWED, false));
    CPPUNIT_ASSERT(trustStore.isAllowed(*peerDeviceCert_, false));

    // Explicitly banning the peer's account certificate must override the trust
    // granted by the organization CA.
    CPPUNIT_ASSERT(trustStore.setCertificateStatus(peerAccount_.second, PermissionStatus::BANNED, false));
    CPPUNIT_ASSERT(!trustStore.isAllowed(*peerDeviceCert_, false));
}

void
TrustStoreTest::testUnpinRevokesTrust()
{
    dhtnet::tls::CertificateStore certStore(testDir_ / "certs", nullptr);
    dhtnet::tls::TrustStore trustStore(certStore);

    CPPUNIT_ASSERT(trustStore.setCertificateStatus(ca_.second, PermissionStatus::ALLOWED, false));
    CPPUNIT_ASSERT(trustStore.isAllowed(*peerDeviceCert_, false));

    // Toggling allowPeersFromTrusted off unpins the CA (status UNDEFINED), which
    // must drop the trust again.
    CPPUNIT_ASSERT(trustStore.setCertificateStatus(ca_.second, PermissionStatus::UNDEFINED, false));
    CPPUNIT_ASSERT(!trustStore.isAllowed(*peerDeviceCert_, false));
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::TrustStoreTest::name());
