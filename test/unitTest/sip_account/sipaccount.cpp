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

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

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
    const auto cipherNameLength = std::size_t {10};
    const auto maxCipherStringLength = cipherNameLength * 3;
    const std::vector<std::string_view> cipherNames {"0123456789",
                                                     "0123456789",
                                                     "0123456789",
                                                     "0123456789"};

    const auto trimmedCount =
        sip_utils::trimmedCipherNameCountForMaxStringLength(cipherNames, maxCipherStringLength);

    CPPUNIT_ASSERT_EQUAL(std::size_t {2}, trimmedCount);
    const auto serializedLength = trimmedCount * cipherNameLength + trimmedCount - 1;
    CPPUNIT_ASSERT(serializedLength <= maxCipherStringLength);
    CPPUNIT_ASSERT(serializedLength + cipherNameLength + 1 > maxCipherStringLength);
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::SipAccountTest::name());
