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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "account.h"
#include <algorithm>
#ifdef SFL_VIDEO
#include "video/libav_utils.h"
#endif

#include "manager.h"
#include "dbus/configurationmanager.h"

const char * const Account::AUDIO_CODECS_KEY =      "audioCodecs";  // 0/9/110/111/112/
const char * const Account::VIDEO_CODECS_KEY =      "videoCodecs";
const char * const Account::VIDEO_CODEC_ENABLED =   "enabled";
const char * const Account::VIDEO_CODEC_NAME =      "name";
const char * const Account::VIDEO_CODEC_BITRATE =   "bitrate";
const char * const Account::RINGTONE_PATH_KEY =     "ringtonePath";
const char * const Account::RINGTONE_ENABLED_KEY =  "ringtoneEnabled";
const char * const Account::DISPLAY_NAME_KEY =      "displayName";
const char * const Account::ALIAS_KEY =             "alias";
const char * const Account::TYPE_KEY =              "type";
const char * const Account::ID_KEY =                "id";
const char * const Account::USERNAME_KEY =          "username";
const char * const Account::AUTHENTICATION_USERNAME_KEY = "authenticationUsername";
const char * const Account::PASSWORD_KEY =          "password";
const char * const Account::HOSTNAME_KEY =          "hostname";
const char * const Account::ACCOUNT_ENABLE_KEY =    "enable";
const char * const Account::MAILBOX_KEY =           "mailbox";

using std::map;
using std::string;
using std::vector;


Account::Account(const string &accountID, const string &type) :
    accountID_(accountID)
    , username_()
    , hostname_()
    , alias_()
    , enabled_(true)
    , type_(type)
    , registrationState_(UNREGISTERED)
    , audioCodecList_()
    , videoCodecList_()
    , audioCodecStr_()
    , ringtonePath_("/usr/share/sflphone/ringtones/konga.ul")
    , ringtoneEnabled_(true)
    , displayName_("")
    , userAgent_("SFLphone")
    , mailBox_()
{
    // Initialize the codec order, used when creating a new account
    loadDefaultCodecs();
}

Account::~Account()
{}

void Account::setRegistrationState(const RegistrationState &state)
{
    if (state != registrationState_) {
        registrationState_ = state;

        // Notify the client
        ConfigurationManager *c(Manager::instance().getDbusManager()->getConfigurationManager());
        c->registrationStateChanged(accountID_, registrationState_);
    }
}

void Account::loadDefaultCodecs()
{
    // TODO
    // CodecMap codecMap = Manager::instance ().getCodecDescriptorMap ().getCodecsMap();

    // Initialize codec
    vector<string> result;
    result.push_back("0");
    result.push_back("3");
    result.push_back("8");
    result.push_back("9");
    result.push_back("110");
    result.push_back("111");
    result.push_back("112");

    setActiveAudioCodecs(result);
#ifdef SFL_VIDEO
    setVideoCodecs(libav_utils::getDefaultCodecs());
#endif
}

void Account::setVideoCodecs(const vector<map<string, string> > &list)
{
#ifdef SFL_VIDEO
    // first clear the previously stored codecs
    videoCodecList_.clear();
    // FIXME: do real validation here
    videoCodecList_ = list;
#else
    (void) list;
#endif
}

void Account::setActiveAudioCodecs(const vector<string> &list)
{
    // first clear the previously stored codecs
    audioCodecList_.clear();

    // list contains the ordered payload of active codecs picked by the user for this account
    // we used the CodecOrder vector to save the order.
    for (vector<string>::const_iterator iter = list.begin(); iter != list.end();
            ++iter) {
        int payload = std::atoi(iter->c_str());
        audioCodecList_.push_back(payload);
    }

    // update the codec string according to new codec selection
    audioCodecStr_ = ManagerImpl::join_string(list);
}

string Account::mapStateNumberToString(RegistrationState state)
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

    if (state > NUMBER_OF_STATES)
        return "ERROR";

    return mapStateToChar[state];
}

vector<map<string, string> >
Account::getAllVideoCodecs() const
{
    return videoCodecList_;
}

namespace {
    bool is_inactive(const map<string, string> &codec)
    {
        map<string, string>::const_iterator iter = codec.find(Account::VIDEO_CODEC_ENABLED);
        return iter == codec.end() or iter->second != "true";
    }
}

vector<map<string, string> >
Account::getActiveVideoCodecs() const
{
    // FIXME: validate video codec details first
    vector<map<string, string> > result(videoCodecList_);
    result.erase(std::remove_if(result.begin(), result.end(), is_inactive), result.end());
    return result;
}
