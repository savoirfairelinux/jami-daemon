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


#include <dbus-c++/glib-integration.h>

#include <dbus/dbus.h> // for DBUS_WATCH_*

using namespace DBus;

Glib::BusTimeout::BusTimeout( Timeout::Internal* ti, GMainContext* ctx )
: Timeout(ti), _ctx(ctx)
{
	_enable();
}

Glib::BusTimeout::~BusTimeout()
{
	_disable();
}

void Glib::BusTimeout::toggle()
{
	debug_log("glib: timeout %p toggled (%s)", this, Timeout::enabled() ? "on":"off");

	if(Timeout::enabled())	_enable();
	else			_disable();
}

gboolean Glib::BusTimeout::timeout_handler( gpointer data )
{
	Glib::BusTimeout* t = reinterpret_cast<Glib::BusTimeout*>(data);

	t->handle();

	return TRUE;
}

void Glib::BusTimeout::_enable()
{
	_source = g_timeout_source_new(Timeout::interval());
	g_source_set_callback(_source, timeout_handler, this, NULL);
	g_source_attach(_source, _ctx);
}

void Glib::BusTimeout::_disable()
{
	g_source_destroy(_source);
}

struct BusSource
{
	GSource source;
	GPollFD poll;
};

static gboolean watch_prepare( GSource *source, gint *timeout )
{
//	debug_log("glib: watch_prepare");

	*timeout = -1;
	return FALSE;
}

static gboolean watch_check( GSource *source )
{
//	debug_log("glib: watch_check");

	BusSource* io = (BusSource*)source;
	return io->poll.revents ? TRUE : FALSE;
}

static gboolean watch_dispatch( GSource *source, GSourceFunc callback, gpointer data )
{
	debug_log("glib: watch_dispatch");

	gboolean cb = callback(data);
	DBus::default_dispatcher->dispatch_pending(); //TODO: won't work in case of multiple dispatchers
	return cb;
}

static GSourceFuncs watch_funcs = {
	watch_prepare,
	watch_check,
	watch_dispatch,
	NULL
};

Glib::BusWatch::BusWatch( Watch::Internal* wi, GMainContext* ctx )
: Watch(wi), _ctx(ctx)
{
	_enable();
}

Glib::BusWatch::~BusWatch()
{
	_disable();
}

void Glib::BusWatch::toggle()
{
	debug_log("glib: watch %p toggled (%s)", this, Watch::enabled() ? "on":"off");

	if(Watch::enabled())	_enable();
	else			_disable();
}

gboolean Glib::BusWatch::watch_handler( gpointer data )
{
	Glib::BusWatch* w = reinterpret_cast<Glib::BusWatch*>(data);

	BusSource* io = (BusSource*)(w->_source);

	int flags = 0;
	if(io->poll.revents & G_IO_IN)
	     flags |= DBUS_WATCH_READABLE;
	if(io->poll.revents & G_IO_OUT)
	     flags |= DBUS_WATCH_WRITABLE;
	if(io->poll.revents & G_IO_ERR)
	     flags |= DBUS_WATCH_ERROR;
	if(io->poll.revents & G_IO_HUP)
	     flags |= DBUS_WATCH_HANGUP;

	w->handle(flags);

	return TRUE;
}

void Glib::BusWatch::_enable()
{
	_source = g_source_new(&watch_funcs, sizeof(BusSource));
	g_source_set_callback(_source, watch_handler, this, NULL);

	int flags = Watch::flags();
	int condition = 0;

	if(flags & DBUS_WATCH_READABLE)
		condition |= G_IO_IN;
//	if(flags & DBUS_WATCH_WRITABLE)
//		condition |= G_IO_OUT;
	if(flags & DBUS_WATCH_ERROR)
		condition |= G_IO_ERR;
	if(flags & DBUS_WATCH_HANGUP)
		condition |= G_IO_HUP;

	GPollFD* poll = &(((BusSource*)_source)->poll);
	poll->fd = Watch::descriptor();
	poll->events = condition;
	poll->revents = 0;

	g_source_add_poll(_source, poll);
	g_source_attach(_source, _ctx);
}

void Glib::BusWatch::_disable()
{
	GPollFD* poll = &(((BusSource*)_source)->poll);
	g_source_remove_poll(_source, poll);
	g_source_destroy(_source);
}

void Glib::BusDispatcher::attach( GMainContext* ctx )
{
	_ctx = ctx ? ctx : g_main_context_default();
}

Timeout* Glib::BusDispatcher::add_timeout( Timeout::Internal* wi )
{
	Timeout* t = new Glib::BusTimeout(wi, _ctx);

	debug_log("glib: added timeout %p (%s)", t, t->enabled() ? "on":"off");

	return t;
}

void Glib::BusDispatcher::rem_timeout( Timeout* t )
{
	debug_log("glib: removed timeout %p", t);

	delete t;
}

Watch* Glib::BusDispatcher::add_watch( Watch::Internal* wi )
{
	Watch* w = new Glib::BusWatch(wi, _ctx);

	debug_log("glib: added watch %p (%s) fd=%d flags=%d",
		w, w->enabled() ? "on":"off", w->descriptor(), w->flags()
	);
	return w;
}

void Glib::BusDispatcher::rem_watch( Watch* w )
{
	debug_log("glib: removed watch %p", w);

	delete w;
}
