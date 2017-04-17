
#include "contactsmanager.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <json/json.h>

#include <unistd.h>

#include <algorithm>
#include <array>
#include <memory>
#include <sstream>
#include <cctype>
#include <cstdarg>
#include <string>
#include <fstream>

#include "string_utils.h"
#include "logger.h"
#include "fileutils.h"

#include "contact.h"
#include <typeinfo>
namespace ring {


ContactsManager::ContactsManager(const std::string& accountID) 
	: accountID_ (accountID) ,idPath_(fileutils::get_data_dir()+DIR_SEPARATOR_STR+accountID){

}

Contact
ContactsManager::addContact(const std::string& uri, bool confirmed)
{
    dht::InfoHash h (uri);
    auto c = contacts_.find(h);
    if (c == contacts_.end())
        c = contacts_.emplace(h, Contact{}).first;
    else if (c->second.isActive() and c->second.confirmed == confirmed)
        return c->second;
    c->second.added = std::time(nullptr);
    c->second.confirmed = confirmed or c->second.confirmed;
	return c->second;
}

Contact
ContactsManager::removeContact(const std::string& uri, bool ban)
{
    dht::InfoHash h (uri);
    auto c = contacts_.find(h);
    if (c == contacts_.end())
        c = contacts_.emplace(h, Contact{}).first;
    else if (not c->second.isActive() and c->second.banned == ban)
        return c->second;
    c->second.removed = std::time(nullptr);
    c->second.banned = ban;
    return c->second;
    }


std::vector<std::map<std::string, std::string>>
ContactsManager::getContacts() const
{
    std::vector<std::map<std::string, std::string>> ret;
    ret.reserve(contacts_.size());
    for (const auto& c : contacts_) {
        if (not (c.second.isActive() or c.second.isBanned()))
            continue;
        std::map<std::string, std::string> cm {
            {"id", c.first.toString()},
            {"added", std::to_string(c.second.added)}
        };
        if (c.second.isActive())
            cm.emplace("confirmed", c.second.confirmed ? TRUE_STR : FALSE_STR);
        else if (c.second.isBanned())
            cm.emplace("banned", TRUE_STR);
        ret.emplace_back(std::move(cm));
    }
    return ret;
}

bool
ContactsManager::updateContact(const dht::InfoHash& id, const Contact& contact, Contact& returnedContact)
{

    bool stateChanged {false};
    auto c = contacts_.find(id);
	
    if (c == contacts_.end()) {
        RING_DBG("[Account %s] new contact: %s",accountID_ , id.toString().c_str());
        c = contacts_.emplace(id, contact).first;
        stateChanged = c->second.isActive() or c->second.isBanned();
    } else {
        RING_DBG("[Account %s] updated contact: %s", accountID_, id.toString().c_str());
        stateChanged = c->second.update(contact);
    }
	
	returnedContact = c->second;
    
	return stateChanged;
}

void
ContactsManager::loadContacts()
{
    decltype(contacts_) contacts;
    try {
        // read file
        auto file = fileutils::loadFile("contacts", idPath_);
        // load values
        msgpack::object_handle oh = msgpack::unpack((const char*)file.data(), file.size());
        oh.get().convert(contacts);
    } catch (const std::exception& e) {
        RING_WARN("[Account %s] error loading contacts: %s", accountID_, e.what());
        return;
    }
	Contact contactToReturn;
    for (auto& peer : contacts)
        updateContact(peer.first, peer.second,contactToReturn);
}

void
ContactsManager::saveContacts() const
{
    std::ofstream file(idPath_+DIR_SEPARATOR_STR "contacts", std::ios::trunc | std::ios::binary);
    msgpack::pack(file, contacts_);
}

}
