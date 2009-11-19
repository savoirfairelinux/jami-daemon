#include <global.h>
#include "networkmanager.h"
#include <iostream>
#include <instance.h>
#include "../manager.h"
#include "sip/sipvoiplink.h"

using namespace std;

const string NetworkManager::statesString[5] = {"unknown", "asleep", "connecting", "connected", "disconnected"};

string NetworkManager::stateAsString(const uint32_t& state)
{
	return statesString[state];
}

void NetworkManager::StateChanged(const uint32_t& state)
{
	_warn("Network state changed: %s", stateAsString(state).c_str());
}

void NetworkManager::PropertiesChanged(const std::map< std::string, ::DBus::Variant >& argin0)
{
	const map< string, ::DBus::Variant >::const_iterator iter = argin0.begin();

	string message = iter->first;

	_warn("Properties changed: %s", iter->first.c_str());
/*
	DBus::Variant variant = iter->second;
	DBus::MessageIter i = variant.reader();
	cout << i.type() << endl;// 97
	cout << (char )i.type() << endl;
	cout << (char)i.array_type() << endl;

	cout << i.is_array() << endl;// 1
	cout << i.is_dict() << endl;// 0
	cout << i.array_type() << endl;// 111

	int size;
	::DBus::Path* value = new ::DBus::Path[10];
	size = i.get_array(value);
	cout << "length: " << size << endl;

	while (!i.at_end())
	{
		char **array = new char*[2];
		size_t length = i.get_array(&array);
		cout << "length: " << length << endl;
		i = i.recurse();
	}
*/
	Manager::instance().registerAccounts();
}

NetworkManager::NetworkManager(DBus::Connection& connection, const DBus::Path& path, const char* destination): DBus::ObjectProxy (connection, path, destination)
{
}

