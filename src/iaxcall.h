/*
 *  Copyright (C) 2006 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
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
#ifndef IAXCALL_H
#define IAXCALL_H

#include "call.h"
#include <iax/iax-client.h>

/**
 * IAXCall are IAX implementation of a normal Call 
 * @author Yan Morin <yan.morin@gmail.com>
 */
class IAXCall : public Call
{
public:
    IAXCall(const CallID& id, Call::CallType type);

    ~IAXCall();

    /** Get the session pointer or 0 */
    struct iax_session* getSession() { return _session; }

    /** Set the session pointer 
     * @param session the session pointer to assign
     */
    void setSession(struct iax_session* session) { _session = session; }

    void setFormat(int format) { _format = format; }
    int getFormat() { return _format; }
    
private:
    // each call is associate to a session
    struct iax_session* _session;
    int _format;
};

#endif
