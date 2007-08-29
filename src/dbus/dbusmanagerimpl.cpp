/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
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
 
#include <dbusmanagerimpl.h>
#include "manager.h"

const char* DBusManagerImpl::SERVER_NAME = "org.sflphone.SFLPhone";

int 
DBusManagerImpl::exec(){
    
	DBus::default_dispatcher = &_dispatcher;

	DBus::Connection conn = DBus::Connection::SessionBus();
	conn.request_name(SERVER_NAME);

	_callManager = new CallManager(conn);
    //_configurationManager = new ConfigurationManager(conn);
    
    Manager::instance().getEvents();  // Register accounts
    
    _debug("Starting DBus event loop\n");
	_dispatcher.enter();

	return 1;
}
        
    
