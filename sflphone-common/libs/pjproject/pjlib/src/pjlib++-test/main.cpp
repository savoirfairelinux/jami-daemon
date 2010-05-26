/* $Id */
/* 
 * Copyright (C) 2008-2009 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Teluu Inc. (http://www.teluu.com)
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */
#include <pj++/file.hpp>
#include <pj++/list.hpp>
#include <pj++/lock.hpp>
#include <pj++/hash.hpp>
#include <pj++/os.hpp>
#include <pj++/proactor.hpp>
#include <pj++/sock.hpp>
#include <pj++/string.hpp>
#include <pj++/timer.hpp>
#include <pj++/tree.hpp>

class My_Async_Op : public Pj_Async_Op
{
};

class My_Event_Handler : public Pj_Event_Handler
{
};

int main()
{
    Pjlib lib;
    Pj_Caching_Pool mem;
    Pj_Pool the_pool;
    Pj_Pool *pool = &the_pool;
    
    the_pool.attach(mem.create_pool(4000,4000));

    Pj_Semaphore_Lock lsem(pool);
    Pj_Semaphore_Lock *plsem;

    plsem = new(pool) Pj_Semaphore_Lock(pool);
    delete plsem;

    Pj_Proactor proactor(pool, 100, 100);

    My_Event_Handler *event_handler = new(the_pool) My_Event_Handler;
    proactor.register_socket_handler(pool, event_handler);
    proactor.unregister_handler(event_handler);

    return 0;
}

