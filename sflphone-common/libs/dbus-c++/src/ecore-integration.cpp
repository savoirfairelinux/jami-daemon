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

#include <dbus-c++/ecore-integration.h>

#include <dbus/dbus.h> // for DBUS_WATCH_*

using namespace DBus;

Ecore::BusTimeout::BusTimeout (Timeout::Internal* ti)
        : Timeout (ti) {
    _enable();
}

Ecore::BusTimeout::~BusTimeout() {
    _disable();
}

void Ecore::BusTimeout::toggle() {
    debug_log ("ecore: timeout %p toggled (%s)", this, Timeout::enabled() ? "on":"off");

    if (Timeout::enabled())	_enable();
    else			_disable();
}

int Ecore::BusTimeout::timeout_handler (void *data) {
    Ecore::BusTimeout* t = reinterpret_cast<Ecore::BusTimeout*> (data);

    debug_log ("Ecore::BusTimeout::timeout_handler( void *data )");

    t->handle();

    return 1; // 1 -> reshedule in ecore for next timer interval
}

void Ecore::BusTimeout::_enable() {
    debug_log ("Ecore::BusTimeout::_enable()");

    _etimer = ecore_timer_add ( ( (double) Timeout::interval()) /1000, timeout_handler, this);
}

void Ecore::BusTimeout::_disable() {
    debug_log ("Ecore::BusTimeout::_disable()");

    ecore_timer_del (_etimer);
}

static bool watch_prepare (int *timeout) {
    debug_log ("ecore: watch_prepare");

    *timeout = -1;
    return false;
}

static bool watch_check() {
    debug_log ("ecore: watch_check");

    return true;
}

static bool watch_dispatch (void *data) {
    debug_log ("ecore: watch_dispatch");

    bool cb = true;
    DBus::default_dispatcher->dispatch_pending(); //TODO: won't work in case of multiple dispatchers
    return cb;
}

Ecore::BusWatch::BusWatch (Watch::Internal* wi)
        : Watch (wi) {
    _enable();
}

Ecore::BusWatch::~BusWatch() {
    _disable();
}

void Ecore::BusWatch::toggle() {
    debug_log ("ecore: watch %p toggled (%s)", this, Watch::enabled() ? "on":"off");

    if (Watch::enabled())	_enable();
    else			_disable();
}

int Ecore::BusWatch::watch_handler_read (void *data, Ecore_Fd_Handler *fdh) {
    Ecore::BusWatch* w = reinterpret_cast<Ecore::BusWatch*> (data);

    debug_log ("ecore: watch_handler_read");

    int flags = DBUS_WATCH_READABLE;

    watch_dispatch (NULL);

    w->handle (flags);

    return 1;
}

int Ecore::BusWatch::watch_handler_error (void *data, Ecore_Fd_Handler *fdh) {
    Ecore::BusWatch* w = reinterpret_cast<Ecore::BusWatch*> (data);

    debug_log ("ecore: watch_handler_error");

    int flags = DBUS_WATCH_ERROR;

    watch_dispatch (NULL);

    return 1;
}

void Ecore::BusWatch::_enable() {
    debug_log ("Ecore::BusWatch::_enable()");

    int flags = Watch::flags();

    fd_handler_read = ecore_main_fd_handler_add (Watch::descriptor(),
                      ECORE_FD_READ,
                      watch_handler_read,
                      this,
                      NULL, NULL);

    ecore_main_fd_handler_active_set (fd_handler_read, ECORE_FD_READ);

    fd_handler_error = ecore_main_fd_handler_add (Watch::descriptor(),
                       ECORE_FD_ERROR,
                       watch_handler_error,
                       this,
                       NULL, NULL);

    ecore_main_fd_handler_active_set (fd_handler_error, ECORE_FD_ERROR);
}

void Ecore::BusWatch::_disable() {
    ecore_main_fd_handler_del (fd_handler_read);
    ecore_main_fd_handler_del (fd_handler_error);
}

void Ecore::BusDispatcher::attach() {
}

Timeout* Ecore::BusDispatcher::add_timeout (Timeout::Internal* wi) {
    Timeout* t = new Ecore::BusTimeout (wi);

    debug_log ("ecore: added timeout %p (%s)", t, t->enabled() ? "on":"off");

    return t;
}

void Ecore::BusDispatcher::rem_timeout (Timeout* t) {
    debug_log ("ecore: removed timeout %p", t);

    delete t;
}

Watch* Ecore::BusDispatcher::add_watch (Watch::Internal* wi) {
    Watch* w = new Ecore::BusWatch (wi);

    debug_log ("ecore: added watch %p (%s) fd=%d flags=%d",
               w, w->enabled() ? "on":"off", w->descriptor(), w->flags()
              );
    return w;
}

void Ecore::BusDispatcher::rem_watch (Watch* w) {
    debug_log ("ecore: removed watch %p", w);

    delete w;
}
