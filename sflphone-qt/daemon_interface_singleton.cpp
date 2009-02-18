#include "daemon_interface_singleton.h"


DaemonInterface * DaemonInterfaceSingleton::daemon = new DaemonInterface("org.sflphone.SFLphone", "/org/sflphone/SFLphone/ConfigurationManager", QDBusConnection::sessionBus());


DaemonInterface & DaemonInterfaceSingleton::getInstance(){
	if(!daemon){
		daemon = new DaemonInterface("org.sflphone.SFLphone", "/org/sflphone/SFLphone/ConfigurationManager", QDBusConnection::sessionBus());
	}
	return *daemon;
}
	