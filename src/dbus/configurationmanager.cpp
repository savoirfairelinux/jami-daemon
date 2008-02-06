/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc@squidy.info>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *                                                                              
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
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

#include <global.h>
#include <configurationmanager.h>
#include <sstream>
#include "../manager.h"

const char* ConfigurationManager::SERVER_PATH = "/org/sflphone/SFLphone/ConfigurationManager";



	ConfigurationManager::ConfigurationManager( DBus::Connection& connection )
: DBus::ObjectAdaptor(connection, SERVER_PATH)
{
}

	std::map< ::DBus::String, ::DBus::String > 
ConfigurationManager::getAccountDetails( const ::DBus::String& accountID )
{
	_debug("ConfigurationManager::getAccountDetails received\n");
	return Manager::instance().getAccountDetails(accountID);
}

	void 
ConfigurationManager::setAccountDetails( const ::DBus::String& accountID, 
		const std::map< ::DBus::String, ::DBus::String >& details )
{
	_debug("ConfigurationManager::setAccountDetails received\n");
	Manager::instance().setAccountDetails(accountID, details);
}

	void 
ConfigurationManager::addAccount( const std::map< ::DBus::String, ::DBus::String >& details )
{
	_debug("ConfigurationManager::addAccount received\n");
	Manager::instance().addAccount(details);
}


	void 
ConfigurationManager::removeAccount( const ::DBus::String& accoundID )
{
	_debug("ConfigurationManager::removeAccount received\n");
	return Manager::instance().removeAccount(accoundID);
}

std::vector< ::DBus::String > 
ConfigurationManager::getAccountList(  )
{
	_debug("ConfigurationManager::getAccountList received\n");
	return Manager::instance().getAccountList();
}


std::vector< ::DBus::String > 
ConfigurationManager::getToneLocaleList(  )
{
	_debug("ConfigurationManager::getToneLocaleList received\n");

}



	::DBus::String 
ConfigurationManager::getVersion(  )
{
	_debug("ConfigurationManager::getVersion received\n");

}


	std::vector< ::DBus::String > 
ConfigurationManager::getRingtoneList(  )
{
	_debug("ConfigurationManager::getRingtoneList received\n");

}



	std::vector< ::DBus::String  > 
ConfigurationManager::getCodecList(  )
{
	_debug("ConfigurationManager::getCodecList received\n");
	return Manager::instance().getCodecList();
}

	std::vector< ::DBus::String > 
ConfigurationManager::getCodecDetails( const ::DBus::Int32& payload )
{
	_debug("ConfigurationManager::getCodecList received\n");
	return Manager::instance().getCodecDetails( payload );
}

	std::vector< ::DBus::String > 
ConfigurationManager::getActiveCodecList(  )
{
	_debug("ConfigurationManager::getActiveCodecList received\n");
	return Manager::instance().getActiveCodecList();
}

	void 
ConfigurationManager::setActiveCodecList( const std::vector< ::DBus::String >& list )
{
	_debug("ConfigurationManager::setActiveCodecList received\n");
	 Manager::instance().setActiveCodecList(list);
}

	std::vector< ::DBus::String > 
ConfigurationManager::getPlaybackDeviceList(  )
{
	_debug("ConfigurationManager::getPlaybackDeviceList received\n");

}

	std::vector< ::DBus::String > 
ConfigurationManager::getRecordDeviceList(  )
{
	_debug("ConfigurationManager::getRecordDeviceList received\n");

}

	::DBus::String
ConfigurationManager::getDefaultAccount(  )
{
	_debug("ConfigurationManager::getDefaultAccount received\n");
	return Manager::instance().getDefaultAccount();
}

/*
 * used to set a default account
 */ 
	void 
ConfigurationManager::setDefaultAccount( const ::DBus::String& accountID )
{
	 _debug("ConfigurationManager::setDefaultAccount received\n");
	Manager::instance().setDefaultAccount(accountID);

}
