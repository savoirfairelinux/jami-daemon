/*
 *  Copyright (C) 2026 Savoir-faire Linux Inc.
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

#include "../../test_runner.h"

#include <string>
#include <vector>

namespace jami {
namespace test {

class SipTlsTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "sip_tls"; }

private:
    void trims_cipher_names_with_delimiters();

    CPPUNIT_TEST_SUITE(SipTlsTest);
    CPPUNIT_TEST(trims_cipher_names_with_delimiters);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(SipTlsTest, SipTlsTest::name());

void
SipTlsTest::trims_cipher_names_with_delimiters()
{
    const std::string first(500, 'A');
    const std::string fittingSecond(499, 'B');
    const std::string overflowingSecond(500, 'C');

    CPPUNIT_ASSERT_EQUAL(std::size_t(2),
                         SIPAccount::trimmedCipherCountForNames({first, fittingSecond}));
    CPPUNIT_ASSERT_EQUAL(std::size_t(1),
                         SIPAccount::trimmedCipherCountForNames({first, overflowingSecond}));
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::SipTlsTest::name());
