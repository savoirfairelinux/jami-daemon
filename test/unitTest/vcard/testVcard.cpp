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

#include "vcard.h"

#include <string>

#include "../../test_runner.h"

namespace jami {
namespace test {

class VCardTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "vcard"; }

private:
    void fitToSizeKeepsSmallVCardTest();
    void fitToSizeDropsPhotoTest();
    void fitToSizeDropsVCardTooBigWithoutPhotoTest();

    CPPUNIT_TEST_SUITE(VCardTest);
    CPPUNIT_TEST(fitToSizeKeepsSmallVCardTest);
    CPPUNIT_TEST(fitToSizeDropsPhotoTest);
    CPPUNIT_TEST(fitToSizeDropsVCardTooBigWithoutPhotoTest);
    CPPUNIT_TEST_SUITE_END();

    static constexpr size_t MAX_SIZE {16 * 1024};

    static std::string makeVCard(const std::string& photo, const std::string& note = {})
    {
        std::string vCard = "BEGIN:VCARD\nVERSION:2.1\nFN:Cyrille\n"
                            "TEL;other:ring:529e3a6708878bf57c1aa99e563f2a26f63a41e4\n";
        if (not note.empty())
            vCard += "NOTE:" + note + "\n";
        if (not photo.empty())
            vCard += "PHOTO;ENCODING=BASE64;TYPE=JPEG:" + photo + "\n";
        return vCard + "END:VCARD\n";
    }
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(VCardTest, VCardTest::name());

void
VCardTest::fitToSizeKeepsSmallVCardTest()
{
    auto vCard = makeVCard(std::string(4000, 'A'));
    CPPUNIT_ASSERT_EQUAL(vCard, vCard::utils::fitToSize(vCard, MAX_SIZE));
}

void
VCardTest::fitToSizeDropsPhotoTest()
{
    // A 350x350 avatar sent by jami-client-qt weighs about 38 KB in base64
    auto vCard = makeVCard(std::string(38000, 'A'));
    auto fitted = vCard::utils::fitToSize(vCard, MAX_SIZE);

    CPPUNIT_ASSERT(not fitted.empty());
    CPPUNIT_ASSERT(fitted.size() <= MAX_SIZE);
    auto details = vCard::utils::toMap(fitted);
    CPPUNIT_ASSERT_EQUAL(std::string("Cyrille"), details["FN"]);
    CPPUNIT_ASSERT_EQUAL(std::string("ring:529e3a6708878bf57c1aa99e563f2a26f63a41e4"), details["TEL;other"]);
    CPPUNIT_ASSERT(fitted.find("PHOTO") == std::string::npos);
}

void
VCardTest::fitToSizeDropsVCardTooBigWithoutPhotoTest()
{
    CPPUNIT_ASSERT(
        vCard::utils::fitToSize(makeVCard(std::string(4000, 'A'), std::string(20000, 'B')), MAX_SIZE).empty());
    CPPUNIT_ASSERT(vCard::utils::fitToSize(makeVCard({}, std::string(20000, 'B')), MAX_SIZE).empty());
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::VCardTest::name());
