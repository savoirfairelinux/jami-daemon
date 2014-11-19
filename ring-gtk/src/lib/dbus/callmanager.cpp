/****************************************************************************
 *   Copyright (C) 2009-2014 by Savoir-Faire Linux                          *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>          *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com> *
 *                                                                          *
 *   This library is free software; you can redistribute it and/or          *
 *   modify it under the terms of the GNU Lesser General Public             *
 *   License as published by the Free Software Foundation; either           *
 *   version 2.1 of the License, or (at your option) any later version.     *
 *                                                                          *
 *   This library is distributed in the hope that it will be useful,        *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU      *
 *   Lesser General Public License for more details.                        *
 *                                                                          *
 *   You should have received a copy of the GNU General Public License      *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/

#include "callmanager.h"

CallManagerInterface * DBus::CallManager::interface = nullptr;

CallManagerInterface & DBus::CallManager::instance(){
   if (!dbus_metaTypeInit) registerCommTypes();
   if (!interface)
      interface = new CallManagerInterface( "org.sflphone.SFLphone", "/org/sflphone/SFLphone/CallManager", QDBusConnection::sessionBus());
   if(!interface->connection().isConnected())
      throw "Error : sflphoned not connected. Service " + interface->service() + " not connected. From call manager interface.";
   if (!interface->isValid())
      throw "SFLphone daemon not available, be sure it running";
   return *interface;
}
