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

#include "common.h"
#include "test_runner.h"
#include "manager.h"
#include "jami.h"
#include "jamidht/jamiaccount.h"
#include "logger.h"

#include <dhtnet/certstore.h>

namespace jami {
namespace test {

class CertStoreTest : public CppUnit::TestFixture
{
public:
    CertStoreTest()
    {
        // Init daemon
        libjami::init(
            libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
    }
    ~CertStoreTest() { libjami::fini(); }
    static std::string name() { return "certstore"; }
    void setUp();
    void tearDown();

    std::string aliceId;
    std::string bobId;

private:
    void trustStoreTest();
    void getCertificateWithSplitted();
    void testBannedParent();

    CPPUNIT_TEST_SUITE(CertStoreTest);
    CPPUNIT_TEST(trustStoreTest);
    CPPUNIT_TEST(getCertificateWithSplitted);
    CPPUNIT_TEST(testBannedParent);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(CertStoreTest, CertStoreTest::name());

void
CertStoreTest::setUp()
{
    auto actors = load_actors_and_wait_for_announcement("actors/alice-bob.yml");
    aliceId = actors["alice"];
    bobId = actors["bob"];
}

void
CertStoreTest::tearDown()
{
    wait_for_removal_of({aliceId, bobId});
}

void
CertStoreTest::trustStoreTest()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);

    auto ca = dht::crypto::generateIdentity("test CA");
    auto account = dht::crypto::generateIdentity("test account", ca, 4096, true);
    auto device = dht::crypto::generateIdentity("test device", account);
    auto device2 = dht::crypto::generateIdentity("test device 2", account);
    auto storeSize = aliceAccount->certStore().getPinnedCertificates().size();
    auto id = ca.second->getId().toString();
    auto pinned = aliceAccount->certStore().getPinnedCertificates();
    CPPUNIT_ASSERT(std::find_if(pinned.begin(), pinned.end(), [&](auto v) { return v == id; })
                   == pinned.end());

    // Test certificate status
    auto certAllowed = aliceAccount->accountManager()->getCertificatesByStatus(
        dhtnet::tls::TrustStore::PermissionStatus::ALLOWED);
    CPPUNIT_ASSERT(
        std::find_if(certAllowed.begin(), certAllowed.end(), [&](auto v) { return v == id; })
        == certAllowed.end());
    CPPUNIT_ASSERT(aliceAccount->accountManager()->getCertificateStatus(id)
                   == dhtnet::tls::TrustStore::PermissionStatus::UNDEFINED);
    aliceAccount->setCertificateStatus(ca.second, dhtnet::tls::TrustStore::PermissionStatus::ALLOWED);
    certAllowed = aliceAccount->accountManager()->getCertificatesByStatus(
        dhtnet::tls::TrustStore::PermissionStatus::ALLOWED);
    CPPUNIT_ASSERT(
        std::find_if(certAllowed.begin(), certAllowed.end(), [&](auto v) { return v == id; })
        != certAllowed.end());
    CPPUNIT_ASSERT(aliceAccount->accountManager()->getCertificateStatus(id)
                   == dhtnet::tls::TrustStore::PermissionStatus::ALLOWED);
    aliceAccount->setCertificateStatus(ca.second, dhtnet::tls::TrustStore::PermissionStatus::UNDEFINED);
    CPPUNIT_ASSERT(aliceAccount->accountManager()->getCertificateStatus(id)
                   == dhtnet::tls::TrustStore::PermissionStatus::UNDEFINED);
    aliceAccount->setCertificateStatus(ca.second, dhtnet::tls::TrustStore::PermissionStatus::ALLOWED);
    CPPUNIT_ASSERT(aliceAccount->accountManager()->getCertificateStatus(id)
                   == dhtnet::tls::TrustStore::PermissionStatus::ALLOWED);

    // Test getPinnedCertificates
    pinned = aliceAccount->certStore().getPinnedCertificates();
    CPPUNIT_ASSERT(pinned.size() == storeSize + 2 /* account + device */);
    CPPUNIT_ASSERT(std::find_if(pinned.begin(), pinned.end(), [&](auto v) { return v == id; })
                   != pinned.end());

    // Test findCertificateByUID & findIssuer
    CPPUNIT_ASSERT(!aliceAccount->certStore().findCertificateByUID("NON_EXISTING_ID"));
    auto cert = aliceAccount->certStore().findCertificateByUID(id);
    CPPUNIT_ASSERT(cert);
    auto issuer = aliceAccount->certStore().findIssuer(cert);
    CPPUNIT_ASSERT(issuer);
    CPPUNIT_ASSERT(issuer->getId().toString() == id);

    // Test is allowed
    CPPUNIT_ASSERT(aliceAccount->accountManager()->isAllowed(*ca.second));
    CPPUNIT_ASSERT(aliceAccount->accountManager()->isAllowed(*account.second));
    CPPUNIT_ASSERT(aliceAccount->accountManager()->isAllowed(*device.second));

    // Ban device
    aliceAccount->setCertificateStatus(device.second, dhtnet::tls::TrustStore::PermissionStatus::BANNED);
    CPPUNIT_ASSERT(aliceAccount->accountManager()->getCertificateStatus(device.second->getId().toString())
                   == dhtnet::tls::TrustStore::PermissionStatus::BANNED);
    CPPUNIT_ASSERT(aliceAccount->accountManager()->getCertificateStatus(id)
                   == dhtnet::tls::TrustStore::PermissionStatus::ALLOWED);

    CPPUNIT_ASSERT(aliceAccount->accountManager()->isAllowed(*ca.second));
    CPPUNIT_ASSERT(aliceAccount->accountManager()->isAllowed(*account.second));
    CPPUNIT_ASSERT(not aliceAccount->accountManager()->isAllowed(*device.second));

    // Ban account
    aliceAccount->setCertificateStatus(account.second, dhtnet::tls::TrustStore::PermissionStatus::BANNED);
    CPPUNIT_ASSERT(aliceAccount->accountManager()->getCertificateStatus(account.second->getId().toString())
                   == dhtnet::tls::TrustStore::PermissionStatus::BANNED);
    CPPUNIT_ASSERT(aliceAccount->accountManager()->isAllowed(*ca.second));
    CPPUNIT_ASSERT(not aliceAccount->accountManager()->isAllowed(*account.second));
    CPPUNIT_ASSERT(not aliceAccount->accountManager()->isAllowed(*device2.second));

    // Unban account
    aliceAccount->setCertificateStatus(account.second,
                                    dhtnet::tls::TrustStore::PermissionStatus::ALLOWED);
    CPPUNIT_ASSERT(aliceAccount->accountManager()->getCertificateStatus(account.second->getId().toString())
                   == dhtnet::tls::TrustStore::PermissionStatus::ALLOWED);
    CPPUNIT_ASSERT(aliceAccount->accountManager()->isAllowed(*ca.second));
    CPPUNIT_ASSERT(aliceAccount->accountManager()->isAllowed(*account.second));
    CPPUNIT_ASSERT(aliceAccount->accountManager()->isAllowed(*device2.second));

    // Ban CA
    aliceAccount->setCertificateStatus(ca.second, dhtnet::tls::TrustStore::PermissionStatus::BANNED);
    CPPUNIT_ASSERT(aliceAccount->accountManager()->getCertificateStatus(ca.second->getId().toString())
                   == dhtnet::tls::TrustStore::PermissionStatus::BANNED);
    CPPUNIT_ASSERT(not aliceAccount->accountManager()->isAllowed(*ca.second));
    CPPUNIT_ASSERT(not aliceAccount->accountManager()->isAllowed(*account.second));
    CPPUNIT_ASSERT(not aliceAccount->accountManager()->isAllowed(*device2.second));

    aliceAccount->setCertificateStatus(ca.second, dhtnet::tls::TrustStore::PermissionStatus::BANNED);
    CPPUNIT_ASSERT(aliceAccount->accountManager()->getCertificateStatus(ca.second->getId().toString())
                   == dhtnet::tls::TrustStore::PermissionStatus::BANNED);

    // Test unpin
    aliceAccount->certStore().unpinCertificate(id);
    pinned = aliceAccount->certStore().getPinnedCertificates();
    CPPUNIT_ASSERT(std::find_if(pinned.begin(), pinned.end(), [&](auto v) { return v == id; })
                   == pinned.end());

    // Test statusToStr
    /*CPPUNIT_ASSERT(strcmp(dhtnet::tls::statusToStr(dhtnet::tls::TrustStatus::TRUSTED),
                          libdhtnet::Certificate::TrustStatus::TRUSTED)
                   == 0);
    CPPUNIT_ASSERT(strcmp(dhtnet::tls::statusToStr(dhtnet::tls::TrustStatus::UNTRUSTED),
                          libdhtnet::Certificate::TrustStatus::UNTRUSTED)
                   == 0);*/
}

void
CertStoreTest::getCertificateWithSplitted()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto ca = dht::crypto::generateIdentity("test CA");
    auto account = dht::crypto::generateIdentity("test account", ca, 4096, true);
    auto device = dht::crypto::generateIdentity("test device", account);

    auto caCert = std::make_shared<dht::crypto::Certificate>(ca.second->toString(false));
    auto accountCert = std::make_shared<dht::crypto::Certificate>(account.second->toString(false));
    auto devicePartialCert = std::make_shared<dht::crypto::Certificate>(
        device.second->toString(false));

    aliceAccount->certStore().pinCertificate(caCert);
    aliceAccount->certStore().pinCertificate(accountCert);
    aliceAccount->certStore().pinCertificate(devicePartialCert);

    auto fullCert = aliceAccount->certStore().getCertificate(device.second->getId().toString());
    CPPUNIT_ASSERT(fullCert->issuer && fullCert->issuer->getUID() == accountCert->getUID());
    CPPUNIT_ASSERT(fullCert->issuer->issuer
                   && fullCert->issuer->issuer->getUID() == caCert->getUID());
}

void
CertStoreTest::testBannedParent()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);

    auto ca = dht::crypto::generateIdentity("test CA");
    auto account = dht::crypto::generateIdentity("test account", ca, 4096, true);
    auto device = dht::crypto::generateIdentity("test device", account);
    auto device2 = dht::crypto::generateIdentity("test device 2", account);
    auto id = ca.second->getId().toString();
    auto pinned = aliceAccount->certStore().getPinnedCertificates();
    CPPUNIT_ASSERT(std::find_if(pinned.begin(), pinned.end(), [&](auto v) { return v == id; })
                   == pinned.end());

    // Ban account
    aliceAccount->setCertificateStatus(account.second, dhtnet::tls::TrustStore::PermissionStatus::BANNED);
    CPPUNIT_ASSERT(aliceAccount->accountManager()->getCertificateStatus(account.second->getId().toString())
                   == dhtnet::tls::TrustStore::PermissionStatus::BANNED);
    CPPUNIT_ASSERT(not aliceAccount->accountManager()->isAllowed(*account.second));
    CPPUNIT_ASSERT(not aliceAccount->accountManager()->isAllowed(*device2.second));
    CPPUNIT_ASSERT(not aliceAccount->accountManager()->isAllowed(*device.second));
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::CertStoreTest::name());
