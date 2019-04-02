/*
 *  Copyright (C) 2011-2019 Savoir-faire Linux Inc.
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

#include "security/certstore.h"

namespace jami { namespace test {

class CertStoreTest : public CppUnit::TestFixture {
public:
    static std::string name() { return "certstore"; }

private:
    void trustStoreTest();

    CPPUNIT_TEST_SUITE(CertStoreTest);
    CPPUNIT_TEST(trustStoreTest);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(CertStoreTest, CertStoreTest::name());

void
CertStoreTest::trustStoreTest()
{
    jami::tls::TrustStore trustStore;

    auto ca = dht::crypto::generateIdentity("test CA");
    auto account = dht::crypto::generateIdentity("test account", ca, 4096, true);
    auto device = dht::crypto::generateIdentity("test device", account);
    auto device2 = dht::crypto::generateIdentity("test device 2", account);

    CPPUNIT_ASSERT(trustStore.getCertificateStatus(ca.second->getId().toString()) == jami::tls::TrustStore::PermissionStatus::UNDEFINED);
    trustStore.setCertificateStatus(ca.second, jami::tls::TrustStore::PermissionStatus::ALLOWED);
    CPPUNIT_ASSERT(trustStore.getCertificateStatus(ca.second->getId().toString()) == jami::tls::TrustStore::PermissionStatus::ALLOWED);
    trustStore.setCertificateStatus(ca.second, jami::tls::TrustStore::PermissionStatus::UNDEFINED);
    CPPUNIT_ASSERT(trustStore.getCertificateStatus(ca.second->getId().toString()) == jami::tls::TrustStore::PermissionStatus::UNDEFINED);
    trustStore.setCertificateStatus(ca.second, jami::tls::TrustStore::PermissionStatus::ALLOWED);
    CPPUNIT_ASSERT(trustStore.getCertificateStatus(ca.second->getId().toString()) == jami::tls::TrustStore::PermissionStatus::ALLOWED);

    CPPUNIT_ASSERT(trustStore.isAllowed(*ca.second));
    CPPUNIT_ASSERT(trustStore.isAllowed(*account.second));
    CPPUNIT_ASSERT(trustStore.isAllowed(*device.second));

    // Ban device
    trustStore.setCertificateStatus(device.second, jami::tls::TrustStore::PermissionStatus::BANNED);
    CPPUNIT_ASSERT(trustStore.getCertificateStatus(device.second->getId().toString()) == jami::tls::TrustStore::PermissionStatus::BANNED);
    CPPUNIT_ASSERT(trustStore.getCertificateStatus(ca.second->getId().toString()) == jami::tls::TrustStore::PermissionStatus::ALLOWED);

    CPPUNIT_ASSERT(trustStore.isAllowed(*ca.second));
    CPPUNIT_ASSERT(trustStore.isAllowed(*account.second));
    CPPUNIT_ASSERT(not trustStore.isAllowed(*device.second));

    // Ban account
    trustStore.setCertificateStatus(account.second, jami::tls::TrustStore::PermissionStatus::BANNED);
    CPPUNIT_ASSERT(trustStore.getCertificateStatus(account.second->getId().toString()) == jami::tls::TrustStore::PermissionStatus::BANNED);
    CPPUNIT_ASSERT(trustStore.isAllowed(*ca.second));
    CPPUNIT_ASSERT(not trustStore.isAllowed(*account.second));
    CPPUNIT_ASSERT(not trustStore.isAllowed(*device2.second));

    // Unban account
    trustStore.setCertificateStatus(account.second, jami::tls::TrustStore::PermissionStatus::ALLOWED);
    CPPUNIT_ASSERT(trustStore.getCertificateStatus(account.second->getId().toString()) == jami::tls::TrustStore::PermissionStatus::ALLOWED);
    CPPUNIT_ASSERT(trustStore.isAllowed(*ca.second));
    CPPUNIT_ASSERT(trustStore.isAllowed(*account.second));
    CPPUNIT_ASSERT(trustStore.isAllowed(*device2.second));

    // Ban CA
    trustStore.setCertificateStatus(ca.second, jami::tls::TrustStore::PermissionStatus::BANNED);
    CPPUNIT_ASSERT(trustStore.getCertificateStatus(ca.second->getId().toString()) == jami::tls::TrustStore::PermissionStatus::BANNED);
    CPPUNIT_ASSERT(not trustStore.isAllowed(*ca.second));
    CPPUNIT_ASSERT(not trustStore.isAllowed(*account.second));
    CPPUNIT_ASSERT(not trustStore.isAllowed(*device2.second));

    trustStore.setCertificateStatus(ca.second, jami::tls::TrustStore::PermissionStatus::BANNED);
    CPPUNIT_ASSERT(trustStore.getCertificateStatus(ca.second->getId().toString()) == jami::tls::TrustStore::PermissionStatus::BANNED);
}

}} // namespace jami::test

RING_TEST_RUNNER(jami::test::CertStoreTest::name());
