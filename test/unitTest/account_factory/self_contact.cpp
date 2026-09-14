/*
 * Copyright (C) 2026 Savoir-faire Linux Inc.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "jamidht/account_manager.h"
#include "jamidht/accountarchive.h"
#include "manager.h"
#include "fileutils.h"
#include "jami.h"
#include "../../test_runner.h"

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <fstream>

using namespace std::chrono_literals;

namespace jami::test {
namespace {
class ContactTestManager : public AccountManager
{
public:
    ContactTestManager(const std::string& id, const std::filesystem::path& path,
                       const dht::crypto::Identity& identity)
        : AccountManager(id, path, "")
    {
        info_ = std::make_unique<AccountInfo>();
        info_->identity = identity;
        info_->accountId = identity.second->issuer->getId().toString();
        info_->deviceId = identity.second->getLongId().toString();
        OnChangeCallback callbacks;
        callbacks.contactAdded = [this](const auto& uri, bool) { added.push_back(uri); };
        callbacks.contactRemoved = [this](const auto& uri, bool) { removed.push_back(uri); };
        callbacks.devicesChanged = [](const auto&) {};
        info_->contacts = std::make_unique<ContactList>(id, identity.second->issuer, path, callbacks);
        certStore().pinCertificate(identity.second, false);
        setAccountDataChangedCallback([this] { ++syncChanges; });
    }
    void initAuthentication(std::string, std::unique_ptr<AccountCredentials>,
                            AuthSuccessCallback, AuthFailureCallback, const OnChangeCallback&) override {}
    bool changePassword(const std::string&, const std::string&) override { return false; }
    void syncDevices() override { ++syncChanges; }
    void registerName(const std::string&, std::string_view, const std::string&, RegistrationCallback) override {}

    unsigned syncChanges {0};
    std::vector<std::string> added;
    std::vector<std::string> removed;
};
}

class SelfContactTest : public CppUnit::TestFixture
{
public:
    SelfContactTest()
    {
        libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (!Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("dring-sample.yml"));
    }
    ~SelfContactTest() { libjami::fini(); }
    static std::string name() { return "self_contact"; }

    void setUp() override
    {
        ownsRoot_ = std::filesystem::create_directory(root_);
        CPPUNIT_ASSERT(ownsRoot_);
        ca_ = dht::crypto::generateEcIdentity("authority", {}, true);
        account_ = dht::crypto::generateEcIdentity("account", ca_, true);
        local_ = dht::crypto::generateEcIdentity("local", account_);
        linked_ = dht::crypto::generateEcIdentity("linked", account_);
        revoked_ = dht::crypto::generateEcIdentity("revoked", account_);
        externalAccount_ = dht::crypto::generateEcIdentity("different account", ca_, true);
        external_ = dht::crypto::generateEcIdentity("external", externalAccount_);
        account_.second->revoke(*account_.first, *revoked_.second);
        manager_ = std::make_unique<ContactTestManager>(
            "self-contact-test-" + local_.second->getLongId().toString(), root_, local_);
        manager_->certStore().pinCertificate(linked_.second, false);
        manager_->certStore().pinCertificate(revoked_.second, false);
        manager_->certStore().pinCertificate(external_.second, false);
    }

    void tearDown() override
    {
        manager_.reset();
        if (ownsRoot_)
            std::filesystem::remove_all(root_);
    }

private:
    using Status = dhtnet::tls::TrustStore::PermissionStatus;
    const std::filesystem::path root_ {"self-contact-test-state"};
    bool ownsRoot_ {false};
    dht::crypto::Identity ca_, account_, local_, linked_, revoked_, externalAccount_, external_;
    std::unique_ptr<ContactTestManager> manager_;

    std::map<dht::InfoHash, Contact> legacy() const
    {
        Contact bad;
        bad.removed = nowMs() + 5s; // also exercise future/seconds-only legacy clocks
        bad.banned = true;
        Contact other = bad;
        other.added = timePointFromMilliseconds(1234);
        other.conversationId = "keep-external-conversation";
        return {{account_.second->getId(), bad}, {externalAccount_.second->getId(), other}};
    }

    void writeLegacy(const std::map<dht::InfoHash, Contact>& contacts)
    {
        std::ofstream file;
        file.exceptions(std::ios::failbit | std::ios::badbit);
        file.open(root_ / "contacts", std::ios::binary);
        msgpack::pack(file, contacts);
    }

    void testMutationGuards()
    {
        auto& contacts = *manager_->getInfo()->contacts;
        contacts.setContacts({});
        const auto before = fileutils::loadFile(root_ / "contacts");
        const auto self = account_.second->getId().toString();
        manager_->removeContact(self, true);
        CPPUNIT_ASSERT(contacts.getContacts().empty());
        CPPUNIT_ASSERT_EQUAL(0u, manager_->syncChanges);
        CPPUNIT_ASSERT(manager_->removed.empty());
        CPPUNIT_ASSERT(!manager_->setCertificateStatus(self, Status::BANNED));
        CPPUNIT_ASSERT(!manager_->setCertificateStatus(account_.second->getLongId().toString(), Status::BANNED));
        CPPUNIT_ASSERT(!manager_->setCertificateStatus(account_.second, Status::BANNED, false));
        CPPUNIT_ASSERT(fileutils::loadFile(root_ / "contacts") == before);
        CPPUNIT_ASSERT(manager_->isAllowed(*linked_.second, false));
        CPPUNIT_ASSERT(!manager_->isAllowed(*external_.second, false)); // shared CA is not account ownership

        CPPUNIT_ASSERT(manager_->setCertificateStatus(linked_.second->getLongId().toString(), Status::BANNED));
        CPPUNIT_ASSERT(!manager_->isAllowed(*linked_.second, true));
        manager_->removeContact(externalAccount_.second->getId().toString(), true);
        CPPUNIT_ASSERT(manager_->getContactInfo(externalAccount_.second->getId().toString())->isBanned());
        CPPUNIT_ASSERT(!manager_->isAllowed(*external_.second, true));
        manager_->removeContact(self, false);
        CPPUNIT_ASSERT(!manager_->getContactInfo(self)->isActive());
        CPPUNIT_ASSERT(!manager_->getContactInfo(self)->banned);
        CPPUNIT_ASSERT(!manager_->isAllowed(*revoked_.second, true));
    }

    void testLoadRepairAndReplay()
    {
        auto original = legacy();
        const auto self = account_.second->getId();
        auto& contacts = *manager_->getInfo()->contacts;
        writeLegacy(original);
        // Reproduce the live permission produced by the old ingestion code.
        contacts.trust_->setCertificateStatus(account_.second, Status::BANNED, false);
        CPPUNIT_ASSERT(manager_->setCertificateStatus(linked_.second, Status::BANNED, false));
        CPPUNIT_ASSERT(!manager_->isAllowed(*local_.second, false));
        manager_->reloadContacts();
        auto repaired = contacts.getContactInfo(self).value();
        CPPUNIT_ASSERT(!repaired.isActive() && !repaired.banned);
        CPPUNIT_ASSERT(repaired.added == original.at(self).added);
        CPPUNIT_ASSERT(repaired.confirmed == original.at(self).confirmed);
        CPPUNIT_ASSERT(repaired.conversationId == original.at(self).conversationId);
        CPPUNIT_ASSERT(toSecondsSinceEpoch(repaired.removed) > toSecondsSinceEpoch(original.at(self).removed));
        CPPUNIT_ASSERT(manager_->isAllowed(*local_.second, false));
        CPPUNIT_ASSERT(!manager_->isAllowed(*linked_.second, true)); // explicit device ban
        CPPUNIT_ASSERT(!manager_->isAllowed(*revoked_.second, true)); // CRL
        CPPUNIT_ASSERT(!contacts.isValidAccountDevice(*revoked_.second));
        CPPUNIT_ASSERT(!manager_->isAllowed(*external_.second, true));
        auto stored = ContactList::contactsFromPath(root_);
        CPPUNIT_ASSERT(stored.at(self).toJson() == repaired.toJson());
        CPPUNIT_ASSERT(stored.at(externalAccount_.second->getId()).toJson()
                       == original.at(externalAccount_.second->getId()).toJson());
        CPPUNIT_ASSERT_EQUAL(1u, manager_->syncChanges);
        CPPUNIT_ASSERT(manager_->added.empty() && manager_->removed.empty());

        const auto bytes = fileutils::loadFile(root_ / "contacts");
        DeviceSync stale;
        stale.peers = original;
        manager_->onSyncData(std::move(stale), false);
        manager_->reloadContacts();
        CPPUNIT_ASSERT_EQUAL(1u, manager_->syncChanges);
        CPPUNIT_ASSERT(fileutils::loadFile(root_ / "contacts") == bytes);
        CPPUNIT_ASSERT(manager_->added.empty() && manager_->removed.empty());

        auto newerBad = original.at(self);
        newerBad.removed = repaired.removed + 10s;
        DeviceSync newer;
        newer.peers.emplace(self, newerBad);
        manager_->onSyncData(std::move(newer), false);
        CPPUNIT_ASSERT_EQUAL(2u, manager_->syncChanges);
        CPPUNIT_ASSERT(!contacts.getContactInfo(self)->isBanned());
        CPPUNIT_ASSERT(contacts.getContactInfo(self)->removed > newerBad.removed);
        CPPUNIT_ASSERT(!manager_->isAllowed(*revoked_.second, true));
        CPPUNIT_ASSERT(manager_->setCertificateStatus(ca_.second, Status::BANNED, false));
        manager_->reloadContacts();
        CPPUNIT_ASSERT(!manager_->isAllowed(*local_.second, true)); // higher authority ban is untouched
    }

    void testArchiveAndFreshImport()
    {
        AccountArchive archive;
        archive.id = account_;
        archive.ca_key = ca_.first;
        archive.contacts = legacy();
        AccountArchive imported(std::string_view(archive.serialize()));
        const auto self = account_.second->getId();
        CPPUNIT_ASSERT(ContactList::normalizeSelfContact(self, imported.contacts));
        const auto canonical = imported.contacts.at(self).toJson();
        CPPUNIT_ASSERT(!ContactList::normalizeSelfContact(self, imported.contacts));
        manager_->getInfo()->contacts->setContacts(imported.contacts);
        CPPUNIT_ASSERT(manager_->getContactInfo(self.toString())->toJson() == canonical);
        CPPUNIT_ASSERT(!manager_->getContactInfo(self.toString())->isActive());
        CPPUNIT_ASSERT(manager_->added.empty() && manager_->removed.empty());
        CPPUNIT_ASSERT(!manager_->isAllowed(*external_.second, true));
        CPPUNIT_ASSERT(!manager_->isAllowed(*revoked_.second, true));
        CPPUNIT_ASSERT(ContactList::contactsFromPath(root_).at(self).toJson() == canonical);
        // An archive ingestion that hasn't pre-normalized also uses the invariant.
        manager_->getInfo()->contacts->setContacts(archive.contacts);
        CPPUNIT_ASSERT(!manager_->getContactInfo(self.toString())->isBanned());
    }

    void testFailedPersistenceKeepsOldState()
    {
        auto original = legacy();
        writeLegacy(original);
        auto& contacts = *manager_->getInfo()->contacts;
        contacts.trust_->setCertificateStatus(account_.second, Status::BANNED, false);
        auto before = fileutils::loadFile(root_ / "contacts");
        std::filesystem::create_directory(root_ / "contacts.new");
        CPPUNIT_ASSERT_THROW(manager_->reloadContacts(), std::exception);
        CPPUNIT_ASSERT(fileutils::loadFile(root_ / "contacts") == before);
        CPPUNIT_ASSERT(!manager_->isAllowed(*local_.second, false));
        CPPUNIT_ASSERT_EQUAL(0u, manager_->syncChanges);
        CPPUNIT_ASSERT(contacts.getContacts().empty());
        auto overflow = original;
        overflow.at(account_.second->getId()).removed = TimePoint::max();
        CPPUNIT_ASSERT_THROW(ContactList::normalizeSelfContact(account_.second->getId(), overflow),
                             std::overflow_error);
    }

    CPPUNIT_TEST_SUITE(SelfContactTest);
    CPPUNIT_TEST(testMutationGuards);
    CPPUNIT_TEST(testLoadRepairAndReplay);
    CPPUNIT_TEST(testArchiveAndFreshImport);
    CPPUNIT_TEST(testFailedPersistenceKeepsOldState);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(SelfContactTest, SelfContactTest::name());
} // namespace jami::test

CORE_TEST_RUNNER(jami::test::SelfContactTest::name())
