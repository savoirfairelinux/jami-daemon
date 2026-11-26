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

// knownDevices entries used to be a plain std::pair<std::string, uint64_t>,
// serialized by msgpack as a two-element array [name, lastSyncSeconds].
struct LegacyKnownDevice
{
    std::string name;
    int64_t lastSyncSeconds {0};

    MSGPACK_DEFINE(name, lastSyncSeconds)
};

// Same layout, but keeping the unsigned type the old daemon actually used: this is
// what reproduces the time_point::min() wraparound found in real user data.
struct LegacyKnownDeviceUnsigned
{
    std::string name;
    uint64_t lastSyncSeconds {0};

    MSGPACK_DEFINE(name, lastSyncSeconds)
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
    void testContactMetadata();
    void testContactMergeProperties();
    void testContactMergeScenarios();
    // Same backward-compatibility checks for TrustRequest
    void testTrustRequestLegacyToNew();
    void testTrustRequestNewToLegacy();
    void testTrustRequestMsgpackRoundtrip();
    // "invited" (sender-embedded invite timestamp) survives a roundtrip and defaults to unset
    // ({}) when reading a payload from a daemon that predates this field.
    void testTrustRequestInvitedRoundtrip();
    void testTrustRequestInvitedMissingDefaultsUnset();
    // knownDevices: legacy [name, seconds] entries, including the time_point::min()
    // value that old daemons wrapped around into an out-of-range uint64
    void testKnownDeviceLegacyToNew();
    void testKnownDeviceLegacyNeverSynced();
    // A single unreadable entry must not discard every other known device
    void testKnownDeviceMapSurvivesBadEntry();
    void testKnownDeviceMsgpackRoundtrip();

    CPPUNIT_TEST_SUITE(ContactSerializationTest);
    CPPUNIT_TEST(testContactLegacyToNew);
    CPPUNIT_TEST(testContactNewToLegacy);
    CPPUNIT_TEST(testContactMsgpackRoundtrip);
    CPPUNIT_TEST(testContactJson);
    CPPUNIT_TEST(testContactToMapStaysSeconds);
    CPPUNIT_TEST(testContactIsActiveMsResolution);
    CPPUNIT_TEST(testContactMetadata);
    CPPUNIT_TEST(testContactMergeProperties);
    CPPUNIT_TEST(testContactMergeScenarios);
    CPPUNIT_TEST(testTrustRequestLegacyToNew);
    CPPUNIT_TEST(testTrustRequestNewToLegacy);
    CPPUNIT_TEST(testTrustRequestMsgpackRoundtrip);
    CPPUNIT_TEST(testTrustRequestInvitedRoundtrip);
    CPPUNIT_TEST(testTrustRequestInvitedMissingDefaultsUnset);
    CPPUNIT_TEST(testKnownDeviceLegacyToNew);
    CPPUNIT_TEST(testKnownDeviceLegacyNeverSynced);
    CPPUNIT_TEST(testKnownDeviceMapSurvivesBadEntry);
    CPPUNIT_TEST(testKnownDeviceMsgpackRoundtrip);
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
ContactSerializationTest::testContactMetadata()
{
    Contact contact;
    contact.added = timePointFromMilliseconds(2000);
    contact.metadata.entries = {{"label", {2000, std::string(64, 'a'), {0x80, 0, 0xff}}},
                                {"empty", {2000, std::string(64, 'a'), {}}}};
    CPPUNIT_ASSERT(repack<Contact>(contact).metadata == contact.metadata);
    msgpack::zone zone;
    msgpack::object object(contact, zone);
    CPPUNIT_ASSERT(object.as<Contact>().metadata == contact.metadata);
    CPPUNIT_ASSERT(Contact(contact.toJson()).metadata == contact.metadata);
    CPPUNIT_ASSERT(repack<Contact>(LegacyContact {}).metadata.entries.empty());
    CPPUNIT_ASSERT(Contact(Json::Value {}).metadata.entries.empty());
    CPPUNIT_ASSERT_EQUAL(time_t(2), repack<LegacyContact>(contact).added);
    CPPUNIT_ASSERT(!contact.toMap().contains("metadata"));
}

namespace {

const std::string writerA(64, 'a');
const std::string writerB(64, 'b');

Contact
makeContact(int64_t addedMs,
            int64_t removedMs,
            bool confirmed = false,
            bool banned = false,
            std::string conversationId = {},
            std::map<std::string, MetadataEntry> metadata = {})
{
    Contact contact;
    contact.added = timePointFromMilliseconds(addedMs);
    contact.removed = timePointFromMilliseconds(removedMs);
    contact.confirmed = confirmed;
    contact.banned = banned;
    contact.conversationId = std::move(conversationId);
    contact.metadata.entries = std::move(metadata);
    return contact;
}

} // namespace

void
ContactSerializationTest::testContactMergeProperties()
{
    // Copies of one contact's history as different devices may hold it. Copies of the same
    // addition share its conversation, as conversationId ties are resolved in favour of the local copy.
    const std::vector<Contact> copies {
        makeContact(1000, 0, false, false, "first", {{"k", {1100, writerA, {1}}}}),
        makeContact(1000, 0, true, false, "first", {{"k", {1200, writerB, {2}}}, {"j", {1100, writerB, {3}}}}),
        makeContact(1000, 2000),
        makeContact(1000, 2000, false, true),
        makeContact(2000, 0, true, false, "tied", {{"m", {2100, writerA, {5}}}}),
        makeContact(3000, 0, false, false, "second", {{"k", {3100, writerA, {4}}}}),
        makeContact(0, 2500, false, true),
    };
    for (const auto& a : copies) {
        CPPUNIT_ASSERT(a.isValid());
        CPPUNIT_ASSERT(Contact::merge(a, a) == a);
        for (const auto& b : copies) {
            const auto ab = Contact::merge(a, b);
            CPPUNIT_ASSERT(ab.isValid());
            CPPUNIT_ASSERT(ab == Contact::merge(b, a));
            CPPUNIT_ASSERT(Contact::merge(ab, b) == ab);
            for (const auto& c : copies)
                CPPUNIT_ASSERT(Contact::merge(ab, c) == Contact::merge(a, Contact::merge(b, c)));
        }
    }
}

void
ContactSerializationTest::testContactMergeScenarios()
{
    const auto active = makeContact(1000, 0, true, false, "conv", {{"k", {1100, writerA, {1}}}});

    // Removal discards metadata and confirmation, and a stale copy cannot restore them.
    const auto removed = Contact::merge(active, makeContact(0, 2000));
    CPPUNIT_ASSERT(!removed.isActive());
    CPPUNIT_ASSERT(removed.metadata.entries.empty());
    CPPUNIT_ASSERT(!removed.confirmed);
    CPPUNIT_ASSERT(removed.conversationId.empty());
    CPPUNIT_ASSERT(Contact::merge(removed, active) == removed);

    // Re-adding starts empty: registers of the earlier addition never enter the new one.
    const auto readded = makeContact(3000, 0, false, false, "new", {{"j", {3100, writerB, {2}}}});
    CPPUNIT_ASSERT(Contact::merge(active, readded) == readded);
    CPPUNIT_ASSERT(Contact::merge(removed, readded) == readded); // The older removal time is reset.
    CPPUNIT_ASSERT(Contact::merge(Contact::merge(removed, readded), active).metadata == readded.metadata);

    // Within an addition, confirmation is sticky and registers are merged.
    const auto other = makeContact(1000, 0, false, false, "conv", {{"j", {1200, writerB, {3}}}});
    const auto merged = Contact::merge(other, active);
    CPPUNIT_ASSERT(merged.confirmed);
    CPPUNIT_ASSERT_EQUAL(size_t(2), merged.metadata.entries.size());

    // A ban wins over a plain removal at the same time; an addition at the same time as a removal loses.
    CPPUNIT_ASSERT(Contact::merge(makeContact(1000, 2000), makeContact(1000, 2000, false, true)).isBanned());
    CPPUNIT_ASSERT(!Contact::merge(makeContact(2000, 0), makeContact(1000, 2000)).isActive());

    // The local copy wins conversationId ties and keeps its registers if they cannot be merged.
    auto conflicting = active;
    conflicting.conversationId = "other";
    conflicting.metadata.entries.at("k").value = {9};
    const auto local = Contact::merge(active, conflicting);
    CPPUNIT_ASSERT_EQUAL(active.conversationId, local.conversationId);
    CPPUNIT_ASSERT(local.metadata == active.metadata);

    // Each invariant violation is detected and repaired by merging a contact with itself,
    // which keeps the latest event and clears the offending field.
    auto stale = [](Contact c, auto&& change) {
        change(c);
        return c;
    };
    const std::vector<Contact> invalid {
        stale(removed, [&](Contact& c) { c.confirmed = true; }),
        stale(removed, [&](Contact& c) { c.metadata = active.metadata; }),
        stale(removed, [&](Contact& c) { c.conversationId = "conv"; }),
        stale(active, [&](Contact& c) { c.banned = true; }),
        stale(active, [&](Contact& c) { c.removed = timePointFromMilliseconds(500); }),
    };
    for (const auto& contact : invalid) {
        CPPUNIT_ASSERT(contact.isMergeable() && !contact.isValid());
        const auto normalized = Contact::merge(contact, contact);
        CPPUNIT_ASSERT(normalized.isValid());
        CPPUNIT_ASSERT(normalized.added == contact.added && normalized.isActive() == contact.isActive());
    }
    CPPUNIT_ASSERT(Contact::merge(invalid[0], invalid[0]) == removed);
    CPPUNIT_ASSERT(Contact::merge(invalid[3], invalid[3]) == active);
    CPPUNIT_ASSERT(Contact::merge(invalid[4], invalid[4]) == active);
    CPPUNIT_ASSERT(!Contact {}.isMergeable());
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

void
ContactSerializationTest::testTrustRequestInvitedRoundtrip()
{
    TrustRequest req;
    req.conversationId = "conv1";
    req.received = timePointFromMilliseconds(1700000001123);
    req.invited = timePointFromMilliseconds(1699999999000);
    req.payload = {7, 8, 9};

    auto out = repack<TrustRequest>(req);
    CPPUNIT_ASSERT(out.received == req.received);
    CPPUNIT_ASSERT(out.invited == req.invited);
    CPPUNIT_ASSERT_EQUAL(req.conversationId, out.conversationId);
    CPPUNIT_ASSERT(out.payload == req.payload);
}

void
ContactSerializationTest::testTrustRequestInvitedMissingDefaultsUnset()
{
    // A daemon that predates the "invited" field never wrote an invitedMs key: reading its
    // data must not spuriously invent a non-zero invite timestamp.
    LegacyTrustRequest legacy;
    legacy.conversationId = "conv1";
    legacy.received = 1700000001;
    legacy.payload = {1, 2, 3};

    auto req = repack<TrustRequest>(legacy);
    CPPUNIT_ASSERT(req.invited == TimePoint {});
}

void
ContactSerializationTest::testKnownDeviceLegacyToNew()
{
    // A regular legacy entry: [name, lastSyncSeconds] read as seconds * 1000.
    LegacyKnownDevice legacy {"Device A", 1700000001};

    auto device = repack<KnownDeviceData>(legacy);
    CPPUNIT_ASSERT_EQUAL(std::string("Device A"), device.name);
    CPPUNIT_ASSERT_EQUAL(int64_t(1700000001) * 1000, device.lastSyncMs);
}

void
ContactSerializationTest::testKnownDeviceLegacyNeverSynced()
{
    // Devices that were never synced carry time_point::min(). Daemons predating the
    // millisecond migration ran that through time_t into a uint64_t field, so the
    // negative second count wrapped to a value far beyond INT64_MAX. Reading it must
    // not throw (msgpack::type_error derives from std::bad_cast) and must degrade to
    // "never synced" rather than an absurd timestamp.
    constexpr uint64_t WRAPPED_NEVER_SYNCED = 18446744064486179580ULL;
    LegacyKnownDeviceUnsigned legacy {"Device B", WRAPPED_NEVER_SYNCED};

    KnownDeviceData device;
    CPPUNIT_ASSERT_NO_THROW(device = repack<KnownDeviceData>(legacy));
    CPPUNIT_ASSERT_EQUAL(std::string("Device B"), device.name);
    CPPUNIT_ASSERT_EQUAL(int64_t(0), device.lastSyncMs);
}

void
ContactSerializationTest::testKnownDeviceMapSurvivesBadEntry()
{
    // The whole knownDevices file used to be discarded when a single entry failed to
    // parse, silently dropping every known device of the account.
    constexpr uint64_t WRAPPED_NEVER_SYNCED = 18446744064486179580ULL;
    std::map<std::string, LegacyKnownDeviceUnsigned> legacy;
    legacy.emplace("deviceA", LegacyKnownDeviceUnsigned {"Device A", 1700000001});
    legacy.emplace("deviceB", LegacyKnownDeviceUnsigned {"Device B", WRAPPED_NEVER_SYNCED});

    std::map<std::string, KnownDeviceData> devices;
    CPPUNIT_ASSERT_NO_THROW((devices = repack<std::map<std::string, KnownDeviceData>>(legacy)));
    CPPUNIT_ASSERT_EQUAL(size_t(2), devices.size());
    CPPUNIT_ASSERT_EQUAL(int64_t(1700000001) * 1000, devices.at("deviceA").lastSyncMs);
    CPPUNIT_ASSERT_EQUAL(int64_t(0), devices.at("deviceB").lastSyncMs);
}

void
ContactSerializationTest::testKnownDeviceMsgpackRoundtrip()
{
    KnownDeviceData device {"Device C", 1700000001123};

    auto out = repack<KnownDeviceData>(device);
    CPPUNIT_ASSERT_EQUAL(device.name, out.name);
    CPPUNIT_ASSERT_EQUAL(device.lastSyncMs, out.lastSyncMs);
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::ContactSerializationTest::name());
