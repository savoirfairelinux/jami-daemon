/*
 *  Copyright (C) 2017-2019 Savoir-faire Linux Inc.
 *  Author: Sébastien Blin <sebastien.blin@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <condition_variable>
#include <string>
#include <fstream>
#include <streambuf>

#include "manager.h"
#include "jamidht/conversationrepository.h"
#include "jamidht/jamiaccount.h"
#include "../../test_runner.h"
#include "dring.h"
#include "base64.h"
#include "fileutils.h"
#include "account_const.h"

#include <git2.h>

using namespace DRing::Account;





#include <git2/sys/transport.h>
#include <git2/errors.h>
#include "jamidht/multiplexed_socket.h"
#include "jamidht/connectionmanager.h"


typedef struct {
    jami::ChannelSocket* socket;
    git_transport* owner;
	git_smart_subtransport base;
} ChannelSocketTransport;

typedef struct {
    jami::ChannelSocket* socket;
    git_smart_subtransport_stream base;
} ChannelStream;


static int stream_read(git_smart_subtransport_stream *stream,
	char *buffer,
	size_t buf_size,
	size_t *bytes_read)
{
    JAMI_ERR("READ");
	ChannelStream* st = (ChannelStream*) stream;
    std::error_code ec;
    *bytes_read = st->socket->read((unsigned char*)(buffer), buf_size, ec);
    JAMI_ERR("READ %u", read);
	return 0;
}

static int stream_write(git_smart_subtransport_stream *stream,
	  const char *buffer, size_t len)
{
    JAMI_ERR("WRITE");
	ChannelStream* st = (ChannelStream*) stream;
    std::error_code ec;
    auto written = st->socket->write((const unsigned char*)(buffer), len, ec);
    JAMI_ERR("WRITTEN: %u", written);
	return 0;
}

static void stream_free(git_smart_subtransport_stream *stream)
{
    JAMI_ERR("FREE");
	delete stream;
}

static int stream_new(
	ChannelStream **out,
	jami::ChannelSocket* socket)
{
    JAMI_ERR("NEW");
	ChannelStream *stream = new ChannelStream();

	stream->socket = socket;
	stream->base.read = stream_read;
	stream->base.write = stream_write;
	stream->base.free = stream_free;

	*out = stream;
    JAMI_ERR("NEW END");

	return 0;
}


static int subtransport_action(
	git_smart_subtransport_stream **out,
	git_smart_subtransport *transport,
	const char *url,
	git_smart_service_t action)
{
    JAMI_ERR("ACTION %u", action);
    JAMI_ERR("URL %s", url);
	ChannelSocketTransport *ft = (ChannelSocketTransport *)transport;
	return stream_new((ChannelStream **) out, ft->socket);
}

static int subtransport_close(git_smart_subtransport *transport)
{
    JAMI_ERR("CLOSE");
	ChannelSocketTransport *ft = (ChannelSocketTransport *)transport;
//	ft->socket->shutdown();
	return 0;
}

static void subtransport_free(git_smart_subtransport *transport)
{
    JAMI_ERR("FREE");
	delete transport;
}

static int subtransport_new(
	ChannelSocketTransport **out,
	git_transport *owner,
	void *payload)
{
    JAMI_ERR("NEW");
    ChannelSocketTransport *sub = new ChannelSocketTransport();
    jami::ChannelSocket* channelSocket = (jami::ChannelSocket*)payload;

	sub->owner = owner;
	sub->socket = channelSocket;
	sub->base.action = subtransport_action;
	sub->base.close = subtransport_close;
	sub->base.free = subtransport_free;

    JAMI_ERR("...");
	*out = sub;

	return 0;
}

int subtransport_cb(
	git_smart_subtransport **out,
	git_transport *owner,
	void *payload)
{
    JAMI_ERR("CB");
	ChannelSocketTransport *sub;

	if (subtransport_new(&sub, owner, payload) < 0)
		return -1;

	*out = &sub->base;
	return 0;
}

static int
test_cb(git_transport **out, git_remote *owner, void *param)
{
    git_transport* parent = nullptr;

    git_smart_subtransport_definition sbDef = {
		subtransport_cb,
		1,
		param
	};
    git_transport_smart(&parent, owner, &sbDef);

    *out = parent;


    JAMI_ERR("TEST CB");
    return 0;
}










#define UNUSED(x) (void)(x)

typedef struct {
	git_smart_subtransport_stream base;
	jami::ChannelSocket* socket;
} fuzzer_stream;

typedef struct {
	git_smart_subtransport base;
	git_transport *owner;
	jami::ChannelSocket* socket;
} fuzzer_subtransport;

static git_repository *repo;

static int fuzzer_stream_read(git_smart_subtransport_stream *stream,
	char *buffer,
	size_t buf_size,
	size_t *bytes_read)
{
    JAMI_ERR("READ!");
	fuzzer_stream *fs = (fuzzer_stream *) stream;

    std::error_code ec;
    auto len = fs->socket->waitForData(std::chrono::milliseconds(99999999), ec);
    if (len > 0) {
        JAMI_ERR("READ! %u", len);
        *bytes_read = fs->socket->read((unsigned char*)(buffer), len, ec);
    }
    JAMI_ERR("READ! %u", *bytes_read);

	return 0;
}

static int fuzzer_stream_write(git_smart_subtransport_stream *stream,
	  const char *buffer, size_t len)
{
    JAMI_ERR("WRITE!");
	fuzzer_stream *fs = (fuzzer_stream *) stream;
    std::error_code ec;
    auto written = fs->socket->write((const unsigned char*)(buffer), len, ec);
	return 0;
}

static void fuzzer_stream_free(git_smart_subtransport_stream *stream)
{
    JAMI_ERR("FREE!");
	delete stream;
}

static int fuzzer_stream_new(
	fuzzer_stream **out,
	jami::ChannelSocket* socket)
{
    JAMI_ERR("NEW!");
	fuzzer_stream *stream = new fuzzer_stream();

	stream->socket = socket;
	stream->base.read = fuzzer_stream_read;
	stream->base.write = fuzzer_stream_write;
	stream->base.free = fuzzer_stream_free;

	*out = stream;

	return 0;
}

static int fuzzer_subtransport_action(
	git_smart_subtransport_stream **out,
	git_smart_subtransport *transport,
	const char *url,
	git_smart_service_t action)
{
    JAMI_ERR("ACTION!");
	fuzzer_subtransport *ft = (fuzzer_subtransport *) transport;

	UNUSED(url);
	UNUSED(action);

	return fuzzer_stream_new((fuzzer_stream **) out, ft->socket);
}

static int fuzzer_subtransport_close(git_smart_subtransport *transport)
{
    JAMI_ERR("CLOSE!");
	UNUSED(transport);
	return 0;
}

static void fuzzer_subtransport_free(git_smart_subtransport *transport)
{
    JAMI_ERR("FREE!");
	delete transport;
}

static int fuzzer_subtransport_new(
	fuzzer_subtransport **out,
	git_transport *owner,
	jami::ChannelSocket* socket)
{
    JAMI_ERR("NEW!");
	fuzzer_subtransport *sub = new fuzzer_subtransport();

	sub->owner = owner;
    sub->socket = socket;
	sub->base.action = fuzzer_subtransport_action;
	sub->base.close = fuzzer_subtransport_close;
	sub->base.free = fuzzer_subtransport_free;

	*out = sub;

	return 0;
}

int fuzzer_subtransport_cb(
	git_smart_subtransport **out,
	git_transport *owner,
	void *payload)
{
	jami::ChannelSocket* socket = (jami::ChannelSocket*) payload;
	fuzzer_subtransport *sub;

	if (fuzzer_subtransport_new(&sub, owner, socket) < 0)
		return -1;

	*out = &sub->base;
	return 0;
}

int fuzzer_transport_cb(git_transport **out, git_remote *owner, void *param)
{
	git_smart_subtransport_definition def = {
		fuzzer_subtransport_cb,
		1,
		param
	};
	return git_transport_smart(out, owner, &def);
}

void fuzzer_git_abort(const char *op)
{
	const git_error *err = giterr_last();
	fprintf(stderr, "unexpected libgit error: %s: %s\n",
		op, err ? err->message : "<none>");
	abort();
}







namespace jami { namespace test {

class ConversationRepositoryTest : public CppUnit::TestFixture {
public:
    ~ConversationRepositoryTest() {
        DRing::fini();
    }
    static std::string name() { return "ConversationRepository"; }
    void setUp();
    void tearDown();

    std::string aliceId;
    std::string bobId;

private:
    void testCreateRepository();
    void testCloneViaChannelSocket();

    CPPUNIT_TEST_SUITE(ConversationRepositoryTest);
   // CPPUNIT_TEST(testCreateRepository);
    CPPUNIT_TEST(testCloneViaChannelSocket);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ConversationRepositoryTest, ConversationRepositoryTest::name());

void
ConversationRepositoryTest::setUp()
{
    // Init daemon
    DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
    CPPUNIT_ASSERT(DRing::start("dring-sample.yml"));

    std::map<std::string, std::string> details = DRing::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "ALICE";
    details[ConfProperties::ALIAS] = "ALICE";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = "";
    aliceId = Manager::instance().addAccount(details);

    details = DRing::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "BOB";
    details[ConfProperties::ALIAS] = "BOB";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = "";
    bobId = Manager::instance().addAccount(details);

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);

    bool ready = false;
    bool idx = 0;
    while(!ready && idx < 100) {
        auto details = aliceAccount->getVolatileAccountDetails();
        auto daemonStatus = details[DRing::Account::ConfProperties::Registration::STATUS];
        ready = (daemonStatus == "REGISTERED");
        details = bobAccount->getVolatileAccountDetails();
        daemonStatus = details[DRing::Account::ConfProperties::Registration::STATUS];
        ready &= (daemonStatus == "REGISTERED");
        if (!ready) {
            idx += 1;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void
ConversationRepositoryTest::tearDown()
{
    auto currentAccSize = Manager::instance().getAccountList().size();
    Manager::instance().removeAccount(aliceId);
    Manager::instance().removeAccount(bobId);
    // Because cppunit is not linked with dbus, just poll if removed
    for (int i = 0; i < 40; ++i) {
        if (Manager::instance().getAccountList().size() <= currentAccSize - 2) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void
ConversationRepositoryTest::testCreateRepository()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceDeviceId = aliceAccount->getAccountDetails()[ConfProperties::RING_DEVICE_ID];

    auto repository = ConversationRepository::createConversation(aliceAccount->weak());

    // Assert that repository exists
    CPPUNIT_ASSERT(repository != nullptr);
    auto repoPath = fileutils::get_data_dir()+DIR_SEPARATOR_STR+aliceAccount->getAccountID()+DIR_SEPARATOR_STR+"conversations"+DIR_SEPARATOR_STR+repository->id();
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));

    // Assert that first commit is signed by alice
    git_repository* repo;
    CPPUNIT_ASSERT(git_repository_open(&repo, repoPath.c_str()) == 0);

    // 1. Verify that last commit is correctly signed by alice
    git_oid commit_id;
    CPPUNIT_ASSERT(git_reference_name_to_id(&commit_id, repo, "HEAD") == 0);

	git_buf signature = {}, signed_data = {};
    git_commit_extract_signature(&signature, &signed_data, repo, &commit_id, "signature");
    auto pk = base64::decode(std::string(signature.ptr, signature.ptr + signature.size));
    auto data = std::vector<uint8_t>(signed_data.ptr, signed_data.ptr + signed_data.size);
    git_repository_free(repo);

    CPPUNIT_ASSERT(aliceAccount->identity().second->getPublicKey().checkSignature(data, pk));

    // 2. Check created files
    auto CRLsPath = repoPath + DIR_SEPARATOR_STR + "CRLs" + DIR_SEPARATOR_STR + aliceDeviceId;
    CPPUNIT_ASSERT(fileutils::isDirectory(repoPath));

    auto adminCrt = repoPath + DIR_SEPARATOR_STR + "admins" + DIR_SEPARATOR_STR + aliceDeviceId + ".crt";
    CPPUNIT_ASSERT(fileutils::isFile(adminCrt));

    auto crt = std::ifstream(adminCrt);
    std::string adminCrtStr((std::istreambuf_iterator<char>(crt)), std::istreambuf_iterator<char>());

    auto cert = aliceAccount->identity().second;
    auto deviceCert = cert->toString(false);
    auto parentCert = cert->issuer->toString(true);

    CPPUNIT_ASSERT(adminCrtStr == parentCert);

    auto deviceCrt = repoPath + DIR_SEPARATOR_STR + "devices" + DIR_SEPARATOR_STR + aliceDeviceId + ".crt";
    CPPUNIT_ASSERT(fileutils::isFile(deviceCrt));

    crt = std::ifstream(deviceCrt);
    std::string deviceCrtStr((std::istreambuf_iterator<char>(crt)), std::istreambuf_iterator<char>());

    CPPUNIT_ASSERT(deviceCrtStr == deviceCert);
}

void
ConversationRepositoryTest::testCloneViaChannelSocket()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceDeviceId = aliceAccount->getAccountDetails()[ConfProperties::RING_DEVICE_ID];
    auto bobDeviceId = bobAccount->getAccountDetails()[ConfProperties::RING_DEVICE_ID];

    bobAccount->connectionManager().onICERequest([](const std::string&) {return true;});
    aliceAccount->connectionManager().onICERequest([](const std::string&) {return true;});
    auto repository = ConversationRepository::createConversation(aliceAccount->weak());
    auto repoPath = fileutils::get_data_dir()+DIR_SEPARATOR_STR+aliceAccount->getAccountID()+DIR_SEPARATOR_STR+"conversations"+DIR_SEPARATOR_STR+repository->id();
    auto clonedPath = repoPath + ".test_cloned";



    std::mutex mtx;
    std::unique_lock<std::mutex> lk{ mtx };
    std::condition_variable rcv, scv;
    bool successfullyConnected = false;
    bool successfullyReceive = false;
    bool receiverConnected = false;
    std::shared_ptr<ChannelSocket> channelSocket = nullptr;

    bobAccount->connectionManager().onChannelRequest(
    [&successfullyReceive](const std::string&, const std::string& name) {
        JAMI_ERR("...");
        successfullyReceive = name == "git://*";
        return true;
    });

    aliceAccount->connectionManager().onChannelRequest(
    [&successfullyReceive](const std::string&, const std::string& name) {
        JAMI_ERR("...");
        return true;
    });

    bobAccount->connectionManager().onConnectionReady(
    [&](const std::string&, const std::string& name, std::shared_ptr<ChannelSocket> socket) {
        receiverConnected = socket && (name == "git://*");
        JAMI_ERR("...");
        channelSocket = socket;
        rcv.notify_one();
    });

    aliceAccount->connectionManager().connectDevice(bobDeviceId, "git://*",
        [&](std::shared_ptr<ChannelSocket> socket) {
        JAMI_ERR("...");
        if (socket) {
            successfullyConnected = true;

            std::string buf = "From d21399329cfa4a414a917355a3061f007fa0d3d0 Mon Sep 17 00:00:00 2001\
From: =?UTF-8?q?S=C3=A9bastien=20Blin?=\
 <sebastien.blin@savoirfairelinux.com>\
Date: Wed, 18 Dec 2019 17:03:47 -0500\
Subject: [PATCH] das\
\
---\
 x | 6 ++++++\
 1 file changed, 6 insertions(+)\
 create mode 100644 x\
\
diff --git a/x b/x\
new file mode 100644\
index 0000000..cc6d6dc\
--- /dev/null\
+++ b/x\
@@ -0,0 +1,6 @@\
+dashdsahklhdklasda\
+f\
+ds\
+ fas\
+\
+\
--\
2.21.0\
";

            std::vector<uint8_t> vec(buf.begin(), buf.end());
            std::error_code ec;
            JAMI_ERR("%u", vec.size());
            socket->write(&vec[0], vec.size(), ec);
        }
        scv.notify_one();
    });

    rcv.wait_for(lk, std::chrono::seconds(10));
    scv.wait_for(lk, std::chrono::seconds(10));
    CPPUNIT_ASSERT(successfullyReceive);
    CPPUNIT_ASSERT(successfullyConnected);
    CPPUNIT_ASSERT(receiverConnected);

    auto res = git_transport_register("git", fuzzer_transport_cb, (void*)channelSocket.get());
    git_repository *rep = nullptr;
    res = git_clone(&rep, "git://test", clonedPath.c_str(), nullptr);
    git_repository_free(rep);
}

}} // namespace test

RING_TEST_RUNNER(jami::test::ConversationRepositoryTest::name())
