/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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

#include "preferences.h"
#include <sstream>
#include "global.h"
#include <cassert>
        
const char * const Preferences::DFT_ZONE = "North America";

namespace {
    static const char * const DFT_PULSE_LENGTH_STR ="250";  /** Default DTMF lenght */
    static const char * const ZRTP_ZIDFILE = "zidFile";     /** The filename used for storing ZIDs */
    static const char * const ALSA_DFT_CARD	= "0";          /** Default sound card index */
    static const char * const DFT_FRAME_SIZE = "20";        /** Default frame size in millisecond */
    static const char * const DFT_VOL_SPKR_STR = "100";     /** Default speaker volume */
    static const char * const DFT_VOL_MICRO_STR	= "100";    /** Default mic volume */
} // end anonymous namespace

Preferences::Preferences() :  _accountOrder ("")
    , _audioApi (1) // 1 is pulseaudio, 0 alsa
    , _historyLimit (30)
    , _historyMaxCalls (20)
    , _notifyMails (false)
    , _zoneToneChoice (DFT_ZONE) // DFT_ZONE
    , _registrationExpire (180)
    , _portNum (5060)
    , _searchBarDisplay (true)
    , _zeroConfenable (false)
    , _md5Hash (false)
{

}

Preferences::~Preferences() {}


void Preferences::serialize (Conf::YamlEmitter *emiter)
{
	Conf::MappingNode preferencemap (NULL);

    Conf::ScalarNode order (_accountOrder);
    std::string audioapistr = (_audioApi == 1) ? "pulseaudio" : "alsa";
    Conf::ScalarNode audioapi (audioapistr);
    std::stringstream histlimitstr;
    histlimitstr << _historyLimit;
    Conf::ScalarNode historyLimit (histlimitstr.str());
    std::stringstream histmaxstr;
    histmaxstr << _historyMaxCalls;
    Conf::ScalarNode historyMaxCalls (histmaxstr.str());
    Conf::ScalarNode notifyMails (_notifyMails);
    Conf::ScalarNode zoneToneChoice (_zoneToneChoice);
    std::stringstream expirestr;
    expirestr << _registrationExpire;
    Conf::ScalarNode registrationExpire (expirestr.str());
    std::stringstream portstr;
    portstr << _portNum;
    Conf::ScalarNode portNum (portstr.str());
    Conf::ScalarNode searchBarDisplay (_searchBarDisplay);
    Conf::ScalarNode zeroConfenable (_zeroConfenable);
    Conf::ScalarNode md5Hash (_md5Hash);

    preferencemap.setKeyValue (orderKey, &order);
    preferencemap.setKeyValue (audioApiKey, &audioapi);
    preferencemap.setKeyValue (historyLimitKey, &historyLimit);
    preferencemap.setKeyValue (historyMaxCallsKey, &historyMaxCalls);
    preferencemap.setKeyValue (notifyMailsKey, &notifyMails);
    preferencemap.setKeyValue (zoneToneChoiceKey, &zoneToneChoice);
    preferencemap.setKeyValue (registrationExpireKey, &registrationExpire);
    preferencemap.setKeyValue (portNumKey, &portNum);
    preferencemap.setKeyValue (searchBarDisplayKey, &searchBarDisplay);
    preferencemap.setKeyValue (zeroConfenableKey, &zeroConfenable);
    preferencemap.setKeyValue (md5HashKey, &md5Hash);

    emiter->serializePreference (&preferencemap);
}

void Preferences::unserialize (Conf::MappingNode *map)
{
    if (map == NULL) {
        _error ("Preference: Error: Preference map is NULL");
        return;
    }

    map->getValue (orderKey, &_accountOrder);
    std::string audioApi;
    map->getValue (audioApiKey, &audioApi);
    // 1 is pulseaudio, 0 is alsa
    _audioApi = (audioApi == "pulseaudio") ? 1 : 0;
    map->getValue (historyLimitKey, &_historyLimit);
    map->getValue (historyMaxCallsKey, &_historyMaxCalls);
    map->getValue (notifyMailsKey, &_notifyMails);
    map->getValue (zoneToneChoiceKey, &_zoneToneChoice);
    map->getValue (registrationExpireKey, &_registrationExpire);
    map->getValue (portNumKey, &_portNum);
    map->getValue (searchBarDisplayKey, &_searchBarDisplay);
    map->getValue (zeroConfenableKey, &_zeroConfenable);
    map->getValue (md5HashKey, &_md5Hash);
}


VoipPreference::VoipPreference() :  _playDtmf (true)
    , _playTones (true)
    , _pulseLength (atoi (DFT_PULSE_LENGTH_STR)) // DFT_PULSE_LENGTH_STR
    , _symmetricRtp (true)
    , _zidFile (ZRTP_ZIDFILE) // ZRTP_ZID_FILENAME
{

}

VoipPreference::~VoipPreference() {}


void VoipPreference::serialize (Conf::YamlEmitter *emitter)
{
    Conf::MappingNode preferencemap (NULL);

    Conf::ScalarNode playDtmf (_playDtmf);
    Conf::ScalarNode playTones (_playTones);
    std::stringstream pulselengthstr;
    pulselengthstr << _pulseLength;
    Conf::ScalarNode pulseLength (pulselengthstr.str());
    Conf::ScalarNode symmetricRtp (_symmetricRtp);
    Conf::ScalarNode zidFile (_zidFile.c_str());

    preferencemap.setKeyValue (playDtmfKey, &playDtmf);
    preferencemap.setKeyValue (playTonesKey, &playTones);
    preferencemap.setKeyValue (pulseLengthKey, &pulseLength);
    preferencemap.setKeyValue (symmetricRtpKey, &symmetricRtp);
    preferencemap.setKeyValue (zidFileKey, &zidFile);

    emitter->serializeVoipPreference (&preferencemap);
}

void VoipPreference::unserialize (Conf::MappingNode *map)
{
    if (!map) {
        _error ("VoipPreference: Error: Preference map is NULL");
        return;
    }

    map->getValue (playDtmfKey, &_playDtmf);
    map->getValue (playTonesKey, &_playTones);
    map->getValue (pulseLengthKey, &_pulseLength);
    map->getValue (symmetricRtpKey, &_symmetricRtp);
    map->getValue (zidFileKey, &_zidFile);
}



AddressbookPreference::AddressbookPreference() : _photo (true)
    , _enabled (true)
    , _list ("")
    , _maxResults (25)
    , _business (true)
    , _home (true)
    , _mobile (true)
{

}

AddressbookPreference::~AddressbookPreference() {}

void AddressbookPreference::serialize (Conf::YamlEmitter *emitter)
{
    Conf::MappingNode preferencemap (NULL);

    Conf::ScalarNode photo (_photo);
    Conf::ScalarNode enabled (_enabled);
    Conf::ScalarNode list (_list);
    std::stringstream maxresultstr;
    maxresultstr << _maxResults;
    Conf::ScalarNode maxResults (maxresultstr.str());
    Conf::ScalarNode business (_business);
    Conf::ScalarNode home (_home);
    Conf::ScalarNode mobile (_mobile);

    preferencemap.setKeyValue (photoKey, &photo);
    preferencemap.setKeyValue (enabledKey, &enabled);
    preferencemap.setKeyValue (listKey, &list);
    preferencemap.setKeyValue (maxResultsKey, &maxResults);
    preferencemap.setKeyValue (businessKey, &business);
    preferencemap.setKeyValue (homeKey, &home);
    preferencemap.setKeyValue (mobileKey, &mobile);

    emitter->serializeAddressbookPreference (&preferencemap);

}

void AddressbookPreference::unserialize (Conf::MappingNode *map)
{
    if (!map) {
        _error ("Addressbook: Error: Preference map is NULL");
        return;
    }

    map->getValue (photoKey, &_photo);
    map->getValue (enabledKey, &_enabled);
    map->getValue (listKey, &_list);
    map->getValue (maxResultsKey, &_maxResults);
    map->getValue (businessKey, &_business);
    map->getValue (homeKey, &_home);
    map->getValue (mobileKey, &_mobile);
}


HookPreference::HookPreference() : _iax2Enabled (false)
    , _numberAddPrefix ("")
    , _numberEnabled (false)
    , _sipEnabled (false)
    , _urlCommand ("x-www-browser")
    , _urlSipField ("X-sflphone-url")
{

}

void HookPreference::serialize (Conf::YamlEmitter *emitter)
{
    Conf::MappingNode preferencemap (NULL);

    Conf::ScalarNode iax2Enabled (_iax2Enabled);
    Conf::ScalarNode numberAddPrefix (_numberAddPrefix);
    Conf::ScalarNode numberEnabled (_numberEnabled);
    Conf::ScalarNode sipEnabled (_sipEnabled);
    Conf::ScalarNode urlCommand (_urlCommand);
    Conf::ScalarNode urlSipField (_urlSipField);

    preferencemap.setKeyValue (iax2EnabledKey, &iax2Enabled);
    preferencemap.setKeyValue (numberAddPrefixKey, &numberAddPrefix);
    preferencemap.setKeyValue (numberEnabledKey, &numberEnabled);
    preferencemap.setKeyValue (sipEnabledKey, &sipEnabled);
    preferencemap.setKeyValue (urlCommandKey, &urlCommand);
    preferencemap.setKeyValue (urlSipFieldKey, &urlSipField);

    emitter->serializeHooksPreference (&preferencemap);
}

void HookPreference::unserialize (Conf::MappingNode *map)
{
    if (!map) {
        _error ("Hook: Error: Preference map is NULL");
        return;
    }

    map->getValue (iax2EnabledKey, &_iax2Enabled);
    map->getValue (numberAddPrefixKey, &_numberAddPrefix);
    map->getValue (numberEnabledKey, &_numberEnabled);
    map->getValue (sipEnabledKey, &_sipEnabled);
    map->getValue (urlCommandKey, &_urlCommand);
    map->getValue (urlSipFieldKey, &_urlSipField);
}



AudioPreference::AudioPreference() : _cardin (atoi (ALSA_DFT_CARD)) // ALSA_DFT_CARD
    , _cardout (atoi (ALSA_DFT_CARD)) // ALSA_DFT_CARD
    , _cardring (atoi (ALSA_DFT_CARD)) // ALSA_DFT_CARD
    , _framesize (atoi (DFT_FRAME_SIZE)) // DFT_FRAME_SIZE
    , _plugin ("default") // PCM_DEFAULT
    , _smplrate (44100) // DFT_SAMPLE_RATE
    , _devicePlayback ("")
    , _deviceRecord ("")
    , _deviceRingtone ("")
    , _recordpath ("") // DFT_RECORD_PATH
    , _alwaysRecording(false)
    , _volumemic (atoi (DFT_VOL_SPKR_STR)) // DFT_VOL_SPKR_STR
    , _volumespkr (atoi (DFT_VOL_MICRO_STR)) // DFT_VOL_MICRO_STR
    , _noisereduce (true)
    , _echocancel(false)
    , _echoCancelTailLength(100)
    , _echoCancelDelay(0)
{

}

AudioPreference::~AudioPreference() {}

void AudioPreference::serialize (Conf::YamlEmitter *emitter)
{
    Conf::MappingNode preferencemap (NULL);
    Conf::MappingNode alsapreferencemap (NULL);
    Conf::MappingNode pulsepreferencemap (NULL);

    // alsa preference
    std::stringstream instr;
    instr << _cardin;
    Conf::ScalarNode cardin (instr.str()); // 0
    std::stringstream outstr;
    outstr << _cardout;
    Conf::ScalarNode cardout (outstr.str()); // 0
    std::stringstream ringstr;
    ringstr << _cardring;
    Conf::ScalarNode cardring (ringstr.str());// 0
    std::stringstream framestr;
    framestr << _framesize;
    Conf::ScalarNode framesize (framestr.str()); // 20
    Conf::ScalarNode plugin (_plugin); // default

    std::stringstream ratestr;
    ratestr << _smplrate;
    Conf::ScalarNode smplrate (ratestr.str());// 44100

    //pulseaudio preference
    Conf::ScalarNode devicePlayback (_devicePlayback);//:
    Conf::ScalarNode deviceRecord (_deviceRecord); //:
    Conf::ScalarNode deviceRingtone (_deviceRingtone); //:

    // general preference
    Conf::ScalarNode recordpath (_recordpath); //: /home/msavard/Bureau
    Conf::ScalarNode alwaysRecording(_alwaysRecording);
    std::stringstream micstr;
    micstr << _volumemic;
    Conf::ScalarNode volumemic (micstr.str()); //:  100
    std::stringstream spkrstr;
    spkrstr << _volumespkr;
    Conf::ScalarNode volumespkr (spkrstr.str()); //: 100
    Conf::ScalarNode noise (_noisereduce);
    Conf::ScalarNode echo(_echocancel);
    std::stringstream tailstr;
    _debug("************************************************** serialize echotail %d", _echoCancelTailLength);
    tailstr << _echoCancelTailLength;
    Conf::ScalarNode echotail(tailstr.str());
    std::stringstream delaystr;
    _debug("************************************************** serialize echodelay %d", _echoCancelTailLength);
    delaystr << _echoCancelDelay;
    Conf::ScalarNode echodelay(delaystr.str());

    preferencemap.setKeyValue (recordpathKey, &recordpath);
    preferencemap.setKeyValue (alwaysRecordingKey, &alwaysRecording);
    preferencemap.setKeyValue (volumemicKey, &volumemic);
    preferencemap.setKeyValue (volumespkrKey, &volumespkr);

    preferencemap.setKeyValue (alsamapKey, &alsapreferencemap);
    alsapreferencemap.setKeyValue (cardinKey, &cardin);
    alsapreferencemap.setKeyValue (cardoutKey, &cardout);
    alsapreferencemap.setKeyValue (cardringKey, &cardring);
    alsapreferencemap.setKeyValue (framesizeKey, &framesize);
    alsapreferencemap.setKeyValue (pluginKey, &plugin);
    alsapreferencemap.setKeyValue (smplrateKey, &smplrate);

    preferencemap.setKeyValue (pulsemapKey, &pulsepreferencemap);
    pulsepreferencemap.setKeyValue (devicePlaybackKey, &devicePlayback);
    pulsepreferencemap.setKeyValue (deviceRecordKey, &deviceRecord);
    pulsepreferencemap.setKeyValue (deviceRingtoneKey, &deviceRingtone);

    preferencemap.setKeyValue (noiseReduceKey, &noise);
    preferencemap.setKeyValue(echoCancelKey, &echo);
    preferencemap.setKeyValue(echoTailKey, &echotail);
    preferencemap.setKeyValue(echoDelayKey, &echodelay);

    emitter->serializeAudioPreference (&preferencemap);

}

void AudioPreference::unserialize (Conf::MappingNode *map)
{
	assert(map);

    map->getValue (recordpathKey, &_recordpath);
    map->getValue (alwaysRecordingKey, &_alwaysRecording);
    map->getValue (volumemicKey, &_volumemic);
    map->getValue (volumespkrKey, &_volumespkr);
    map->getValue (noiseReduceKey, &_noisereduce);
    map->getValue(echoCancelKey, &_echocancel);

    Conf::MappingNode *alsamap = (Conf::MappingNode *) (map->getValue ("alsa"));
    if (alsamap) {
    	alsamap->getValue (cardinKey, &_cardin);
		alsamap->getValue (cardoutKey, &_cardout);
		alsamap->getValue (cardringKey, &_cardring);
		alsamap->getValue (framesizeKey, &_framesize);
		alsamap->getValue (smplrateKey, &_smplrate);
		alsamap->getValue (pluginKey, &_plugin);
    }

    Conf::MappingNode *pulsemap = (Conf::MappingNode *) (map->getValue ("pulse"));
    if (pulsemap) {
    	pulsemap->getValue (devicePlaybackKey, &_devicePlayback);
    	pulsemap->getValue (deviceRecordKey, &_deviceRecord);
    	pulsemap->getValue (deviceRingtoneKey, &_deviceRingtone);
    }
}



ShortcutPreferences::ShortcutPreferences() : _hangup ("")
    , _pickup ("")
    , _popup ("")
    , _toggleHold ("")
    , _togglePickupHangup ("")
{

}

ShortcutPreferences::~ShortcutPreferences() {}


std::map<std::string, std::string> ShortcutPreferences::getShortcuts() const
{
    std::map<std::string, std::string> shortcutsMap;

    shortcutsMap[hangupShortKey] = _hangup;
    shortcutsMap[pickupShortKey] = _pickup;
    shortcutsMap[popupShortKey] = _popup;
    shortcutsMap[toggleHoldShortKey] = _toggleHold;
    shortcutsMap[togglePickupHangupShortKey] = _togglePickupHangup;

    return shortcutsMap;
}


void ShortcutPreferences::setShortcuts (std::map<std::string, std::string> map)
{
    _hangup = map[hangupShortKey];
    _pickup = map[pickupShortKey];
    _popup = map[popupShortKey];
    _toggleHold = map[toggleHoldShortKey];
    _togglePickupHangup = map[togglePickupHangupShortKey];
}


void ShortcutPreferences::serialize (Conf::YamlEmitter *emitter)
{
    Conf::MappingNode preferencemap (NULL);

    Conf::ScalarNode hangup (_hangup);
    Conf::ScalarNode pickup (_pickup);
    Conf::ScalarNode popup (_popup);
    Conf::ScalarNode toggleHold (_toggleHold);
    Conf::ScalarNode togglePickupHangup (_togglePickupHangup);

    preferencemap.setKeyValue (hangupShortKey, &hangup);
    preferencemap.setKeyValue (pickupShortKey, &pickup);
    preferencemap.setKeyValue (popupShortKey, &popup);
    preferencemap.setKeyValue (toggleHoldShortKey, &toggleHold);
    preferencemap.setKeyValue (togglePickupHangupShortKey, &togglePickupHangup);

    emitter->serializeShortcutPreference (&preferencemap);
}

void ShortcutPreferences::unserialize (Conf::MappingNode *map)
{
    if (map == NULL) {
        _error ("ShortcutPreference: Error: Preference map is NULL");
        return;
    }
    map->getValue (hangupShortKey, &_hangup);
    map->getValue (pickupShortKey, &_pickup);
    map->getValue (popupShortKey, &_popup);
    map->getValue (toggleHoldShortKey, &_toggleHold);
    map->getValue (togglePickupHangupShortKey, &_togglePickupHangup);
}
