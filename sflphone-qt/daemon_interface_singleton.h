#include "daemon_interface_p.h"

class DaemonInterfaceSingleton
{

private:

	static DaemonInterface * daemon;

public:

	//TODO verifier pointeur ou pas pour singleton en c++
	static DaemonInterface & getInstance();

};
