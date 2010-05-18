/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#ifdef USE_NETWORKMANAGER

#include <global.h>
#include "networkmanager.h"
#include <iostream>
#include <instance.h>
#include "../manager.h"

using namespace std;

const string NetworkManager::statesString[5] = {"unknown", "asleep", "connecting", "connected", "disconnected"};

string NetworkManager::stateAsString(const uint32_t& state)
{
	return statesString[state];
}

void NetworkManager::StateChanged(const uint32_t& state)
{
	_warn("Network state changed: %s", stateAsString(state).c_str());
}

void NetworkManager::PropertiesChanged(const std::map< std::string, ::DBus::Variant >& argin0)
{
	const map< string, ::DBus::Variant >::const_iterator iter = argin0.begin();

	string message = iter->first;

	_warn("Properties changed: %s", iter->first.c_str());
/*
	DBus::Variant variant = iter->second;
	DBus::MessageIter i = variant.reader();
	cout << i.type() << endl;// 97
	cout << (char )i.type() << endl;
	cout << (char)i.array_type() << endl;

	cout << i.is_array() << endl;// 1
	cout << i.is_dict() << endl;// 0
	cout << i.array_type() << endl;// 111

	int size;
	::DBus::Path* value = new ::DBus::Path[10];
	size = i.get_array(value);
	cout << "length: " << size << endl;

	while (!i.at_end())
	{
		char **array = new char*[2];
		size_t length = i.get_array(&array);
		cout << "length: " << length << endl;
		i = i.recurse();
	}
*/
	Manager::instance().registerAccounts();
}

NetworkManager::NetworkManager(DBus::Connection& connection, const DBus::Path& path, const char* destination): DBus::ObjectProxy (connection, path, destination)
{
}

#endif
