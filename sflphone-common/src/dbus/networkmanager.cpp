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

//	if(state == NM_STATE_CONNECTED)
//		Manager::instance().registerAccounts();
}

void NetworkManager::PropertiesChanged(const std::map< std::string, ::DBus::Variant >& argin0)
{

	const map< string, ::DBus::Variant >::const_iterator iter = argin0.begin();

	_warn("Properties changed: %s", iter->first.c_str());

	Manager::instance().registerAccounts();
}

NetworkManager::NetworkManager(DBus::Connection& connection, const DBus::Path& path, const char* destination): DBus::ObjectProxy (connection, path, destination)
{
}

