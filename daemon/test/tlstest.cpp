/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "tlstest.h"
#include "account.h"
#include "test_utils.h"
#include "logger.h"

#include "sip/tlsvalidator.h"

void TlsTest::testKey()
{
    TITLE();

    const char *validKey = WORKSPACE "tlsSample/keyonly.pem";
    const char *validCertWithKey = WORKSPACE "tlsSample/certwithkey.pem";
    const char *corruptedKey = WORKSPACE "tlsSample/corruptedkey.pem";

    TlsValidator validKeyValidator("",validKey);
    TlsValidator validCertWithKeyValidator(validCertWithKey,"");
    // TlsValidator corruptedKeyValidator("",corruptedKey);

    CPPUNIT_ASSERT(validKeyValidator.exist().first == TlsValidator::ChecksValues::FAILED);

    CPPUNIT_ASSERT(validCertWithKeyValidator.embedPrivateKey().first == TlsValidator::ChecksValues::FAILED);

    // TODO write a similar test, the current code will trigger CertificateChecks::MISSING_CERTIFICATE first
    // and quit
    // CPPUNIT_ASSERT(containsPrivateKey(corruptedKey) == TlsValidator::ChecksValues::FAILED);
}

void TlsTest::testCertificate()
{
    TITLE();

    const char *validCa = WORKSPACE "tlsSample/ca.crt";
    const char *validCertificate = WORKSPACE "tlsSample/cert.crt";
    const char *fakeCertificate = WORKSPACE "tlsSample/fake.crt";
    const char *expiredCertificate = WORKSPACE "tlsSample/expired.crt";

    TlsValidator validCaValidator(validCa,"");
    TlsValidator validCertificateValidator(validCertificate,"");
    TlsValidator fakeCertificateValidator(fakeCertificate,"");
    TlsValidator expiredCertificateValidator(expiredCertificate,"");

    // A valid certificate need a CA, this one doesn't have one
    CPPUNIT_ASSERT(validCertificateValidator.isValid(true) == true);

    validCertificateValidator.setCaTlsValidator(validCaValidator);

    // A valid certificate need a CA, this one have one
    CPPUNIT_ASSERT(validCertificateValidator.isValid(true) == true);

    // This is a png
    CPPUNIT_ASSERT(fakeCertificateValidator.validCertificate().first == TlsValidator::ChecksValues::FAILED);
    fakeCertificateValidator.setCaTlsValidator(validCaValidator);
    CPPUNIT_ASSERT(fakeCertificateValidator.validCertificate().first == TlsValidator::ChecksValues::FAILED);

    // This would need a CA to be valid
    // CPPUNIT_ASSERT(certificateIsValid(NULL, validCertificate) != 0);

    // This is an invalid CA
    // CPPUNIT_ASSERT(certificateIsValid(validCertificate, validCertificate) != 0);

    // This certificate is expired
    CPPUNIT_ASSERT(expiredCertificateValidator.notExpired().first == TlsValidator::ChecksValues::FAILED);
}

void TlsTest::testHostname()
{
    TITLE();

    const char *correctUrl = "www.savoirfairelinux.com";
    const char *wrongUrl = "www..com";

    CPPUNIT_ASSERT(TlsValidator::verifyHostnameCertificate(correctUrl, 443) == 0);

    CPPUNIT_ASSERT(TlsValidator::verifyHostnameCertificate(correctUrl, 80 ) != 0);
    CPPUNIT_ASSERT(TlsValidator::verifyHostnameCertificate(correctUrl, 0  ) != 0);
    CPPUNIT_ASSERT(TlsValidator::verifyHostnameCertificate(wrongUrl  , 443) != 0);
    CPPUNIT_ASSERT(TlsValidator::verifyHostnameCertificate(NULL      , 443) != 0);
}
