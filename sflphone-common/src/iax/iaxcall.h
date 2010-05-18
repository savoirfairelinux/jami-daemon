/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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
#ifndef IAXCALL_H
#define IAXCALL_H

#include "call.h"
#include "audio/codecs/codecDescriptor.h"

#include <iax-client.h>
#include <frame.h>

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
    int getSupportedFormat (std::string accountID);

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
    int getFirstMatchingFormat(int needles, std::string accountID);

    // AUDIO
    /** 
     * Set internal codec Map: initialization only, not protected 
     * @param map The codec map
     */
    void setCodecMap(const CodecDescriptor& map) { _codecMap = map; } 

    /** 
     * Get internal codec Map: initialization only, not protected 
     * @return CodecDescriptor	The codec map
     */
    CodecDescriptor& getCodecMap();

    /** 
     * Return audio codec [mutex protected]
     * @return AudioCodecType The payload of the codec
     */
    AudioCodecType getAudioCodec();

private:
    /** Each call is associated with an iax_session */
    struct iax_session* _session;

    /** 
     * Set the audio codec used.  [not protected] 
     * @param audioCodec  The payload of the codec
     */
    void setAudioCodec(AudioCodecType audioCodec) { _audioCodec = audioCodec; }

    /** Codec Map */
    CodecDescriptor _codecMap;

    /** Codec pointer */
    AudioCodecType _audioCodec;

    /**
     * Format currently in use in the conversation,
     * sent in each outgoing voice packet.
     */
    int _format;
};

#endif
