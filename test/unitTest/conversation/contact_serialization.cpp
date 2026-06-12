/*
 * Copyright (C) 2004-2026 Savoir-faire Linux Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "../../test_runner.h"

#include "jamidht/jami_contact.h"

#include <msgpack.hpp>

#include <ctime>
#include <string>

namespace jami {
namespace test {

// Mimics the on-wire/on-disk layout produced by daemons released before the
// millisecond migration (time_t seconds, MSGPACK_DEFINE_MAP). Used to verify
// both directions of backward compatibility.
struct LegacyContact
{
    time_t added {0};
    time_t removed {0};
    bool confirmed {false};
    bool banned {false};
    std::string conversationId {};

    MSGPACK_DEFINE_MAP(added, removed, confirmed, banned, conversationId)
};

struct LegacyTrustRequest
{
    std::shared_ptr<dht::crypto::PublicKey> device;
    std::string conversationId;
    time_t received {0};
    std::vector<uint8_t> payload;

    MSGPACK_DEFINE_MAP(device, conversationId, received, payload)
};

class ContactSerializationTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "contact_serialization"; }

private:
    // Data written by an old daemon (seconds) is read as seconds * 1000
    void testContactLegacyToNew();
    // Data written by a new daemon is readable by an old daemon (seconds keys)
    void testContactNewToLegacy();
    // Millisecond precision survives a msgpack round-trip between new daemons
    void testContactMsgpackRoundtrip();
    // JSON: ms keys preferred, legacy seconds-only JSON still readable
    void testContactJson();
    // Client-facing map keeps exposing seconds
    void testContactToMapStaysSeconds();
    // isActive() distinguishes events within the same second
    void testContactIsActiveMsResolution();
    // Same backward-compatibility checks for TrustRequest
    void testTrustRequestLegacyToNew();
    void testTrustRequestNewToLegacy();
    void testTrustRequestMsgpackRoundtrip();

    CPPUNIT_TEST_SUITE(ContactSerializationTest);
    CPPUNIT_TEST(testContactLegacyToNew);
    CPPUNIT_TEST(testContactNewToLegacy);
    CPPUNIT_TEST(testContactMsgpackRoundtrip);
    CPPUNIT_TEST(testContactJson);
    CPPUNIT_TEST(testContactToMapStaysSeconds);
    CPPUNIT_TEST(testContactIsActiveMsResolution);
    CPPUNIT_TEST(testTrustRequestLegacyToNew);
    CPPUNIT_TEST(testTrustRequestNewToLegacy);
    CPPUNIT_TEST(testTrustRequestMsgpackRoundtrip);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ContactSerializationTest, ContactSerializationTest::name());

template<typename Out, typename In>
static Out
repack(const In& in)
{
    msgpack::sbuffer buffer;
    msgpack::pack(buffer, in);
    msgpack::object_handle oh = msgpack::unpack(buffer.data(), buffer.size());
    Out out;
    oh.get().convert(out);
    return out;
}

void
ContactSerializationTest::testContactLegacyToNew()
{
    LegacyContact legacy;
    legacy.added = 1700000001;
    legacy.removed = 1700000002;
    legacy.confirmed = true;
    legacy.banned = false;
    legacy.conversationId = "conv1";

    auto contact = repack<Contact>(legacy);
    CPPUNIT_ASSERT_EQUAL(int64_t(1700000001000), toMillisecondsSinceEpoch(contact.added));
    CPPUNIT_ASSERT_EQUAL(int64_t(1700000002000), toMillisecondsSinceEpoch(contact.removed));
    CPPUNIT_ASSERT(contact.confirmed);
    CPPUNIT_ASSERT(!contact.banned);
    CPPUNIT_ASSERT_EQUAL(std::string("conv1"), contact.conversationId);
}

void
ContactSerializationTest::testContactNewToLegacy()
{
    Contact contact;
    contact.added = timePointFromMilliseconds(1700000001123);
    contact.removed = timePointFromMilliseconds(1700000002456);
    contact.confirmed = true;
    contact.banned = false;
    contact.conversationId = "conv1";

    auto legacy = repack<LegacyContact>(contact);
    CPPUNIT_ASSERT_EQUAL(time_t(1700000001), legacy.added);
    CPPUNIT_ASSERT_EQUAL(time_t(1700000002), legacy.removed);
    CPPUNIT_ASSERT(legacy.confirmed);
    CPPUNIT_ASSERT(!legacy.banned);
    CPPUNIT_ASSERT_EQUAL(std::string("conv1"), legacy.conversationId);
}

void
ContactSerializationTest::testContactMsgpackRoundtrip()
{
    Contact contact;
    contact.added = timePointFromMilliseconds(1700000001123);
    // Removed 1 ms later, within the same second
    contact.removed = timePointFromMilliseconds(1700000001124);
    contact.confirmed = true;
    contact.banned = true;
    contact.conversationId = "conv1";

    auto out = repack<Contact>(contact);
    CPPUNIT_ASSERT(out.added == contact.added);
    CPPUNIT_ASSERT(out.removed == contact.removed);
    CPPUNIT_ASSERT_EQUAL(contact.confirmed, out.confirmed);
    CPPUNIT_ASSERT_EQUAL(contact.banned, out.banned);
    CPPUNIT_ASSERT_EQUAL(contact.conversationId, out.conversationId);
}

void
ContactSerializationTest::testContactJson()
{
    Contact contact;
    contact.added = timePointFromMilliseconds(1700000001123);
    contact.removed = timePointFromMilliseconds(1700000002456);
    contact.confirmed = true;
    contact.conversationId = "conv1";

    // Round-trip via JSON preserves ms
    Contact fromJson(contact.toJson());
    CPPUNIT_ASSERT(fromJson.added == contact.added);
    CPPUNIT_ASSERT(fromJson.removed == contact.removed);
    CPPUNIT_ASSERT_EQUAL(contact.confirmed, fromJson.confirmed);
    CPPUNIT_ASSERT_EQUAL(contact.conversationId, fromJson.conversationId);

    // Legacy JSON (seconds only, e.g. an old account archive) still loads
    Json::Value legacy;
    legacy[ContactMapKeys::ADDED] = Json::Int64(1700000001);
    legacy[ContactMapKeys::REMOVED] = Json::Int64(1700000002);
    legacy[ContactMapKeys::CONVERSATIONID] = "conv1";
    Contact fromLegacy(legacy);
    CPPUNIT_ASSERT_EQUAL(int64_t(1700000001000), toMillisecondsSinceEpoch(fromLegacy.added));
    CPPUNIT_ASSERT_EQUAL(int64_t(1700000002000), toMillisecondsSinceEpoch(fromLegacy.removed));
}

void
ContactSerializationTest::testContactToMapStaysSeconds()
{
    Contact contact;
    contact.added = timePointFromMilliseconds(1700000002456);
    contact.removed = timePointFromMilliseconds(1700000001123);
    contact.confirmed = true;

    auto map = contact.toMap();
    CPPUNIT_ASSERT_EQUAL(std::string("1700000002"), map.at("added"));
    CPPUNIT_ASSERT_EQUAL(std::string("1700000001"), map.at("removed"));
}

void
ContactSerializationTest::testContactIsActiveMsResolution()
{
    Contact contact;
    contact.added = timePointFromMilliseconds(1700000001500);
    // Removed 1 ms later, within the same second: now detectable
    contact.removed = timePointFromMilliseconds(1700000001501);
    CPPUNIT_ASSERT(!contact.isActive());
    // Re-added 1 ms after removal
    contact.added = timePointFromMilliseconds(1700000001502);
    CPPUNIT_ASSERT(contact.isActive());
}

void
ContactSerializationTest::testTrustRequestLegacyToNew()
{
    LegacyTrustRequest legacy;
    legacy.conversationId = "conv1";
    legacy.received = 1700000001;
    legacy.payload = {1, 2, 3};

    auto req = repack<TrustRequest>(legacy);
    CPPUNIT_ASSERT_EQUAL(std::string("conv1"), req.conversationId);
    CPPUNIT_ASSERT_EQUAL(int64_t(1700000001000), toMillisecondsSinceEpoch(req.received));
    CPPUNIT_ASSERT(req.payload == legacy.payload);
}

void
ContactSerializationTest::testTrustRequestNewToLegacy()
{
    TrustRequest req;
    req.conversationId = "conv1";
    req.received = timePointFromMilliseconds(1700000001123);
    req.payload = {4, 5, 6};

    auto legacy = repack<LegacyTrustRequest>(req);
    CPPUNIT_ASSERT_EQUAL(std::string("conv1"), legacy.conversationId);
    CPPUNIT_ASSERT_EQUAL(time_t(1700000001), legacy.received);
    CPPUNIT_ASSERT(legacy.payload == req.payload);
}

void
ContactSerializationTest::testTrustRequestMsgpackRoundtrip()
{
    TrustRequest req;
    req.conversationId = "conv1";
    req.received = timePointFromMilliseconds(1700000001123);
    req.payload = {7, 8, 9};

    auto out = repack<TrustRequest>(req);
    CPPUNIT_ASSERT(out.received == req.received);
    CPPUNIT_ASSERT_EQUAL(req.conversationId, out.conversationId);
    CPPUNIT_ASSERT(out.payload == req.payload);
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::ContactSerializationTest::name());
