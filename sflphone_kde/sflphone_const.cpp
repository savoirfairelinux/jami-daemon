#include "sflphone_const.h"

int getProtocolIndexByName(QString protocolName)
{
	if(protocolName == (QString)"SIP")
		return 0;
	if(protocolName == (QString)"IAX")
		return 1;
	return -1;
}

QString getProtocolNameByIndex(int protocolIndex)
{
	if(protocolIndex == 0)
		return "SIP";
	if(protocolIndex == 1)
		return "IAX";
	return "UNKNOWN PROTOCOLE INDEX";
}