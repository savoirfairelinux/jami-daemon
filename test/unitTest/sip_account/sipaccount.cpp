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

#include "sip/sipaccount.h"
#include "test_runner.h"

#include <string>

namespace jami {
namespace test {

class SipAccountTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "sipaccount"; }

private:
    void trimsCipherListWithSeparators();

    CPPUNIT_TEST_SUITE(SipAccountTest);
    CPPUNIT_TEST(trimsCipherListWithSeparators);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(SipAccountTest, SipAccountTest::name());

void
SipAccountTest::trimsCipherListWithSeparators()
{
    pj_ssl_cipher cipher {};
    unsigned count = 1;
    CPPUNIT_ASSERT(pj_ssl_cipher_get_availables(&cipher, &count) == PJ_SUCCESS);
    CPPUNIT_ASSERT(count > 0);
    const auto* name = pj_ssl_cipher_name(cipher);
    CPPUNIT_ASSERT(name);
    const std::string cipherName(name);
    CPPUNIT_ASSERT(not cipherName.empty());
    CipherArray ciphers(1000 / cipherName.size(), cipher);

    SIPAccount::trimCiphers(ciphers);

    std::string cipherList;
    for (const auto& item : ciphers) {
        if (not cipherList.empty())
            cipherList += ':';
        cipherList += pj_ssl_cipher_name(item);
    }
    CPPUNIT_ASSERT(cipherList.size() <= 1000);
    CPPUNIT_ASSERT(cipherList.size() + 1 + cipherName.size() > 1000);
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::SipAccountTest::name());
