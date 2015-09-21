/*
 *  Copyright (C) 2004-2015 Savoir-faire Linux Inc.
 *
 *  Author: Alexandre Lision <alexandre.lision@savoirfairelinux.com>
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

/*
 * ebail - 2015/02/18
 * unit test is based on old SDP manager code
 * this test is disabled for the moment
 * */
#if 0

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "tlstest.h"
#include "account.h"
#include "test_utils.h"
#include "logger.h"

#include "sip/tlsvalidator.h"

namespace ring { namespace test {

void TlsTest::testKey()
{
    TITLE();

    const char *validKey = WORKSPACE "tlsSample/keyonly.pem";
    const char *validCertWithKey = WORKSPACE "tlsSample/certwithkey.pem";
    const char *corruptedKey = WORKSPACE "tlsSample/corruptedkey.pem";

    CPPUNIT_ASSERT(containsPrivateKey(validKey) == 0);

    CPPUNIT_ASSERT(containsPrivateKey(validCertWithKey) == 0);

    CPPUNIT_ASSERT(containsPrivateKey(corruptedKey) != 0);
}

void TlsTest::testCertificate()
{
    TITLE();

    const char *validCa = WORKSPACE "tlsSample/ca.crt";
    const char *validCertificate = WORKSPACE "tlsSample/cert.crt";
    const char *fakeCertificate = WORKSPACE "tlsSample/fake.crt";
    const char *expiredCertificate = WORKSPACE "tlsSample/expired.crt";

    CPPUNIT_ASSERT(certificateIsValid(NULL, validCa) == 0);

    CPPUNIT_ASSERT(certificateIsValid(validCa, validCertificate) == 0);

    // This is a png
    CPPUNIT_ASSERT(certificateIsValid(NULL, fakeCertificate) != 0);

    // This would need a CA to be valid
    CPPUNIT_ASSERT(certificateIsValid(NULL, validCertificate) != 0);

    // This is an invalid CA
    CPPUNIT_ASSERT(certificateIsValid(validCertificate, validCertificate) != 0);

    // This certificate is expired
    CPPUNIT_ASSERT(certificateIsValid(NULL, expiredCertificate) != 0);
}

}} // namespace ring::test
#endif
