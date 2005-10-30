/**
 *  Copyright (C) 2005 Savoir-Faire Linux inc.
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
#ifndef SESSIONIO_H
#define SESSIONIO_H

#include <string>

/**
Session IO Interface to send and receive requests
Could be over network or locally
@author Yan Morin
*/
class SessionIO {
public:
    SessionIO();
    virtual ~SessionIO();

    virtual void send(const std::string& response) = 0;
    virtual void sendLast() = 0;
    virtual bool receive(std::string& request) = 0;
    virtual bool good() = 0;
    virtual void init() = 0;
};

#endif
