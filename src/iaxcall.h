/*
 *  Copyright (C) 2006-2007 Savoir-Faire Linux inc.
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
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
 */
#ifndef IAXCALL_H
#define IAXCALL_H

#include "call.h"
#include <iax2/iax-client.h>
#include <iax2/frame.h>

/**
 * @file: iaxcall.h
 * @brief IAXCall are IAX implementation of a normal Call 
 */

class IAXCall : public Call
{
public:
    /**
     * Constructor
     * @param id  The unique ID of the call
     * @param type  The type of the call
     */
    IAXCall(const CallID& id, Call::CallType type);

    /**
     * Destructor
     */
    ~IAXCall();

    /** 
     * @return iax_session* The session pointer or NULL
     */
    struct iax_session* getSession() { return _session; }

    /** 
     * Set the session pointer 
     * @param session the session pointer to assign
     */
    void setSession(struct iax_session* session) { _session = session; }

    /**
     * Set format (one single bit)
     * This function sets the _audioCodec variable with the correct
     * codec.
     * @param format  The format representing the codec
     */
    void setFormat(int format);

    /**
     * Get format for the voice codec used
     * @return int  Bitmask for codecs defined in iax/frame.h
     */
    int getFormat() { return _format; }


    /**
     * @return int  The bitwise list of supported formats
     */
    int getSupportedFormat();

    /**
     * Return a format (int) with the first matching codec selected.
     * 
     * This considers the order of the appearance in the CodecMap,
     * thus, the order of preference.
     *
     * NOTE: Everything returned is bound to the content of the local
     *       CodecMap, so it won't return format values that aren't valid
     *       in this call context.
     *
     * @param needles  The format(s) (bitwise) you are looking for to match
     * @return int  The matching format, thus 0 if none matches
     */
    int getFirstMatchingFormat(int needles);


private:
    /** Each call is associated with an iax_session */
    struct iax_session* _session;

    /**
     * Format currently in use in the conversation,
     * sent in each outgoing voice packet.
     */
    int _format;
};

#endif
