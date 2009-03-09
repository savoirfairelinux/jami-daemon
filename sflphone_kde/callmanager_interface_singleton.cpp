#include "callmanager_interface_singleton.h"


CallManagerInterface * CallManagerInterfaceSingleton::daemon = new CallManagerInterface("org.sflphone.SFLphone", "/org/sflphone/SFLphone/CallManager", QDBusConnection::sessionBus());


CallManagerInterface & CallManagerInterfaceSingleton::getInstance(){
	if(!daemon){
		daemon = new CallManagerInterface("org.sflphone.SFLphone", "/org/sflphone/SFLphone/CallManager", QDBusConnection::sessionBus());
	}
	if(!daemon->isValid())
		throw "Error : sflphoned not connected";
	return *daemon;
}
