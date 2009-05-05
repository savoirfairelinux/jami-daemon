#ifndef CONFIGURATION_MANAGER_INTERFACE_SINGLETON_H
#define CONFIGURATION_MANAGER_INTERFACE_SINGLETON_H

#include "configurationmanager_dbus_interface.h"


class ConfigurationManagerInterfaceSingleton
{

private:

	static ConfigurationManagerInterface * interface;

public:

	//TODO verifier pointeur ou pas pour singleton en c++
	static ConfigurationManagerInterface & getInstance();

};

#endif