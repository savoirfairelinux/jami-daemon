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


#ifndef __DBUSXX_DISPATCHER_H
#define __DBUSXX_DISPATCHER_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "api.h"
#include "connection.h"
#include "eventloop.h"

namespace DBus {

class DXXAPI Timeout
{
public:

	class Internal;

	Timeout( Internal* i );

	virtual ~Timeout(){}

	int interval() const;

	bool enabled() const;

	bool handle();

	virtual void toggle() = 0;

private:

	DXXAPILOCAL Timeout( const Timeout& );

private:

	Internal* _int;
};

class DXXAPI Watch
{
public:

	class Internal;

	Watch( Internal* i );

	virtual ~Watch(){}

	int descriptor() const;

	int flags() const;

	bool enabled() const;

	bool handle( int flags );

	virtual void toggle() = 0;

private:

	DXXAPILOCAL Watch( const Watch& );

private:

	Internal* _int;
};

class DXXAPI Dispatcher
{
public:

	virtual ~Dispatcher()
	{}

	void queue_connection( Connection::Private* );

	void dispatch_pending();

	virtual void enter() = 0;

	virtual void leave() = 0;

	virtual Timeout* add_timeout( Timeout::Internal* ) = 0;

	virtual void rem_timeout( Timeout* ) = 0;

	virtual Watch* add_watch( Watch::Internal* ) = 0;

	virtual void rem_watch( Watch* ) = 0;

	struct Private;

private:

	DefaultMutex _mutex_p;
	Connection::PrivatePList _pending_queue;
};

extern DXXAPI Dispatcher* default_dispatcher;

/* classes for multithreading support
*/

class DXXAPI Mutex
{
public:

	virtual ~Mutex() {}

	virtual void lock() = 0;

	virtual void unlock() = 0;

	struct Internal;

protected:

	Internal* _int;
};

class DXXAPI CondVar
{
public:

	virtual ~CondVar() {}

	virtual void wait( Mutex* ) = 0;

	virtual bool wait_timeout( Mutex*, int timeout ) = 0;

	virtual void wake_one() = 0;

	virtual void wake_all() = 0;

	struct Internal;

protected:

	Internal* _int;
};

#ifndef DBUS_HAS_RECURSIVE_MUTEX
typedef Mutex* (*MutexNewFn)();
typedef bool (*MutexFreeFn)( Mutex* mx );
typedef bool (*MutexLockFn)( Mutex* mx );
typedef void (*MutexUnlockFn)( Mutex* mx );
#else
typedef Mutex* (*MutexNewFn)();
typedef void (*MutexFreeFn)( Mutex* mx );
typedef void (*MutexLockFn)( Mutex* mx );
typedef void (*MutexUnlockFn)( Mutex* mx );
#endif//DBUS_HAS_RECURSIVE_MUTEX

typedef CondVar* (*CondVarNewFn)();
typedef void (*CondVarFreeFn)( CondVar* cv );
typedef void (*CondVarWaitFn)( CondVar* cv, Mutex* mx );
typedef bool (*CondVarWaitTimeoutFn)( CondVar* cv, Mutex* mx, int timeout );
typedef void (*CondVarWakeOneFn)( CondVar* cv );
typedef void (*CondVarWakeAllFn)( CondVar* cv );

#ifdef DBUS_HAS_THREADS_INIT_DEFAULT
void DXXAPI _init_threading();
#endif//DBUS_HAS_THREADS_INIT_DEFAULT

void DXXAPI _init_threading(
	MutexNewFn, MutexFreeFn, MutexLockFn, MutexUnlockFn,
	CondVarNewFn, CondVarFreeFn, CondVarWaitFn, CondVarWaitTimeoutFn, CondVarWakeOneFn, CondVarWakeAllFn
);

template<class Mx, class Cv>
struct Threading
{
	static void init()
	{
		_init_threading(
			mutex_new, mutex_free, mutex_lock, mutex_unlock,
			condvar_new, condvar_free, condvar_wait, condvar_wait_timeout, condvar_wake_one, condvar_wake_all
		);
	}

	static Mutex* mutex_new()
	{
		return new Mx;
	}

	static void mutex_free( Mutex* mx )
	{
		delete mx;
	}

	static void mutex_lock( Mutex* mx )
	{
		mx->lock();
	}

	static void mutex_unlock( Mutex* mx )
	{
		mx->unlock();
	}

	static CondVar* condvar_new()
	{
		return new Cv;
	}

	static void condvar_free( CondVar* cv )
	{
		delete cv;
	}

	static void condvar_wait( CondVar* cv, Mutex* mx )
	{
		cv->wait(mx);
	}

	static bool condvar_wait_timeout( CondVar* cv, Mutex* mx, int timeout )
	{
		return cv->wait_timeout(mx, timeout);
	}

	static void condvar_wake_one( CondVar* cv )
	{
		cv->wake_one();
	}

	static void condvar_wake_all( CondVar* cv )
	{
		cv->wake_all();
	}
};

} /* namespace DBus */

#endif//__DBUSXX_DISPATCHER_H
