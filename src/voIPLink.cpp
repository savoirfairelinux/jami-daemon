/**
 *  Copyright (C) 2005 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author : Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *                                                                              
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *                                                                              
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *                                                                              
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <string>

#include "user_cfg.h"
#include "voIPLink.h"
#include "manager.h"

VoIPLink::VoIPLink ()
{
	initConstructor();
}

VoIPLink::~VoIPLink (void) 
{
}

void
VoIPLink::setType (VoIPLinkType type) 
{
	_type = type;
}

VoIPLinkType
VoIPLink::getType (void)
{
	return _type;
}

void 
VoIPLink::setFullName (const std::string& fullname)
{
	_fullname = fullname;
}

std::string
VoIPLink::getFullName (void)
{
	return _fullname;
}

void 
VoIPLink::setHostName (const std::string& hostname)
{
	_hostname = hostname;
}

std::string
VoIPLink::getHostName (void)
{
	return _hostname;
} 

void 
VoIPLink::setLocalIpAddress (const std::string& ipAdress)
{
	_localIpAddress = ipAdress;
}

std::string 
VoIPLink::getLocalIpAddress (void)
{
	return _localIpAddress;
} 

void
VoIPLink::initConstructor(void)
{
	_type = Sip;
  // TODO: should be inside the account
	_fullname =
Manager::instance().getConfigString(SIGNALISATION,FULL_NAME
) ;
	_hostname = Manager::instance().getConfigString(SIGNALISATION,HOST_PART);
	_localIpAddress = "";
}
