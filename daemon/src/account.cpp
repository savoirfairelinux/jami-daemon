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
#ifdef SFL_VIDEO
#include "video/video_endpoint.h"
#endif

Account::Account(const std::string& accountID, const std::string &type) :
    accountID_(accountID)
    , username_()
    , hostname_()
    , alias_()
    , link_(NULL)
    , enabled_(true)
    , type_(type)
    , registrationState_(Unregistered)
    , codecList_()
#ifdef SFL_VIDEO
    , videoCodecList_()
#endif
    , codecStr_()
    , ringtonePath_("/usr/share/sflphone/ringtones/konga.ul")
    , ringtoneEnabled_(true)
    , displayName_("")
    , userAgent_("SFLphone")
    , mailBox_()
{
    // Initialize the codec list, used when creating a new account
    loadDefaultCodecs();
}

Account::~Account()
{}

void Account::setRegistrationState(const RegistrationState &state)
{
    if (state != registrationState_) {
        registrationState_ = state;

        // Notify the client
        Manager::instance().connectionStatusNotification();
    }
}

void Account::loadDefaultCodecs()
{
    // Initialize codec
    std::vector<std::string> codecList;
    codecList.push_back("0");
    codecList.push_back("3");
    codecList.push_back("8");
    codecList.push_back("9");
    codecList.push_back("110");
    codecList.push_back("111");
    codecList.push_back("112");

    setActiveCodecs(codecList);
#if SFL_VIDEO
    setActiveVideoCodecs(sfl_video::getVideoCodecList());
#endif
}

#if SFL_VIDEO
void Account::setActiveVideoCodecs (const std::vector <std::string> &list)
{
	videoCodecList_ = !list.empty() ? list : sfl_video::getVideoCodecList();
}
#endif

void Account::setActiveCodecs(const std::vector <std::string> &list)
{
    // first clear the previously stored codecs
    codecList_.clear();

    // list contains the ordered payload of active codecs picked by the user for this account
    // we used the CodecList vector to save the order.
    for (std::vector<std::string>::const_iterator iter = list.begin(); iter != list.end();
            ++iter) {
        int payload = std::atoi(iter->c_str());
        codecList_.push_back(static_cast<int>(payload));
    }

    // update the codec string according to new codec selection
    codecStr_ = ManagerImpl::serialize(list);
}

std::string Account::mapStateNumberToString(RegistrationState state)
{
    static const char * mapStateToChar[] = {
        "UNREGISTERED",
        "TRYING",
        "REGISTERED",
        "ERROR",
        "ERRORAUTH",
        "ERRORNETWORK",
        "ERRORHOST",
        "ERROREXISTSTUN",
        "ERRORCONFSTUN"
    };

    if (state > NumberOfStates)
        return "ERROR";

    return mapStateToChar[state];
}
