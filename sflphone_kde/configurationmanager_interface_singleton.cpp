#include "configurationmanager_interface_singleton.h"


ConfigurationManagerInterface * ConfigurationManagerInterfaceSingleton::daemon = new ConfigurationManagerInterface("org.sflphone.SFLphone", "/org/sflphone/SFLphone/ConfigurationManager", QDBusConnection::sessionBus());


ConfigurationManagerInterface & ConfigurationManagerInterfaceSingleton::getInstance(){
	if(!daemon){
		daemon = new ConfigurationManagerInterface("org.sflphone.SFLphone", "/org/sflphone/SFLphone/ConfigurationManager", QDBusConnection::sessionBus());
	}
	if(!daemon->isValid())
		throw "Error : sflphoned not connected";
	return *daemon;
}
	