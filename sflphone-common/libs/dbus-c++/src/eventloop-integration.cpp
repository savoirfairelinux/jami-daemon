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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <dbus-c++/eventloop-integration.h>
#include <dbus-c++/debug.h>

#include <sys/poll.h>

#include <dbus/dbus.h>

using namespace DBus;

BusTimeout::BusTimeout(Timeout::Internal *ti, BusDispatcher *bd)
: Timeout(ti), DefaultTimeout(Timeout::interval(), true, bd)
{
	DefaultTimeout::enabled(Timeout::enabled());
}

void BusTimeout::toggle()
{
	debug_log("timeout %p toggled (%s)", this, Timeout::enabled() ? "on":"off");

	DefaultTimeout::enabled(Timeout::enabled());
}

BusWatch::BusWatch(Watch::Internal *wi, BusDispatcher *bd)
: Watch(wi), DefaultWatch(Watch::descriptor(), 0, bd)
{
	int flags = POLLHUP | POLLERR;

	if (Watch::flags() & DBUS_WATCH_READABLE)
		flags |= POLLIN;
	if (Watch::flags() & DBUS_WATCH_WRITABLE)
		flags |= POLLOUT;

	DefaultWatch::flags(flags);
	DefaultWatch::enabled(Watch::enabled());
}

void BusWatch::toggle()
{
	debug_log("watch %p toggled (%s)", this, Watch::enabled() ? "on":"off");

	DefaultWatch::enabled(Watch::enabled());
}

void BusDispatcher::enter()
{
	debug_log("entering dispatcher %p", this);

	_running = true;

	while (_running)
	{
		do_iteration();
	}

	debug_log("leaving dispatcher %p", this);
}

void BusDispatcher::leave()
{
	_running = false;
}

void BusDispatcher::do_iteration()
{
	dispatch_pending();
	dispatch();
}

Timeout *BusDispatcher::add_timeout(Timeout::Internal *ti)
{
	BusTimeout *bt = new BusTimeout(ti, this);

	bt->expired = new Callback<BusDispatcher, void, DefaultTimeout &>(this, &BusDispatcher::timeout_expired);
	bt->data(bt);

	debug_log("added timeout %p (%s) interval=%d",
		bt, ((Timeout *)bt)->enabled() ? "on":"off", ((Timeout *)bt)->interval());

	return bt;
}

void BusDispatcher::rem_timeout(Timeout *t)
{
	debug_log("removed timeout %p", t);

	delete t;
}

Watch *BusDispatcher::add_watch(Watch::Internal *wi)
{
	BusWatch *bw = new BusWatch(wi, this);

	bw->ready = new Callback<BusDispatcher, void, DefaultWatch &>(this, &BusDispatcher::watch_ready);
	bw->data(bw);

	debug_log("added watch %p (%s) fd=%d flags=%d",
		bw, ((Watch *)bw)->enabled() ? "on":"off", ((Watch *)bw)->descriptor(), ((Watch *)bw)->flags());

	return bw;
}

void BusDispatcher::rem_watch(Watch *w)
{
	debug_log("removed watch %p", w);

	delete w;
}

void BusDispatcher::timeout_expired(DefaultTimeout &et)
{
	debug_log("timeout %p expired", &et);

	BusTimeout *timeout = reinterpret_cast<BusTimeout *>(et.data());

	timeout->handle();
}

void BusDispatcher::watch_ready(DefaultWatch &ew)
{
	BusWatch *watch = reinterpret_cast<BusWatch *>(ew.data());

	debug_log("watch %p ready, flags=%d state=%d",
		watch, ((Watch *)watch)->flags(), watch->state()
	);

	int flags = 0;

	if (watch->state() & POLLIN)
		flags |= DBUS_WATCH_READABLE;
	if (watch->state() & POLLOUT)
		flags |= DBUS_WATCH_WRITABLE;
	if (watch->state() & POLLHUP)
		flags |= DBUS_WATCH_HANGUP;
	if (watch->state() & POLLERR)
		flags |= DBUS_WATCH_ERROR;

	watch->handle(flags);
}

