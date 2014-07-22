/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
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

#ifndef __VOIP_LINK_H__
#define __VOIP_LINK_H__

#include <stdexcept>
#include <functional>
#include <string>
#include <vector>
#include <memory>

class Call;
class Account;

class VoipLinkException : public std::runtime_error {
    public:
        VoipLinkException(const std::string &str = "") :
            std::runtime_error("UserAgent: VoipLinkException occured: " + str) {}
};

/**
 * @file voiplink.h
 * @brief Listener and manager interface for each VoIP protocol
 */
class VoIPLink {
    public:
        VoIPLink() {};
        virtual ~VoIPLink() {};

        /**
         * Virtual method
         * Event listener. Each event send by the call manager is received and handled from here
         */
        virtual bool getEvent() = 0;

        /**
         * Virtual method
         * Returns calls involving this account.
         */
        virtual std::vector<std::shared_ptr<Call> > getCalls(const std::string &account_id) const = 0;

    protected:
        bool handlingEvents_ = false;
};

#endif // __VOIP_LINK_H__
