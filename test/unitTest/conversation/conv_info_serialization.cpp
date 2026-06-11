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

#include "jamidht/conversation.h"

#include <msgpack.hpp>

#include <string>

namespace jami {
namespace test {

// Mimics the on-wire/on-disk layout produced by daemons released before the
// millisecond migration (time_t seconds, MSGPACK_DEFINE_MAP). Used to verify
// both directions of backward compatibility.
struct LegacyConvInfo
{
    std::string id {};
    time_t created {0};
    time_t removed {0};
    time_t erased {0};
    std::set<std::string> members;
    std::string lastDisplayed {};
    ConversationMode mode {0};

    MSGPACK_DEFINE_MAP(id, created, removed, erased, members, lastDisplayed, mode)
};

struct LegacyConversationRequest
{
    std::string conversationId;
    std::string from;
    std::map<std::string, std::string> metadatas;
    time_t received {0};
    time_t declined {0};

    MSGPACK_DEFINE_MAP(from, conversationId, metadatas, received, declined)
};

class ConvInfoSerializationTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "conv_info_serialization"; }

private:
    // Data written by an old daemon (seconds) is read as seconds * 1000
    void testConvInfoLegacyToNew();
    // Data written by a new daemon is readable by an old daemon (seconds keys)
    void testConvInfoNewToLegacy();
    // Millisecond precision survives a msgpack round-trip between new daemons
    void testConvInfoMsgpackRoundtrip();
    // JSON: ms keys preferred, legacy seconds-only JSON still readable
    void testConvInfoJson();
    // Same checks for ConversationRequest
    void testRequestLegacyToNew();
    void testRequestNewToLegacy();
    void testRequestMsgpackRoundtrip();
    // Client-facing map keeps exposing seconds
    void testRequestToMapStaysSeconds();
    // isRemoved() distinguishes events within the same second
    void testIsRemovedMsResolution();

    CPPUNIT_TEST_SUITE(ConvInfoSerializationTest);
    CPPUNIT_TEST(testConvInfoLegacyToNew);
    CPPUNIT_TEST(testConvInfoNewToLegacy);
    CPPUNIT_TEST(testConvInfoMsgpackRoundtrip);
    CPPUNIT_TEST(testConvInfoJson);
    CPPUNIT_TEST(testRequestLegacyToNew);
    CPPUNIT_TEST(testRequestNewToLegacy);
    CPPUNIT_TEST(testRequestMsgpackRoundtrip);
    CPPUNIT_TEST(testRequestToMapStaysSeconds);
    CPPUNIT_TEST(testIsRemovedMsResolution);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ConvInfoSerializationTest, ConvInfoSerializationTest::name());

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
ConvInfoSerializationTest::testConvInfoLegacyToNew()
{
    LegacyConvInfo legacy;
    legacy.id = "conv1";
    legacy.created = 1700000001;
    legacy.removed = 1700000002;
    legacy.erased = 1700000003;
    legacy.members = {"alice", "bob"};
    legacy.lastDisplayed = "commitId";
    legacy.mode = ConversationMode::ONE_TO_ONE;

    auto info = repack<ConvInfo>(legacy);
    CPPUNIT_ASSERT_EQUAL(std::string("conv1"), info.id);
    CPPUNIT_ASSERT_EQUAL(int64_t(1700000001000), toMillisecondsSinceEpoch(info.created));
    CPPUNIT_ASSERT_EQUAL(int64_t(1700000002000), toMillisecondsSinceEpoch(info.removed));
    CPPUNIT_ASSERT_EQUAL(int64_t(1700000003000), toMillisecondsSinceEpoch(info.erased));
    CPPUNIT_ASSERT(info.members == legacy.members);
    CPPUNIT_ASSERT_EQUAL(std::string("commitId"), info.lastDisplayed);
    CPPUNIT_ASSERT(info.mode == ConversationMode::ONE_TO_ONE);
}

void
ConvInfoSerializationTest::testConvInfoNewToLegacy()
{
    ConvInfo info(std::string("conv1"));
    info.created = timePointFromMilliseconds(1700000001123);
    info.removed = timePointFromMilliseconds(1700000002456);
    info.erased = timePointFromMilliseconds(1700000003789);
    info.members = {"alice"};
    info.lastDisplayed = "commitId";
    info.mode = ConversationMode::INVITES_ONLY;

    auto legacy = repack<LegacyConvInfo>(info);
    CPPUNIT_ASSERT_EQUAL(std::string("conv1"), legacy.id);
    CPPUNIT_ASSERT_EQUAL(time_t(1700000001), legacy.created);
    CPPUNIT_ASSERT_EQUAL(time_t(1700000002), legacy.removed);
    CPPUNIT_ASSERT_EQUAL(time_t(1700000003), legacy.erased);
    CPPUNIT_ASSERT(legacy.members == info.members);
    CPPUNIT_ASSERT_EQUAL(std::string("commitId"), legacy.lastDisplayed);
    CPPUNIT_ASSERT(legacy.mode == ConversationMode::INVITES_ONLY);
}

void
ConvInfoSerializationTest::testConvInfoMsgpackRoundtrip()
{
    ConvInfo info(std::string("conv1"));
    info.created = timePointFromMilliseconds(1700000001123);
    info.removed = timePointFromMilliseconds(1700000001124);
    info.members = {"alice", "bob"};

    auto out = repack<ConvInfo>(info);
    CPPUNIT_ASSERT(out.created == info.created);
    CPPUNIT_ASSERT(out.removed == info.removed);
    CPPUNIT_ASSERT(out.erased == info.erased);
    CPPUNIT_ASSERT(out.members == info.members);
}

void
ConvInfoSerializationTest::testConvInfoJson()
{
    ConvInfo info(std::string("conv1"));
    info.created = timePointFromMilliseconds(1700000001123);
    info.removed = timePointFromMilliseconds(1700000002456);
    info.members = {"alice"};

    // Round-trip via JSON preserves ms
    ConvInfo fromJson(info.toJson());
    CPPUNIT_ASSERT(fromJson.created == info.created);
    CPPUNIT_ASSERT(fromJson.removed == info.removed);

    // Legacy JSON (seconds only, e.g. an old account archive) still loads
    Json::Value legacy;
    legacy[ConversationMapKeys::ID] = "conv1";
    legacy[ConversationMapKeys::CREATED] = Json::Int64(1700000001);
    legacy[ConversationMapKeys::REMOVED] = Json::Int64(1700000002);
    ConvInfo fromLegacy(legacy);
    CPPUNIT_ASSERT_EQUAL(int64_t(1700000001000), toMillisecondsSinceEpoch(fromLegacy.created));
    CPPUNIT_ASSERT_EQUAL(int64_t(1700000002000), toMillisecondsSinceEpoch(fromLegacy.removed));
    CPPUNIT_ASSERT_EQUAL(int64_t(0), toMillisecondsSinceEpoch(fromLegacy.erased));
}

void
ConvInfoSerializationTest::testRequestLegacyToNew()
{
    LegacyConversationRequest legacy;
    legacy.conversationId = "conv1";
    legacy.from = "alice";
    legacy.metadatas = {{"mode", "0"}};
    legacy.received = 1700000001;
    legacy.declined = 1700000002;

    auto req = repack<ConversationRequest>(legacy);
    CPPUNIT_ASSERT_EQUAL(std::string("conv1"), req.conversationId);
    CPPUNIT_ASSERT_EQUAL(std::string("alice"), req.from);
    CPPUNIT_ASSERT(req.metadatas == legacy.metadatas);
    CPPUNIT_ASSERT_EQUAL(int64_t(1700000001000), toMillisecondsSinceEpoch(req.received));
    CPPUNIT_ASSERT_EQUAL(int64_t(1700000002000), toMillisecondsSinceEpoch(req.declined));
}

void
ConvInfoSerializationTest::testRequestNewToLegacy()
{
    ConversationRequest req;
    req.conversationId = "conv1";
    req.from = "alice";
    req.received = timePointFromMilliseconds(1700000001123);
    req.declined = timePointFromMilliseconds(1700000002456);

    auto legacy = repack<LegacyConversationRequest>(req);
    CPPUNIT_ASSERT_EQUAL(std::string("conv1"), legacy.conversationId);
    CPPUNIT_ASSERT_EQUAL(std::string("alice"), legacy.from);
    CPPUNIT_ASSERT_EQUAL(time_t(1700000001), legacy.received);
    CPPUNIT_ASSERT_EQUAL(time_t(1700000002), legacy.declined);
}

void
ConvInfoSerializationTest::testRequestMsgpackRoundtrip()
{
    ConversationRequest req;
    req.conversationId = "conv1";
    req.from = "alice";
    req.metadatas = {{"title", "test"}};
    req.received = timePointFromMilliseconds(1700000001123);

    auto out = repack<ConversationRequest>(req);
    CPPUNIT_ASSERT(out.received == req.received);
    CPPUNIT_ASSERT(out.declined == req.declined);
    CPPUNIT_ASSERT(out.metadatas == req.metadatas);
}

void
ConvInfoSerializationTest::testRequestToMapStaysSeconds()
{
    ConversationRequest req;
    req.conversationId = "conv1";
    req.from = "alice";
    req.received = timePointFromMilliseconds(1700000001123);
    req.declined = timePointFromMilliseconds(1700000002456);

    auto map = req.toMap();
    CPPUNIT_ASSERT_EQUAL(std::string("1700000001"), map.at(ConversationMapKeys::RECEIVED));
    CPPUNIT_ASSERT_EQUAL(std::string("1700000002"), map.at(ConversationMapKeys::DECLINED));
}

void
ConvInfoSerializationTest::testIsRemovedMsResolution()
{
    ConvInfo info(std::string("conv1"));
    info.created = timePointFromMilliseconds(1700000001500);
    // Removed 1 ms later, within the same second: now detectable
    info.removed = timePointFromMilliseconds(1700000001501);
    CPPUNIT_ASSERT(info.isRemoved());
    // Re-created 1 ms after removal
    info.created = timePointFromMilliseconds(1700000001502);
    CPPUNIT_ASSERT(!info.isRemoved());
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::ConvInfoSerializationTest::name());
