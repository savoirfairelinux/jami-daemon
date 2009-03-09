#ifndef CALL_MANAGER_INTERFACE_SINGLETON_H
#define CALL_MANAGER_INTERFACE_SINGLETON_H

#include "callmanager_interface_p.h"

class CallManagerInterfaceSingleton
{

private:

	static CallManagerInterface * daemon;

public:

	//TODO verifier pointeur ou pas pour singleton en c++
	static CallManagerInterface & getInstance();

};

#endif