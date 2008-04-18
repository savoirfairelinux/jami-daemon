/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
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
#include <instance.h>
#include "../manager.h"

const char* Instance::SERVER_PATH = "/org/sflphone/SFLphone/Instance";

Instance::Instance( DBus::Connection& connection )
: DBus::ObjectAdaptor(connection, SERVER_PATH)
{
  count = 0;
}

void
Instance::Register( const ::DBus::Int32& pid, 
                     const ::DBus::String& name )
{
    _debug("Instance::register received\n");
    count++;
}


void
Instance::Unregister( const ::DBus::Int32& pid )
{
    _debug("Instance::unregister received\n");
    count --;
    if(count <= 0)
    {
      _debug("0 client running, quitting...");
      DBusManager::instance().exit();
    }
}

::DBus::Int32 
Instance::getRegistrationCount( void )
{
  _debug("Instance::getRegistrationCount\n");
  return count;
}

