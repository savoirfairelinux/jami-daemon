#include "networkmanager.h"
#include <iostream>

using namespace std;

const string NetworkManager::statesString[5] = {"unknown", "asleep", "connecting", "connected", "disconnected"};

string NetworkManager::stateAsString(const uint32_t& state)
{
	return statesString[state];
}

void NetworkManager::StateChanged(const uint32_t& state)
{
	std::cout << "state changed: " << stateAsString(state) << std::endl;
}
	
NetworkManager::NetworkManager(DBus::Connection& connection, const DBus::Path& path, const char* destination): DBus::ObjectProxy (connection, path, destination)
{
}

