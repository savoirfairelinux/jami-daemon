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

#include "jamidht/commit_message.h"

#include <optional>
#include <random>
#include <string>

namespace jami {
namespace test {

class CommitMessageTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "commit_message"; }

private:
    // toString() followed by fromString() preserves every field
    void testToStringFromStringRoundtrip();
    // fromString() followed by toString() reproduces the original JSON string
    void testFromStringToStringRoundtrip();
    // Non-JSON / malformed input returns std::nullopt
    void testFromStringInvalidJson();
    // Example messages are parsed with the correct field values
    void testExampleMessages();
    // A default-constructed CommitMessage has the expected initial values
    void testDefaultValues();

    CPPUNIT_TEST_SUITE(CommitMessageTest);
    CPPUNIT_TEST(testToStringFromStringRoundtrip);
    CPPUNIT_TEST(testFromStringToStringRoundtrip);
    CPPUNIT_TEST(testFromStringInvalidJson);
    CPPUNIT_TEST(testExampleMessages);
    CPPUNIT_TEST(testDefaultValues);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(CommitMessageTest, CommitMessageTest::name());

static bool
messagesEqual(const CommitMessage& a, const CommitMessage& b)
{
    return a.type == b.type && a.body == b.body && a.replyTo == b.replyTo && a.reactTo == b.reactTo
           && a.editedId == b.editedId && a.action == b.action && a.uri == b.uri && a.device == b.device
           && a.confId == b.confId && a.to == b.to && a.reason == b.reason && a.duration == b.duration && a.tid == b.tid
           && a.displayName == b.displayName && a.totalSize == b.totalSize && a.sha3sum == b.sha3sum && a.mode == b.mode
           && a.invited == b.invited;
}

void
CommitMessageTest::testDefaultValues()
{
    CommitMessage msg;
    CPPUNIT_ASSERT(msg.type.empty());
    CPPUNIT_ASSERT(msg.body.empty());
    CPPUNIT_ASSERT(msg.replyTo.empty());
    CPPUNIT_ASSERT(msg.reactTo.empty());
    CPPUNIT_ASSERT(msg.editedId.empty());
    CPPUNIT_ASSERT(msg.action.empty());
    CPPUNIT_ASSERT(msg.uri.empty());
    CPPUNIT_ASSERT(msg.device.empty());
    CPPUNIT_ASSERT(msg.confId.empty());
    CPPUNIT_ASSERT(msg.to.empty());
    CPPUNIT_ASSERT(msg.reason.empty());
    CPPUNIT_ASSERT(msg.duration.empty());
    CPPUNIT_ASSERT(msg.tid.empty());
    CPPUNIT_ASSERT(msg.displayName.empty());
    CPPUNIT_ASSERT_EQUAL(int64_t(-1), msg.totalSize);
    CPPUNIT_ASSERT(msg.sha3sum.empty());
    CPPUNIT_ASSERT_EQUAL(-1, msg.mode);
    CPPUNIT_ASSERT(msg.invited.empty());
}

void
CommitMessageTest::testToStringFromStringRoundtrip()
{
    // Property-based test: generate random input parameters for each
    // CommitMessage factory method, serialize, parse back, and verify equality.

    std::mt19937 rng(12345); // fixed seed for reproducibility

    auto randomHex = [&](size_t len) -> std::string {
        static const char hexChars[] = "0123456789abcdef";
        std::uniform_int_distribution<int> dist(0, 15);
        std::string result;
        result.reserve(len);
        for (size_t i = 0; i < len; ++i)
            result += hexChars[dist(rng)];
        return result;
    };

    auto randomText = [&](size_t minLen = 1, size_t maxLen = 120) -> std::string {
        static const char chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                    "0123456789 !#$%&'()*+,-./:;<=>?@[]^_`{|}~";
        std::uniform_int_distribution<size_t> lenDist(minLen, maxLen);
        std::uniform_int_distribution<int> charDist(0, sizeof(chars) - 2);
        size_t len = lenDist(rng);
        std::string result;
        result.reserve(len);
        for (size_t i = 0; i < len; ++i)
            result += chars[charDist(rng)];
        return result;
    };

    auto randomReaction = [&]() -> std::string {
        std::vector<std::string> emojis = {"👍", "👎", "😂", "❤️", "😅", "🤔", "👋", "😭", "💯", "💀", "☝️"};
        return emojis[std::uniform_int_distribution<size_t>(0, emojis.size() - 1)(rng)];
    };

    auto randomUint64 = [&]() -> uint64_t {
        std::uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);
        return dist(rng);
    };

    auto randomFileSize = [&]() -> int64_t {
        std::uniform_int_distribution<int64_t> dist(0, 1000000000);
        return dist(rng);
    };

    auto assertRoundtrip = [](const CommitMessage& original) {
        auto restored = CommitMessage::fromString(original.toString());
        CPPUNIT_ASSERT(restored.has_value());
        CPPUNIT_ASSERT(messagesEqual(original, *restored));
    };

    for (int i = 0; i < 100; ++i) {
        // CommitMessage::text(body)
        assertRoundtrip(CommitMessage::text(randomText()));

        // CommitMessage::text(body, replyToId)
        assertRoundtrip(CommitMessage::text(randomText(), randomHex(40)));

        // CommitMessage::reaction(reaction, reactToId)
        assertRoundtrip(CommitMessage::reaction(randomReaction(), randomHex(40)));

        // CommitMessage::edit(newBody, editedId)
        assertRoundtrip(CommitMessage::edit(randomText(), randomHex(40)));

        // CommitMessage::edit with empty body (message/reaction deletion)
        assertRoundtrip(CommitMessage::edit("", randomHex(40)));

        // CommitMessage::member(action, memberId) for every action
        for (const auto& action :
             {CommitAction::ADD, CommitAction::JOIN, CommitAction::REMOVE, CommitAction::BAN, CommitAction::UNBAN}) {
            assertRoundtrip(CommitMessage::member(action, randomHex(40)));
        }

        // CommitMessage::outgoingCallEnd(peer, duration_ms)
        assertRoundtrip(CommitMessage::outgoingCallEnd(randomHex(40), randomUint64()));

        // CommitMessage::outgoingCallEnd(peer, duration_ms, reason)
        assertRoundtrip(CommitMessage::outgoingCallEnd(randomHex(40), randomUint64(), "declined"));

        // CommitMessage::conferenceHostingStart(confId, device, hostId)
        assertRoundtrip(
            CommitMessage::conferenceHostingStart(std::to_string(randomUint64()), randomHex(64), randomHex(40)));

        // CommitMessage::conferenceHostingEnd(confId, device, hostId, duration_ms)
        assertRoundtrip(CommitMessage::conferenceHostingEnd(std::to_string(randomUint64()),
                                                            randomHex(64),
                                                            randomHex(40),
                                                            randomUint64()));

        // CommitMessage::fileSent(displayName, sha3sum, tid, totalSize)
        assertRoundtrip(CommitMessage::fileSent(randomText(1, 50), randomHex(128), randomUint64(), randomFileSize()));

        // CommitMessage::fileSent(displayName, sha3sum, tid, totalSize, replyToId)
        assertRoundtrip(
            CommitMessage::fileSent(randomText(1, 50), randomHex(128), randomUint64(), randomFileSize(), randomHex(40)));

        // CommitMessage::fileDeleted(fileCommitId)
        assertRoundtrip(CommitMessage::fileDeleted(randomHex(40)));

        // CommitMessage::initial for one-to-one conversation
        assertRoundtrip(CommitMessage::initial(ConversationMode::ONE_TO_ONE, randomHex(40)));

        // CommitMessage::vote(userId)
        assertRoundtrip(CommitMessage::vote(randomHex(40)));
    }

    assertRoundtrip(CommitMessage::initial(ConversationMode::ADMIN_INVITES_ONLY));
    assertRoundtrip(CommitMessage::initial(ConversationMode::INVITES_ONLY));
    assertRoundtrip(CommitMessage::initial(ConversationMode::PUBLIC));
    assertRoundtrip(CommitMessage::updateProfile());
}

void
CommitMessageTest::testFromStringToStringRoundtrip()
{
    const std::vector<std::string> examples = {
        R"({"action":"add","type":"member","uri":"1239121ca897ed518a61d7f3935add5d58b139bc"})",
        R"({"action":"ban","type":"member","uri":"31415926fab271828b72d7f3935add5d58b139bc"})",
        R"({"action":"join","type":"member","uri":"012801314159265358abcdef935add5d58b139bc"})",
        R"({"action":"remove","type":"member","uri":"0128010bf797ed519b72d2718283141598b139bc"})",
        R"({"action":"unban","type":"member","uri":"01ffaaabb31415919b72d7f3935add5d58b139bc"})",
        R"({"body":"some message","type":"text/plain"})",
        R"({"body":"https://jami.net","type":"text/plain"})",
        R"({"body":"Alice is back.","type":"text/plain"})",
        R"({"body":"List:\n- **bold item**\n- *italic item*","type":"text/plain"})",
        R"({"body":"Edited.","edit":"8ac3b807ae657fb67e638393c27415976e80421f","type":"text/plain"})",
        R"({"body":"","edit":"8ac3b807ae657fb67e638393c27415976e80421f","type":"text/plain"})",
        R"({"body":"A reply.","reply-to":"c82443c9f8e4834b8e6a880185a857a4e5b5e5ad","type":"text/plain"})",
        R"({"body":"+1","react-to":"d015708ff060104912b5e02cb1e25b017246ecf9","type":"text/plain"})",
        R"({"body":"\ud83d\udc4d","react-to":"d433e037b32314bc3de2fad2ca4a12d914ecad57","type":"text/plain"})",
        R"({"duration":"80805","to":"ff114e1934db7b79e4f7ac676cb943d97ffb6a32","type":"application/call-history+json"})",
        R"({"duration":"0","reason":"declined","to":"ff114e1934db7b79e4f7ac676cb943d97ffb6a32","type":"application/call-history+json"})",
        R"({"confId":"9999999997443496","device":"333331aaffa4444444449999999bdcdbbbbbbbbc8488a530002229a8a8bade81","duration":"11456","type":"application/call-history+json","uri":"f32701058c8888ad6a095c6d14650cd5b3ab40a3"})",
        R"({"confId":"9999999997443496","device":"333331aaffa4444444449999999bdcdbbbbbbbbc8488a530002229a8a8bade81","type":"application/call-history+json","uri":"f32701058c8888ad6a095c6d14650cd5b3ab40a3"})",
        R"({"displayName":"SomeFile.txt","sha3sum":"aa2a57c7793fbb73a582f973e3d09ee187fda75efaf098e93116c8287ad031bafa9bac46f35eeba63d7246e3fca18e70a05d8f888868e59b5d2c62542fb56f6a","tid":"4977122036805143","totalSize":"6600","type":"application/data-transfer+json"})",
        R"({"displayName":"aang.jpg","reply-to":"160e330e417401ecdd11094a8dad1355bd734583","sha3sum":"cae439cabde1dd86e15210f1f1486e7bbe0b901e9cb40d087b9673b7827ac5c9b2bf3d5fa3872666d418f205e986e8afa9977ddaba5e4141bdb157fd754656c2","tid":"693561489759880","totalSize":"89040","type":"application/data-transfer+json"})",
        R"({"edit":"00cae7e31da86e3524c1c34ede750f19cd78aa36","tid":"","type":"application/data-transfer+json"})",
        "Merge commit 'ad512a444a7dc7d608c7ea41c86715d06f40988c'",
        R"({"invited":"f32701048c59f9ad6a095c6d14650294b4cf30a4","mode":0,"type":"initial"})",
        R"({"mode":1,"type":"initial"})",
        R"({"mode":2,"type":"initial"})",
        R"({"mode":3,"type":"initial"})",
        R"({"type":"application/update-profile"})",
        R"({"type":"vote","uri":"f32701058c8888ad6a095c6d14650cd5b3ab40a3"})",
        R"({"body":"deprecated edit type","edit":"e59e317cdcd7d92a8033538a08110ec7833fc017","type":"application/edited-message"})",
        R"({"body":"","edit":"3927511d7d0f4bb93a1733ed16a4be91f807a073","type":"application/edited-message"})",
    };

    for (const auto& example : examples) {
        auto msg = CommitMessage::fromString(example);
        CPPUNIT_ASSERT(msg.has_value());
        CPPUNIT_ASSERT_EQUAL(example, msg->toString());
    }
}

void
CommitMessageTest::testFromStringInvalidJson()
{
    CPPUNIT_ASSERT(!CommitMessage::fromString("not json at all").has_value());
    CPPUNIT_ASSERT(!CommitMessage::fromString("{invalid json}").has_value());
    CPPUNIT_ASSERT(!CommitMessage::fromString("").has_value());
    CPPUNIT_ASSERT(!CommitMessage::fromString("[1, 2, 3]").has_value());
}

void
CommitMessageTest::testExampleMessages()
{
    // Verify that example messages are parsed with the correct field values.

    // --- Text messages ---

    // Plain text message
    {
        auto msg = CommitMessage::fromString(R"({"body":"Hello!","type":"text/plain"})");
        CPPUNIT_ASSERT(msg.has_value());
        CPPUNIT_ASSERT_EQUAL(std::string(CommitType::TEXT), msg->type);
        CPPUNIT_ASSERT_EQUAL(std::string("Hello!"), msg->body);
        CPPUNIT_ASSERT(msg->editedId.empty());
        CPPUNIT_ASSERT(msg->replyTo.empty());
        CPPUNIT_ASSERT(msg->reactTo.empty());
    }

    // Text message with edit
    {
        auto msg = CommitMessage::fromString(
            R"({"body":"Hello, how are you?","edit":"7de8a42695da4de31f774df7040893d34de0829d","type":"text/plain"})");
        CPPUNIT_ASSERT(msg.has_value());
        CPPUNIT_ASSERT_EQUAL(std::string(CommitType::TEXT), msg->type);
        CPPUNIT_ASSERT_EQUAL(std::string("Hello, how are you?"), msg->body);
        CPPUNIT_ASSERT_EQUAL(std::string("7de8a42695da4de31f774df7040893d34de0829d"), msg->editedId);
        CPPUNIT_ASSERT(msg->replyTo.empty());
        CPPUNIT_ASSERT(msg->reactTo.empty());
    }

    // Deprecated edited-message type (backward compatibility)
    {
        auto msg = CommitMessage::fromString(
            R"({"body":"Hi!","edit":"81528f849e844b6b0ded23b92ed0fc8d06bc21a2","type":"application/edited-message"})");
        CPPUNIT_ASSERT(msg.has_value());
        CPPUNIT_ASSERT_EQUAL(std::string(CommitType::EDITED_MESSAGE), msg->type);
        CPPUNIT_ASSERT_EQUAL(std::string("Hi!"), msg->body);
        CPPUNIT_ASSERT_EQUAL(std::string("81528f849e844b6b0ded23b92ed0fc8d06bc21a2"), msg->editedId);
    }

    // Deleted message (edit with empty body)
    {
        auto msg = CommitMessage::fromString(
            R"({"body":"","edit":"7de8a42695da4de31f774df7040893d34de0829d","type":"text/plain"})");
        CPPUNIT_ASSERT(msg.has_value());
        CPPUNIT_ASSERT_EQUAL(std::string(CommitType::TEXT), msg->type);
        CPPUNIT_ASSERT(msg->body.empty());
        CPPUNIT_ASSERT_EQUAL(std::string("7de8a42695da4de31f774df7040893d34de0829d"), msg->editedId);
    }

    // Text message with reply-to
    {
        auto msg = CommitMessage::fromString(
            R"({"body":"You're right!","reply-to":"200779c99a3f6ed7efc2a83bdfddb6a9e45a4e55","type":"text/plain"})");
        CPPUNIT_ASSERT(msg.has_value());
        CPPUNIT_ASSERT_EQUAL(std::string(CommitType::TEXT), msg->type);
        CPPUNIT_ASSERT_EQUAL(std::string("You're right!"), msg->body);
        CPPUNIT_ASSERT_EQUAL(std::string("200779c99a3f6ed7efc2a83bdfddb6a9e45a4e55"), msg->replyTo);
        CPPUNIT_ASSERT(msg->editedId.empty());
        CPPUNIT_ASSERT(msg->reactTo.empty());
    }

    // Reaction (emoji via JSON unicode escape)
    {
        auto msg = CommitMessage::fromString(
            R"({"body":"\ud83d\udc4d","react-to":"d433e037b32314bc3de2fad2ca4a12d914ecad57","type":"text/plain"})");
        CPPUNIT_ASSERT(msg.has_value());
        CPPUNIT_ASSERT_EQUAL(std::string(CommitType::TEXT), msg->type);
        CPPUNIT_ASSERT(!msg->body.empty());
        CPPUNIT_ASSERT_EQUAL(std::string("d433e037b32314bc3de2fad2ca4a12d914ecad57"), msg->reactTo);
        CPPUNIT_ASSERT(msg->replyTo.empty());
        CPPUNIT_ASSERT(msg->editedId.empty());
    }

    // Reaction removal (edit with empty body targeting a reaction commit)
    {
        auto msg = CommitMessage::fromString(
            R"({"body":"","edit":"9f5a429073f313d3edb149c61fbd682c6e0fc704","type":"text/plain"})");
        CPPUNIT_ASSERT(msg.has_value());
        CPPUNIT_ASSERT_EQUAL(std::string(CommitType::TEXT), msg->type);
        CPPUNIT_ASSERT(msg->body.empty());
        CPPUNIT_ASSERT_EQUAL(std::string("9f5a429073f313d3edb149c61fbd682c6e0fc704"), msg->editedId);
    }

    // --- Member actions ---

    // Member "add"
    {
        auto msg = CommitMessage::fromString(
            R"({"action":"add","type":"member","uri":"0128010bf797ed519b72d7f3935add5d58b139bc"})");
        CPPUNIT_ASSERT(msg.has_value());
        CPPUNIT_ASSERT_EQUAL(std::string(CommitType::MEMBER), msg->type);
        CPPUNIT_ASSERT_EQUAL(std::string(CommitAction::ADD), msg->action);
        CPPUNIT_ASSERT_EQUAL(std::string("0128010bf797ed519b72d7f3935add5d58b139bc"), msg->uri);
        CPPUNIT_ASSERT(msg->body.empty());
    }

    // Member "join"
    {
        auto msg = CommitMessage::fromString(
            R"({"action":"join","type":"member","uri":"f32701058c69f8ad6a095c6d14650294a4ba39a3"})");
        CPPUNIT_ASSERT(msg.has_value());
        CPPUNIT_ASSERT_EQUAL(std::string(CommitType::MEMBER), msg->type);
        CPPUNIT_ASSERT_EQUAL(std::string(CommitAction::JOIN), msg->action);
        CPPUNIT_ASSERT_EQUAL(std::string("f32701058c69f8ad6a095c6d14650294a4ba39a3"), msg->uri);
    }

    // Member "remove"
    {
        auto msg = CommitMessage::fromString(
            R"({"action":"remove","type":"member","uri":"0128010bf797ed519b72d7f3935add5d58b139bc"})");
        CPPUNIT_ASSERT(msg.has_value());
        CPPUNIT_ASSERT_EQUAL(std::string(CommitType::MEMBER), msg->type);
        CPPUNIT_ASSERT_EQUAL(std::string(CommitAction::REMOVE), msg->action);
        CPPUNIT_ASSERT_EQUAL(std::string("0128010bf797ed519b72d7f3935add5d58b139bc"), msg->uri);
    }

    // Member "ban"
    {
        auto msg = CommitMessage::fromString(
            R"({"action":"ban","type":"member","uri":"0128010bf797ed519b72d7f3935add5d58b139bc"})");
        CPPUNIT_ASSERT(msg.has_value());
        CPPUNIT_ASSERT_EQUAL(std::string(CommitType::MEMBER), msg->type);
        CPPUNIT_ASSERT_EQUAL(std::string(CommitAction::BAN), msg->action);
        CPPUNIT_ASSERT_EQUAL(std::string("0128010bf797ed519b72d7f3935add5d58b139bc"), msg->uri);
    }

    // Member "unban"
    {
        auto msg = CommitMessage::fromString(
            R"({"action":"unban","type":"member","uri":"0128010bf797ed519b72d7f3935add5d58b139bc"})");
        CPPUNIT_ASSERT(msg.has_value());
        CPPUNIT_ASSERT_EQUAL(std::string(CommitType::MEMBER), msg->type);
        CPPUNIT_ASSERT_EQUAL(std::string(CommitAction::UNBAN), msg->action);
        CPPUNIT_ASSERT_EQUAL(std::string("0128010bf797ed519b72d7f3935add5d58b139bc"), msg->uri);
    }

    // --- Call history ---

    // One-to-one call end
    {
        auto msg = CommitMessage::fromString(
            R"({"duration":"80805","to":"ff114e1934db7b79e4f7ac676cb943d97ffb6a32","type":"application/call-history+json"})");
        CPPUNIT_ASSERT(msg.has_value());
        CPPUNIT_ASSERT_EQUAL(std::string(CommitType::CALL_HISTORY), msg->type);
        CPPUNIT_ASSERT_EQUAL(std::string("80805"), msg->duration);
        CPPUNIT_ASSERT_EQUAL(std::string("ff114e1934db7b79e4f7ac676cb943d97ffb6a32"), msg->to);
        CPPUNIT_ASSERT(msg->reason.empty());
        CPPUNIT_ASSERT(msg->confId.empty());
    }

    // One-to-one call declined
    {
        auto msg = CommitMessage::fromString(
            R"({"duration":"0","reason":"declined","to":"ff114e1934db7b79e4f7ac676cb943d97ffb6a32","type":"application/call-history+json"})");
        CPPUNIT_ASSERT(msg.has_value());
        CPPUNIT_ASSERT_EQUAL(std::string(CommitType::CALL_HISTORY), msg->type);
        CPPUNIT_ASSERT_EQUAL(std::string("0"), msg->duration);
        CPPUNIT_ASSERT_EQUAL(std::string("declined"), msg->reason);
        CPPUNIT_ASSERT_EQUAL(std::string("ff114e1934db7b79e4f7ac676cb943d97ffb6a32"), msg->to);
    }

    // Group call start
    {
        auto msg = CommitMessage::fromString(
            R"({"confId":"6342183642926168","device":"c87dc5b688c0e6a7d1cd30fe5c2b4a24aa68d6387ebba9aa7cbb487419578ea1","type":"application/call-history+json","uri":"079ddd3b04f35f6381f2516315e6aa5b98d43ef4"})");
        CPPUNIT_ASSERT(msg.has_value());
        CPPUNIT_ASSERT_EQUAL(std::string(CommitType::CALL_HISTORY), msg->type);
        CPPUNIT_ASSERT_EQUAL(std::string("6342183642926168"), msg->confId);
        CPPUNIT_ASSERT_EQUAL(std::string("c87dc5b688c0e6a7d1cd30fe5c2b4a24aa68d6387ebba9aa7cbb487419578ea1"),
                             msg->device);
        CPPUNIT_ASSERT_EQUAL(std::string("079ddd3b04f35f6381f2516315e6aa5b98d43ef4"), msg->uri);
        CPPUNIT_ASSERT(msg->duration.empty());
    }

    // Group call end
    {
        auto msg = CommitMessage::fromString(
            R"({"confId":"6342183642926168","device":"c87dc5b688c0e6a7d1cd30fe5c2b4a24aa68d6387ebba9aa7cbb487419578ea1","duration":"9142","type":"application/call-history+json","uri":"079ddd3b04f35f6381f2516315e6aa5b98d43ef4"})");
        CPPUNIT_ASSERT(msg.has_value());
        CPPUNIT_ASSERT_EQUAL(std::string(CommitType::CALL_HISTORY), msg->type);
        CPPUNIT_ASSERT_EQUAL(std::string("6342183642926168"), msg->confId);
        CPPUNIT_ASSERT_EQUAL(std::string("c87dc5b688c0e6a7d1cd30fe5c2b4a24aa68d6387ebba9aa7cbb487419578ea1"),
                             msg->device);
        CPPUNIT_ASSERT_EQUAL(std::string("079ddd3b04f35f6381f2516315e6aa5b98d43ef4"), msg->uri);
        CPPUNIT_ASSERT_EQUAL(std::string("9142"), msg->duration);
    }

    // --- Data transfer ---

    // File sent
    {
        auto msg = CommitMessage::fromString(
            R"({"displayName":"some_image.png","sha3sum":"5ce2fb16eb16c9dc42f824218ec0b7be4927d9f9fef9860161159faee1c4236a758aeb4ed98b27bf439364ea3199fce23181be4720c79756cf714271b702efcd","tid":"6147910008623250","totalSize":"581","type":"application/data-transfer+json"})");
        CPPUNIT_ASSERT(msg.has_value());
        CPPUNIT_ASSERT_EQUAL(std::string(CommitType::DATA_TRANSFER), msg->type);
        CPPUNIT_ASSERT_EQUAL(std::string("some_image.png"), msg->displayName);
        CPPUNIT_ASSERT_EQUAL(std::string("5ce2fb16eb16c9dc42f824218ec0b7be4927d9f9fef9860161159faee1c4236a758aeb4ed98b2"
                                         "7bf439364ea3199fce23181be4720c79756cf714271b702efcd"),
                             msg->sha3sum);
        CPPUNIT_ASSERT_EQUAL(std::string("6147910008623250"), msg->tid);
        CPPUNIT_ASSERT_EQUAL(int64_t(581), msg->totalSize);
    }

    // File sent with reply-to
    {
        auto msg = CommitMessage::fromString(
            R"({"displayName":"some_image.png","reply-to":"160e330e417401ecdd11094a8dad1355bd734583","sha3sum":"5ce2fb16eb16c9dc42f824218ec0b7be4927d9f9fef9860161159faee1c4236a758aeb4ed98b27bf439364ea3199fce23181be4720c79756cf714271b702efcd","tid":"6147910008623250","totalSize":"581","type":"application/data-transfer+json"})");
        CPPUNIT_ASSERT(msg.has_value());
        CPPUNIT_ASSERT_EQUAL(std::string(CommitType::DATA_TRANSFER), msg->type);
        CPPUNIT_ASSERT_EQUAL(std::string("some_image.png"), msg->displayName);
        CPPUNIT_ASSERT_EQUAL(std::string("160e330e417401ecdd11094a8dad1355bd734583"), msg->replyTo);
        CPPUNIT_ASSERT_EQUAL(std::string("5ce2fb16eb16c9dc42f824218ec0b7be4927d9f9fef9860161159faee1c4236a758aeb4ed98b2"
                                         "7bf439364ea3199fce23181be4720c79756cf714271b702efcd"),
                             msg->sha3sum);
        CPPUNIT_ASSERT_EQUAL(std::string("6147910008623250"), msg->tid);
        CPPUNIT_ASSERT_EQUAL(int64_t(581), msg->totalSize);
    }

    // File deleted
    {
        auto msg = CommitMessage::fromString(
            R"({"edit":"7fc3b0cba7e0742b0753051a576a5d17a77a57d0","tid":"","type":"application/data-transfer+json"})");
        CPPUNIT_ASSERT(msg.has_value());
        CPPUNIT_ASSERT_EQUAL(std::string(CommitType::DATA_TRANSFER), msg->type);
        CPPUNIT_ASSERT_EQUAL(std::string("7fc3b0cba7e0742b0753051a576a5d17a77a57d0"), msg->editedId);
        CPPUNIT_ASSERT(msg->tid.empty());
    }

    // --- Initial commits ---

    // One-to-one conversation (mode 0 with invited)
    {
        auto msg = CommitMessage::fromString(
            R"({"invited":"f32701048c59f9ad6a095c6d14650294b4cf30a4","mode":0,"type":"initial"})");
        CPPUNIT_ASSERT(msg.has_value());
        CPPUNIT_ASSERT_EQUAL(std::string(CommitType::INITIAL), msg->type);
        CPPUNIT_ASSERT_EQUAL(0, msg->mode);
        CPPUNIT_ASSERT_EQUAL(std::string("f32701048c59f9ad6a095c6d14650294b4cf30a4"), msg->invited);
    }

    // Group conversation (mode 2)
    {
        auto msg = CommitMessage::fromString(R"({"mode":2,"type":"initial"})");
        CPPUNIT_ASSERT(msg.has_value());
        CPPUNIT_ASSERT_EQUAL(std::string(CommitType::INITIAL), msg->type);
        CPPUNIT_ASSERT_EQUAL(2, msg->mode);
        CPPUNIT_ASSERT(msg->invited.empty());
    }

    // --- Vote ---
    {
        auto msg = CommitMessage::fromString(R"({"type":"vote","uri":"f32701048c59f9ad6a095c6d14650294b4cf30a4"})");
        CPPUNIT_ASSERT(msg.has_value());
        CPPUNIT_ASSERT_EQUAL(std::string(CommitType::VOTE), msg->type);
        CPPUNIT_ASSERT_EQUAL(std::string("f32701048c59f9ad6a095c6d14650294b4cf30a4"), msg->uri);
    }

    // --- Update profile ---
    {
        auto msg = CommitMessage::fromString(R"({"type":"application/update-profile"})");
        CPPUNIT_ASSERT(msg.has_value());
        CPPUNIT_ASSERT_EQUAL(std::string(CommitType::UPDATE_PROFILE), msg->type);
    }

    // --- Merge commit ---
    {
        auto msg = CommitMessage::fromString("Merge commit 'ad512a444a7dc7d608c7ea41c86715d06f40988c'");
        CPPUNIT_ASSERT(msg.has_value());
        CPPUNIT_ASSERT_EQUAL(std::string(CommitType::MERGE), msg->type);
        CPPUNIT_ASSERT_EQUAL(std::string("Merge commit 'ad512a444a7dc7d608c7ea41c86715d06f40988c'"), msg->body);
    }
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::CommitMessageTest::name());
