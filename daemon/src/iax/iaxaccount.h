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
#ifndef IAXACCOUNT_H
#define IAXACCOUNT_H

#include "account.h"
#include "iaxvoiplink.h"

/**
 * @file: iaxaccount.h
 * @brief An IAX Account specify IAX specific functions and objects (IAXCall/IAXVoIPLink)
 */
class IAXAccount : public Account {
    public:
        IAXAccount(const std::string& accountID);

        virtual void serialize(Conf::YamlEmitter &emitter);
        virtual void unserialize(const Conf::YamlNode &map);

        std::map<std::string, std::string> getAccountDetails() const;

        // Actually useless, since config loading is done in init()
        void loadConfig();

        // Register an account
        void registerVoIPLink();

        // Unregister an account
        void unregisterVoIPLink(std::function<void(bool)> cb = std::function<void(bool)>());

        std::string getPassword() const {
            return password_;
        }

    private:
        void setAccountDetails(const std::map<std::string, std::string> &details);

         // Account login information: password
        std::string password_;
        IAXVoIPLink link_;
        virtual VoIPLink* getVoIPLink();
        static const char * const ACCOUNT_TYPE;
};

#endif
