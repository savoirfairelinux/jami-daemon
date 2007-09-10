/*
 *
 *  D-Bus++ - C++ bindings for D-Bus
 *
 *  Copyright (C) 2005-2007  Paolo Durante <shackan@gmail.com>
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */


#ifndef __DBUSXX_CONNECTION_H
#define __DBUSXX_CONNECTION_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <list>

#include "api.h"
#include "types.h"
#include "util.h"
#include "message.h"
#include "pendingcall.h"

namespace DBus {

class Connection;

typedef Slot<bool, const Message&> MessageSlot;

typedef std::list<Connection>	ConnectionList;

class ObjectAdaptor;
class Dispatcher;

class DXXAPI Connection
{
public:

	static Connection SystemBus();

	static Connection SessionBus();

	static Connection ActivationBus();

	struct Private;

	typedef std::list<Private*> PrivatePList;

	Connection( Private* );

	Connection( const char* address, bool priv = true );

	Connection( const Connection& c );

	virtual ~Connection();

	Dispatcher* setup( Dispatcher* );

	bool operator == ( const Connection& ) const;

	void add_match( const char* rule );

	void remove_match( const char* rule );

	bool add_filter( MessageSlot& );

	void remove_filter( MessageSlot& );

	bool unique_name( const char* n );

	const char* unique_name() const;

	bool register_bus();

	bool connected() const;

	void disconnect();

	void exit_on_disconnect( bool exit );

	void flush();

	bool send( const Message&, unsigned int* serial = NULL );

	Message send_blocking( Message& msg, int timeout );

	PendingCall send_async( Message& msg, int timeout );

	void request_name( const char* name, int flags = 0 );

	bool has_name( const char* name );

	bool start_service( const char* name, unsigned long flags );

	const std::vector<std::string>& names();

private:

	DXXAPILOCAL void init();

private:

	RefPtrI<Private> _pvt;

friend class ObjectAdaptor; // needed in order to register object paths for a connection
};

} /* namespace DBus */

#endif//__DBUSXX_CONNECTION_H
