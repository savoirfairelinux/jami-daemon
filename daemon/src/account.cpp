/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
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

#include "account.h"
#include "manager.h"

Account::Account (const std::string& accountID, const std::string &type) :
    _accountID (accountID)
    , _link (NULL)
    , _enabled (true)
    , _type (type)
    , _registrationState (Unregistered)
    , _codecOrder ()
    , _codecStr ("")
    , _ringtonePath ("/usr/share/sflphone/ringtones/konga.ul")
    , _ringtoneEnabled (true)
    , _displayName ("")
    , _useragent ("SFLphone")
{
    // Initialize the codec order, used when creating a new account
    loadDefaultCodecs();
}

Account::~Account()
{
}

void Account::setRegistrationState (RegistrationState state)
{
    if (state != _registrationState) {
        _registrationState = state;

        // Notify the client
        Manager::instance().connectionStatusNotification();
    }
}

void Account::loadDefaultCodecs()
{
    // TODO
    // CodecMap codecMap = Manager::instance ().getCodecDescriptorMap ().getCodecsMap();

    // Initialize codec
    std::vector <std::string> codecList;
    codecList.push_back ("0");
    codecList.push_back ("3");
    codecList.push_back ("8");
    codecList.push_back ("9");
    codecList.push_back ("110");
    codecList.push_back ("111");
    codecList.push_back ("112");

    setActiveCodecs (codecList);
}



void Account::setActiveCodecs (const std::vector <std::string> &list)
{
    // first clear the previously stored codecs
    _codecOrder.clear();

    // list contains the ordered payload of active codecs picked by the user for this account
    // we used the CodecOrder vector to save the order.
    size_t i, size = list.size();
    for (i = 0; i < size; i++) {
        int payload = std::atoi (list[i].data());
        _codecOrder.push_back ( (AudioCodecType) payload);
    }

    // update the codec string according to new codec selection
    _codecStr = ManagerImpl::serialize (list);
}
