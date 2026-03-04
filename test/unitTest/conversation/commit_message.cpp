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
    return a.type == b.type && a.body == b.body && a.replyTo == b.replyTo && a.reactTo == b.reactTo && a.edit == b.edit
           && a.action == b.action && a.uri == b.uri && a.device == b.device && a.confId == b.confId && a.to == b.to
           && a.reason == b.reason && a.duration == b.duration && a.tid == b.tid && a.displayName == b.displayName
           && a.totalSize == b.totalSize && a.sha3sum == b.sha3sum && a.mode == b.mode && a.invited == b.invited;
}

void
CommitMessageTest::testDefaultValues()
{
    CommitMessage msg;
    CPPUNIT_ASSERT(msg.type.empty());
    CPPUNIT_ASSERT(msg.body.empty());
    CPPUNIT_ASSERT(msg.replyTo.empty());
    CPPUNIT_ASSERT(msg.reactTo.empty());
    CPPUNIT_ASSERT(msg.edit.empty());
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
    // Build several CommitMessage structs, serialize them and parse them back.
    // All fields in the restored message must equal those in the original.

    // Plain text message
    {
        CommitMessage original;
        original.type = CommitType::TEXT;
        original.body = "Hello, world!";

        auto restored = CommitMessage::fromString(original.toString());
        CPPUNIT_ASSERT(restored.has_value());
        CPPUNIT_ASSERT(messagesEqual(original, *restored));
    }

    // Text message with reply-to
    {
        CommitMessage original;
        original.type = CommitType::TEXT;
        original.body = "A reply.";
        original.replyTo = "c82443c9f8e4834b8e6a880185a857a4e5b5e5ad";

        auto restored = CommitMessage::fromString(original.toString());
        CPPUNIT_ASSERT(restored.has_value());
        CPPUNIT_ASSERT(messagesEqual(original, *restored));
    }

    // Text message with react-to
    {
        CommitMessage original;
        original.type = CommitType::TEXT;
        original.body = "+1";
        original.reactTo = "d015708ff060104912b5e02cb1e25b017246ecf9";

        auto restored = CommitMessage::fromString(original.toString());
        CPPUNIT_ASSERT(restored.has_value());
        CPPUNIT_ASSERT(messagesEqual(original, *restored));
    }

    // Text message with edit
    {
        CommitMessage original;
        original.type = CommitType::TEXT;
        original.body = "Corrected text.";
        original.edit = "8ac3b807ae657fb67e638393c27415976e80421f";

        auto restored = CommitMessage::fromString(original.toString());
        CPPUNIT_ASSERT(restored.has_value());
        CPPUNIT_ASSERT(messagesEqual(original, *restored));
    }

    // Member action
    {
        CommitMessage original;
        original.type = CommitType::MEMBER;
        original.action = CommitAction::ADD;
        original.uri = "0128010bf797ed519b72d7f3935add5d58b139bc";

        auto restored = CommitMessage::fromString(original.toString());
        CPPUNIT_ASSERT(restored.has_value());
        CPPUNIT_ASSERT(messagesEqual(original, *restored));
    }

    // Merge commit
    {
        CommitMessage original;
        original.type = CommitType::MERGE;
        original.body = "Merge commit abc123def456";

        auto restored = CommitMessage::fromString(original.toString());
        CPPUNIT_ASSERT(restored.has_value());
        CPPUNIT_ASSERT(messagesEqual(original, *restored));
    }

    // Data transfer
    {
        CommitMessage original;
        original.type = CommitType::DATA_TRANSFER;
        original.tid = "42";
        original.totalSize = 4096;
        auto restored = CommitMessage::fromString(original.toString());
        CPPUNIT_ASSERT(restored.has_value());
        CPPUNIT_ASSERT(messagesEqual(original, *restored));
    }

    // Initial commit
    {
        CommitMessage original;
        original.type = CommitType::INITIAL;
        original.mode = 0;

        auto restored = CommitMessage::fromString(original.toString());
        CPPUNIT_ASSERT(restored.has_value());
        CPPUNIT_ASSERT(messagesEqual(original, *restored));
    }
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
        R"({"body":"Edited.","edit":"8ac3b807ae657fb67e638393c27415976e80421f","type":"text/plain"})",
        R"({"body":"A reply.","reply-to":"c82443c9f8e4834b8e6a880185a857a4e5b5e5ad","type":"text/plain"})",
        R"({"body":"+1","react-to":"d015708ff060104912b5e02cb1e25b017246ecf9","type":"text/plain"})",
        R"({"body":"\ud83d\udc4d","react-to":"d433e037b32314bc3de2fad2ca4a12d914ecad57","type":"text/plain"})",
        R"({"duration":"80805","to":"ff114e1934db7b79e4f7ac676cb943d97ffb6a32","type":"application/call-history+json"})",
        R"({"duration":"0","reason":"declined","to":"ff114e1934db7b79e4f7ac676cb943d97ffb6a32","type":"application/call-history+json"})",
        R"({"confId":"9999999997443496","device":"333331aaffa4444444449999999bdcdbbbbbbbbc8488a530002229a8a8bade81","duration":"11456","type":"application/call-history+json","uri":"f32701058c8888ad6a095c6d14650cd5b3ab40a3"})",
        R"({"confId":"9999999997443496","device":"333331aaffa4444444449999999bdcdbbbbbbbbc8488a530002229a8a8bade81","type":"application/call-history+json","uri":"f32701058c8888ad6a095c6d14650cd5b3ab40a3"})",
        R"({"displayName":"SomeFile.txt","sha3sum":"aa2a57c7793fbb73a582f973e3d09ee187fda75efaf098e93116c8287ad031bafa9bac46f35eeba63d7246e3fca18e70a05d8f888868e59b5d2c62542fb56f6a","tid":"4977122036805143","totalSize":"6600","type":"application/data-transfer+json"})",
        R"({"edit":"00cae7e31da86e3524c1c34ede750f19cd78aa36","tid":"","type":"application/data-transfer+json"})",
        "Merge commit 'ad512a444a7dc7d608c7ea41c86715d06f40988c'",
        R"({"mode":2,"type":"initial"})",
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

    // Member "add" action
    {
        auto msg = CommitMessage::fromString(
            R"({"action":"add","type":"member","uri":"0128010bf797ed519b72d7f3935add5d58b139bc"})");
        CPPUNIT_ASSERT(msg.has_value());
        CPPUNIT_ASSERT_EQUAL(std::string(CommitType::MEMBER), msg->type);
        CPPUNIT_ASSERT_EQUAL(std::string(CommitAction::ADD), msg->action);
        CPPUNIT_ASSERT_EQUAL(std::string("0128010bf797ed519b72d7f3935add5d58b139bc"), msg->uri);
        CPPUNIT_ASSERT(msg->body.empty());
    }

    // Member "ban" action
    {
        auto msg = CommitMessage::fromString(
            R"({"action":"ban","type":"member","uri":"0128010bf797ed519b72d7f3935add5d58b139bc"})");
        CPPUNIT_ASSERT(msg.has_value());
        CPPUNIT_ASSERT_EQUAL(std::string("ban"), msg->action);
    }

    // Plain text message
    {
        auto msg = CommitMessage::fromString(R"({"body":"ah, a sec -","type":"text/plain"})");
        CPPUNIT_ASSERT(msg.has_value());
        CPPUNIT_ASSERT_EQUAL(std::string(CommitType::TEXT), msg->type);
        CPPUNIT_ASSERT_EQUAL(std::string("ah, a sec -"), msg->body);
        CPPUNIT_ASSERT(msg->edit.empty());
        CPPUNIT_ASSERT(msg->replyTo.empty());
        CPPUNIT_ASSERT(msg->reactTo.empty());
    }

    // Text message with edit
    {
        auto msg = CommitMessage::fromString(
            R"({"body":"Edited.","edit":"8ac3b807ae657fb67e638393c27415976e80421f","type":"text/plain"})");
        CPPUNIT_ASSERT(msg.has_value());
        CPPUNIT_ASSERT_EQUAL(std::string(CommitType::TEXT), msg->type);
        CPPUNIT_ASSERT_EQUAL(std::string("Edited."), msg->body);
        CPPUNIT_ASSERT_EQUAL(std::string("8ac3b807ae657fb67e638393c27415976e80421f"), msg->edit);
        CPPUNIT_ASSERT(msg->replyTo.empty());
    }

    // Text message with reply-to
    {
        auto msg = CommitMessage::fromString(
            R"({"body":"A reply.","reply-to":"c82443c9f8e4834b8e6a880185a857a4e5b5e5ad","type":"text/plain"})");
        CPPUNIT_ASSERT(msg.has_value());
        CPPUNIT_ASSERT_EQUAL(std::string("A reply."), msg->body);
        CPPUNIT_ASSERT_EQUAL(std::string("c82443c9f8e4834b8e6a880185a857a4e5b5e5ad"), msg->replyTo);
        CPPUNIT_ASSERT(msg->edit.empty());
    }

    // Text message with react-to
    {
        auto msg = CommitMessage::fromString(
            R"({"body":"+1","react-to":"70d9304c1e457c3aed8b6e25c8e12c348caedbfb","type":"text/plain"})");
        CPPUNIT_ASSERT(msg.has_value());
        CPPUNIT_ASSERT_EQUAL(std::string("70d9304c1e457c3aed8b6e25c8e12c348caedbfb"), msg->reactTo);
        CPPUNIT_ASSERT(msg->replyTo.empty());
        CPPUNIT_ASSERT(msg->edit.empty());
    }

    // A real-world multi-line text message with embedded escape sequences
    {
        auto msg = CommitMessage::fromString(
            R"({"body":"0. `cyrilleberaud` is no longer in the left conversations panel\n1. Click on `Collaborative editor` swarm\n2. Click `Details` in the top right\n3. Right-click on `cyrilleberaud`\n4. Select `Go to conversation`","type":"text/plain"})");
        CPPUNIT_ASSERT(msg.has_value());
        CPPUNIT_ASSERT_EQUAL(std::string(CommitType::TEXT), msg->type);
        CPPUNIT_ASSERT(!msg->body.empty());
    }
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::CommitMessageTest::name());
