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

#ifndef __DBUSMANAGERIMPL_H__
#define __DBUSMANAGERIMPL_H__

#include "callmanager.h"
#include "configurationmanager.h"
#include "instance.h"

class DBusManagerImpl {
    public:
        CallManager * getCallManager(){ return _callManager; };
        ConfigurationManager * getConfigurationManager(){ return _configurationManager; };
        int exec();
        void exit();
        static const char* SERVER_NAME;
        
    private:
        CallManager*          _callManager;
        ConfigurationManager* _configurationManager;
        Instance*             _instanceManager;
        DBus::BusDispatcher   _dispatcher;
};

#endif
