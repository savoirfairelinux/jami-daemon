#include "configurationmanager_interface_singleton.h"


ConfigurationManagerInterface * ConfigurationManagerInterfaceSingleton::interface = new ConfigurationManagerInterface("org.sflphone.SFLphone", "/org/sflphone/SFLphone/ConfigurationManager", QDBusConnection::sessionBus());


ConfigurationManagerInterface & ConfigurationManagerInterfaceSingleton::getInstance(){
	if(!interface){
		interface = new ConfigurationManagerInterface("org.sflphone.SFLphone", "/org/sflphone/SFLphone/ConfigurationManager", QDBusConnection::sessionBus());
	}
	if(!interface->isValid())
		throw "Error : sflphoned not connected";
	return *interface;
}
	