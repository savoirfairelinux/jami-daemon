#include "sflphone_const.h"

int getProtocoleIndex(QString protocoleName)
{
	if(protocoleName == (QString)"SIP")
		return 0;
	if(protocoleName == (QString)"IAX")
		return 1;
	return -1;
}

QString getIndexProtocole(int protocoleIndex)
{
	if(protocoleIndex == 0)
		return "SIP";
	if(protocoleIndex == 1)
		return "IAX";
	return "UNKNOWN PROTOCOLE INDEX";
}