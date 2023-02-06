/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: florian Wiesweg <florian.wiesweg@campus.tu-berlin.de>
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
 */

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "test_runner.h"

#include "connectivity/security/certstore.h"

namespace jami {
namespace test {

class CertStoreTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "certstore"; }

private:
    void trustStoreTest();
    void getCertificateWithSplitted();

    CPPUNIT_TEST_SUITE(CertStoreTest);
    CPPUNIT_TEST(trustStoreTest);
    CPPUNIT_TEST(getCertificateWithSplitted);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(CertStoreTest, CertStoreTest::name());

void
CertStoreTest::trustStoreTest()
{
    jami::tls::TrustStore trustStore;
    auto& certStore = jami::tls::CertificateStore::instance();

    auto ca = dht::crypto::generateIdentity("test CA");
    auto account = dht::crypto::generateIdentity("test account", ca, 4096, true);
    auto device = dht::crypto::generateIdentity("test device", account);
    auto device2 = dht::crypto::generateIdentity("test device 2", account);
    auto storeSize = certStore.getPinnedCertificates().size();
    auto id = ca.second->getId().toString();
    auto pinned = certStore.getPinnedCertificates();
    CPPUNIT_ASSERT(std::find_if(pinned.begin(), pinned.end(), [&](auto v) { return v == id; })
                   == pinned.end());

    // Test certificate status
    auto certAllowed = trustStore.getCertificatesByStatus(
        jami::tls::TrustStore::PermissionStatus::ALLOWED);
    CPPUNIT_ASSERT(
        std::find_if(certAllowed.begin(), certAllowed.end(), [&](auto v) { return v == id; })
        == certAllowed.end());
    CPPUNIT_ASSERT(trustStore.getCertificateStatus(id)
                   == jami::tls::TrustStore::PermissionStatus::UNDEFINED);
    trustStore.setCertificateStatus(ca.second, jami::tls::TrustStore::PermissionStatus::ALLOWED);
    certAllowed = trustStore.getCertificatesByStatus(
        jami::tls::TrustStore::PermissionStatus::ALLOWED);
    CPPUNIT_ASSERT(
        std::find_if(certAllowed.begin(), certAllowed.end(), [&](auto v) { return v == id; })
        != certAllowed.end());
    CPPUNIT_ASSERT(trustStore.getCertificateStatus(id)
                   == jami::tls::TrustStore::PermissionStatus::ALLOWED);
    trustStore.setCertificateStatus(ca.second, jami::tls::TrustStore::PermissionStatus::UNDEFINED);
    CPPUNIT_ASSERT(trustStore.getCertificateStatus(id)
                   == jami::tls::TrustStore::PermissionStatus::UNDEFINED);
    trustStore.setCertificateStatus(ca.second, jami::tls::TrustStore::PermissionStatus::ALLOWED);
    CPPUNIT_ASSERT(trustStore.getCertificateStatus(id)
                   == jami::tls::TrustStore::PermissionStatus::ALLOWED);

    // Test getPinnedCertificates
    pinned = certStore.getPinnedCertificates();
    CPPUNIT_ASSERT(pinned.size() == storeSize + 2 /* account + device */);
    CPPUNIT_ASSERT(std::find_if(pinned.begin(), pinned.end(), [&](auto v) { return v == id; })
                   != pinned.end());

    // Test findCertificateByUID & findIssuer
    CPPUNIT_ASSERT(!certStore.findCertificateByUID("NON_EXISTING_ID"));
    auto cert = certStore.findCertificateByUID(id);
    CPPUNIT_ASSERT(cert);
    auto issuer = certStore.findIssuer(cert);
    CPPUNIT_ASSERT(issuer);
    CPPUNIT_ASSERT(issuer->getId().toString() == id);

    // Test is allowed
    CPPUNIT_ASSERT(trustStore.isAllowed(*ca.second));
    CPPUNIT_ASSERT(trustStore.isAllowed(*account.second));
    CPPUNIT_ASSERT(trustStore.isAllowed(*device.second));

    // Ban device
    trustStore.setCertificateStatus(device.second, jami::tls::TrustStore::PermissionStatus::BANNED);
    CPPUNIT_ASSERT(trustStore.getCertificateStatus(device.second->getId().toString())
                   == jami::tls::TrustStore::PermissionStatus::BANNED);
    CPPUNIT_ASSERT(trustStore.getCertificateStatus(id)
                   == jami::tls::TrustStore::PermissionStatus::ALLOWED);

    CPPUNIT_ASSERT(trustStore.isAllowed(*ca.second));
    CPPUNIT_ASSERT(trustStore.isAllowed(*account.second));
    CPPUNIT_ASSERT(not trustStore.isAllowed(*device.second));

    // Ban account
    trustStore.setCertificateStatus(account.second, jami::tls::TrustStore::PermissionStatus::BANNED);
    CPPUNIT_ASSERT(trustStore.getCertificateStatus(account.second->getId().toString())
                   == jami::tls::TrustStore::PermissionStatus::BANNED);
    CPPUNIT_ASSERT(trustStore.isAllowed(*ca.second));
    CPPUNIT_ASSERT(not trustStore.isAllowed(*account.second));
    CPPUNIT_ASSERT(not trustStore.isAllowed(*device2.second));

    // Unban account
    trustStore.setCertificateStatus(account.second,
                                    jami::tls::TrustStore::PermissionStatus::ALLOWED);
    CPPUNIT_ASSERT(trustStore.getCertificateStatus(account.second->getId().toString())
                   == jami::tls::TrustStore::PermissionStatus::ALLOWED);
    CPPUNIT_ASSERT(trustStore.isAllowed(*ca.second));
    CPPUNIT_ASSERT(trustStore.isAllowed(*account.second));
    CPPUNIT_ASSERT(trustStore.isAllowed(*device2.second));

    // Ban CA
    trustStore.setCertificateStatus(ca.second, jami::tls::TrustStore::PermissionStatus::BANNED);
    CPPUNIT_ASSERT(trustStore.getCertificateStatus(ca.second->getId().toString())
                   == jami::tls::TrustStore::PermissionStatus::BANNED);
    CPPUNIT_ASSERT(not trustStore.isAllowed(*ca.second));
    CPPUNIT_ASSERT(not trustStore.isAllowed(*account.second));
    CPPUNIT_ASSERT(not trustStore.isAllowed(*device2.second));

    trustStore.setCertificateStatus(ca.second, jami::tls::TrustStore::PermissionStatus::BANNED);
    CPPUNIT_ASSERT(trustStore.getCertificateStatus(ca.second->getId().toString())
                   == jami::tls::TrustStore::PermissionStatus::BANNED);

    // Test unpin
    certStore.unpinCertificate(id);
    pinned = certStore.getPinnedCertificates();
    CPPUNIT_ASSERT(std::find_if(pinned.begin(), pinned.end(), [&](auto v) { return v == id; })
                   == pinned.end());

    // Test statusToStr
    CPPUNIT_ASSERT(strcmp(jami::tls::statusToStr(jami::tls::TrustStatus::TRUSTED),
                          libjami::Certificate::TrustStatus::TRUSTED)
                   == 0);
    CPPUNIT_ASSERT(strcmp(jami::tls::statusToStr(jami::tls::TrustStatus::UNTRUSTED),
                          libjami::Certificate::TrustStatus::UNTRUSTED)
                   == 0);
}

void
CertStoreTest::getCertificateWithSplitted()
{
    jami::tls::TrustStore trustStore;
    auto& certStore = jami::tls::CertificateStore::instance();

    auto ca = dht::crypto::generateIdentity("test CA");
    auto account = dht::crypto::generateIdentity("test account", ca, 4096, true);
    auto device = dht::crypto::generateIdentity("test device", account);

    auto caCert = std::make_shared<dht::crypto::Certificate>(ca.second->toString(false));
    auto accountCert = std::make_shared<dht::crypto::Certificate>(account.second->toString(false));
    auto devicePartialCert = std::make_shared<dht::crypto::Certificate>(
        device.second->toString(false));

    certStore.pinCertificate(caCert);
    certStore.pinCertificate(accountCert);
    certStore.pinCertificate(devicePartialCert);

    auto fullCert = certStore.getCertificate(device.second->getId().toString());
    CPPUNIT_ASSERT(fullCert->issuer && fullCert->issuer->getUID() == accountCert->getUID());
    CPPUNIT_ASSERT(fullCert->issuer->issuer
                   && fullCert->issuer->issuer->getUID() == caCert->getUID());
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::CertStoreTest::name());
