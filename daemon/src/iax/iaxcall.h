/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
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
#include "noncopyable.h"

/**
 * @file: iaxcall.h
 * @brief IAXCall are IAX implementation of a normal Call
 */
class iax_session;

class IAXCall : public Call {
    public:
        /**
         * Constructor
         * @param id  The unique ID of the call
         * @param type  The type of the call
         */
        IAXCall(const std::string& id, Call::CallType type, const std::string &account_id);

        /**
         * @return int  The bitwise list of supported formats
         */
        int getSupportedFormat(const std::string &accountID) const;

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
        int getFirstMatchingFormat(int needles, const std::string &accountID) const;

        int getAudioCodec() const;

        int format;
        iax_session* session;
    private:
        void answer();

        NON_COPYABLE(IAXCall);
};

#endif
