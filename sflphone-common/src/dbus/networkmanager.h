/*
 *  Copyright (C) 2004-2009 Savoir-Faire Linux inc.
 *  Author: Julien Bonjean <julien.bonjean@savoirfairelinux.com>
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

#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include "networkmanager_proxy.h"

using namespace std;

class NetworkManager
: public org::freedesktop::NetworkManager_proxy,
  public DBus::IntrospectableProxy,
  public DBus::ObjectProxy
{
public:

    NetworkManager(DBus::Connection&, const DBus::Path&, const char*);
    void StateChanged(const uint32_t& state);
    void PropertiesChanged(const std::map< std::string, ::DBus::Variant >& argin0);
    string stateAsString(const uint32_t& state);

    enum NMState
    {
        NM_STATE_UNKNOWN = 0,
        NM_STATE_ASLEEP,
        NM_STATE_CONNECTING,
        NM_STATE_CONNECTED,
        NM_STATE_DISCONNECTED
    };

   static const string statesString[5];
};
#endif

