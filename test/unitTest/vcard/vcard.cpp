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

#include "test_runner.h"
#include "vcard.h"

namespace jami {
namespace test {

class VCardTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "vcard"; }

private:
    void testProfileWithoutPhoto();
    void testProfileWithSinglePhoto();

    CPPUNIT_TEST_SUITE(VCardTest);
    CPPUNIT_TEST(testProfileWithoutPhoto);
    CPPUNIT_TEST(testProfileWithSinglePhoto);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(VCardTest, VCardTest::name());

void
VCardTest::testProfileWithoutPhoto()
{
    auto profile = vCard::utils::initVcard();
    profile[std::string(vCard::Property::FORMATTED_NAME)] = "Managed account";

    const auto serialized = vCard::utils::toString(profile);
    CPPUNIT_ASSERT(serialized.find("\nPHOTO") == std::string::npos);
}

void
VCardTest::testProfileWithSinglePhoto()
{
    const std::string photo = "YXZhdGFy";
    for (const auto property : {vCard::Property::PHOTO, vCard::Property::PHOTO_JPEG, vCard::Property::PHOTO_PNG}) {
        auto profile = vCard::utils::initVcard();
        profile[std::string(vCard::Property::FORMATTED_NAME)] = "Managed account";
        profile[std::string(property)] = photo;

        const auto savedProfile = vCard::utils::toMap(vCard::utils::toString(profile));
        std::size_t photoCount = 0;
        for (const auto& [key, value] : savedProfile) {
            if (key.starts_with(vCard::Property::PHOTO))
                ++photoCount;
        }
        CPPUNIT_ASSERT_EQUAL_MESSAGE(std::string(property), std::size_t {1}, photoCount);
        CPPUNIT_ASSERT_EQUAL(photo, savedProfile.at(std::string(property)));
    }
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::VCardTest::name());
