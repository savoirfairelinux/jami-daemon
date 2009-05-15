#include "callmanager_interface_singleton.h"


CallManagerInterface * CallManagerInterfaceSingleton::interface 
    = new CallManagerInterface(
            "org.sflphone.SFLphone", 
            "/org/sflphone/SFLphone/CallManager", 
            QDBusConnection::sessionBus());


CallManagerInterface & CallManagerInterfaceSingleton::getInstance(){
	if(!interface){
		interface = new CallManagerInterface(
		             "org.sflphone.SFLphone", 
		             "/org/sflphone/SFLphone/CallManager", 
		             QDBusConnection::sessionBus());
	}
	if(!interface->isValid())
	{
		throw "Error : sflphoned not connected. Service " + interface->service() + " not connected. From call manager interface.";
		
	}
	return *interface;
}
