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

#ifndef __PREFERENCE_H__
#define __PREFERENCE_H__

#include "config/serializable.h"

// general preferences
const std::string orderKey ("order");                         // :	1234/2345/
const std::string audioApiKey ("audioApi");                   // :	0
const std::string historyLimitKey ("historyLimit");           // :	30
const std::string historyMaxCallsKey ("historyMaxCalls");     // :	20
const std::string notifyMailsKey ("notifyMails");             // :	false
const std::string zoneToneChoiceKey ("zoneToneChoice");       // :	North America
const std::string registrationExpireKey ("registrationExpire");// :	180
const std::string portNumKey ("portNum");                     // :	5060
const std::string searchBarDisplayKey ("searchBarDisplay");   // :	true
const std::string zeroConfenableKey ("zeroConfenable");       // :	false
const std::string md5HashKey ("md5Hash");                     // :	false

// voip preferences
const std::string playDtmfKey ("playDtmf"); // true                    true
const std::string playTonesKey ("playTones"); // true
const std::string pulseLengthKey ("pulseLength"); //=250
const std::string symmetricRtpKey ("symmetric");// =true
const std::string zidFileKey ("zidFile");// =sfl.zid

// addressbook preferences
const std::string photoKey ("photo");//		false
const std::string enabledKey ("enabled");//		true
const std::string listKey ("list");//		1243608768.30329.0@emilou-desktop/1243456917.15690.23@emilou-desktop/
const std::string maxResultsKey ("maxResults");//		25
const std::string businessKey ("business");//		true
const std::string homeKey ("home");//		false
const std::string mobileKey ("mobile");//		false

// hooks preferences
const std::string iax2EnabledKey ("iax2Enabled");// :		false
const std::string numberAddPrefixKey ("numberAddPrefix");//:	false
const std::string numberEnabledKey ("numberEnabled"); //:	false
const std::string sipEnabledKey ("sipEnabled"); //:		false
const std::string urlCommandKey ("urlCommand"); //:		x-www-browser
const std::string urlSipFieldKey ("urlSipField"); //:		X-sflphone-url

// audio preferences
const std::string alsamapKey ("alsa");
const std::string pulsemapKey ("pulse");
const std::string cardinKey ("cardIn");// : 0
const std::string cardoutKey ("cardOut");// 0
const std::string cardringKey ("cardRing");// : 0
const std::string pluginKey ("plugin"); //: default
const std::string smplrateKey ("smplRate");//: 44100
const std::string devicePlaybackKey ("devicePlayback");//:
const std::string deviceRecordKey ("deviceRecord");// :
const std::string deviceRingtoneKey ("deviceRingtone");// :
const std::string recordpathKey ("recordPath");//: /home/msavard/Bureau
const std::string alwaysRecordingKey("alwaysRecording");
const std::string volumemicKey ("volumeMic");//:  100
const std::string volumespkrKey ("volumeSpkr");//: 100
const std::string noiseReduceKey ("noiseReduce");
const std::string echoCancelKey ("echoCancel");
const std::string echoTailKey ("echoTailLength");
const std::string echoDelayKey ("echoDelayLength");

// shortcut preferences
const std::string hangupShortKey ("hangUp");
const std::string pickupShortKey ("pickUp");
const std::string popupShortKey ("popupWindow");
const std::string toggleHoldShortKey ("toggleHold");
const std::string togglePickupHangupShortKey ("togglePickupHangup");

class AudioLayer;

class Preferences : public Serializable
{

    public:

        static const char * const DFT_ZONE;

        Preferences();

        AudioLayer *createAudioLayer();
        AudioLayer *switchAndCreateAudioLayer();

        virtual void serialize (Conf::YamlEmitter *emitter);

        virtual void unserialize (Conf::MappingNode *map);


        std::string getAccountOrder (void) const {
            return _accountOrder;
        }
        void setAccountOrder (std::string ord) {
            _accountOrder = ord;
        }

        std::string getAudioApi (void) const {
            return _audioApi;
        }
        void setAudioApi (const std::string &api) {
            _audioApi = api;
        }

        int getHistoryLimit (void) const {
            return _historyLimit;
        }
        void setHistoryLimit (int lim) {
            _historyLimit = lim;
        }

        int getHistoryMaxCalls (void) const {
            return _historyMaxCalls;
        }
        void setHistoryMaxCalls (int max) {
            _historyMaxCalls = max;
        }

        bool getNotifyMails (void) const {
            return _notifyMails;
        }
        void setNotifyMails (bool mails) {
            _notifyMails = mails;
        }

        std::string getZoneToneChoice (void) const {
            return _zoneToneChoice;
        }
        void setZoneToneChoice (std::string str) {
            _zoneToneChoice = str;
        }

        int getRegistrationExpire (void) const {
            return _registrationExpire;
        }
        void setRegistrationExpire (int exp) {
            _registrationExpire = exp;
        }

        int getPortNum (void) const {
            return _portNum;
        }
        void setPortNum (int port) {
            _portNum = port;
        }

        bool getSearchBarDisplay (void) const {
            return _searchBarDisplay;
        }
        void setSearchBarDisplay (bool search) {
            _searchBarDisplay = search;
        }

        bool getZeroConfenable (void) const {
            return _zeroConfenable;
        }
        void setZeroConfenable (bool enable) {
            _zeroConfenable = enable;
        }

        bool getMd5Hash (void) const {
            return _md5Hash;
        }
        void setMd5Hash (bool md5) {
            _md5Hash = md5;
        }

    private:

        // account order
        std::string _accountOrder;

        std::string _audioApi;
        int _historyLimit;
        int _historyMaxCalls;
        bool _notifyMails;
        std::string _zoneToneChoice;
        int _registrationExpire;
        int _portNum;
        bool _searchBarDisplay;
        bool _zeroConfenable;
        bool _md5Hash;

};


class VoipPreference : public Serializable
{

    public:

        VoipPreference();

        ~VoipPreference();

        virtual void serialize (Conf::YamlEmitter *emitter);

        virtual void unserialize (Conf::MappingNode *map);

        bool getPlayDtmf (void) const {
            return _playDtmf;
        }
        void setPlayDtmf (bool dtmf) {
            _playDtmf = dtmf;
        }

        bool getPlayTones (void) const {
            return _playTones;
        }
        void setPlayTones (bool tone) {
            _playTones = tone;
        }

        int getPulseLength (void) const {
            return _pulseLength;
        }
        void setPulseLength (int length) {
            _pulseLength = length;
        }

        bool getSymmetricRtp (void) const {
            return _symmetricRtp;
        }
        void setSymmetricRtp (bool sym) {
            _symmetricRtp = sym;
        }

        std::string getZidFile (void) const {
            return _zidFile;
        }
        void setZidFile (std::string file) {
            _zidFile = file;
        }

    private:

        bool _playDtmf;
        bool _playTones;
        int _pulseLength;
        bool _symmetricRtp;
        std::string _zidFile;

};

class AddressbookPreference : public Serializable
{

    public:

        AddressbookPreference();

        ~AddressbookPreference();

        virtual void serialize (Conf::YamlEmitter *emitter);

        virtual void unserialize (Conf::MappingNode *map);

        bool getPhoto (void) const {
            return _photo;
        }
        void setPhoto (bool p) {
            _photo = p;
        }

        bool getEnabled (void) const {
            return _enabled;
        }
        void setEnabled (bool e) {
            _enabled = e;
        }

        std::string getList (void) const {
            return _list;
        }
        void setList (std::string l) {
            _list = l;
        }

        int getMaxResults (void) const {
            return _maxResults;
        }
        void setMaxResults (int r) {
            _maxResults = r;
        }

        bool getBusiness (void) const {
            return _business;
        }
        void setBusiness (bool b) {
            _business = b;
        }

        bool getHome (void) const {
            return _home;
        }
        void setHone (bool h) {
            _home = h;
        }

        bool getMobile (void) const {
            return _mobile;
        }
        void setMobile (bool m) {
            _mobile = m;
        }

    private:

        bool _photo;
        bool _enabled;
        std::string _list;
        int _maxResults;
        bool _business;
        bool _home;
        bool _mobile;

};


class HookPreference : public Serializable
{

    public:

        HookPreference();

        virtual void serialize (Conf::YamlEmitter *emitter);

        virtual void unserialize (Conf::MappingNode *map);

        bool getIax2Enabled (void) const {
            return _iax2Enabled;
        }
        void setIax2Enabled (bool i) {
            _iax2Enabled = i;
        }

        std::string getNumberAddPrefix (void) const {
            return _numberAddPrefix;
        }
        void setNumberAddPrefix (std::string n) {
            _numberAddPrefix = n;
        }

        bool getNumberEnabled (void) const {
            return _numberEnabled;
        }
        void setNumberEnabled (bool n) {
            _numberEnabled = n;
        }

        bool getSipEnabled (void) const {
            return _sipEnabled;
        }
        void setSipEnabled (bool s) {
            _sipEnabled = s;
        }

        std::string getUrlCommand (void) const {
            return _urlCommand;
        }
        void setUrlCommand (std::string u) {
            _urlCommand = u;
        }

        std::string getUrlSipField (void) const {
            return _urlSipField;
        }
        void setUrlSipField (std::string u) {
            _urlSipField = u;
        }

    private:

        bool _iax2Enabled;// :		false
        std::string _numberAddPrefix;//:	false
        bool _numberEnabled; //:	false
        bool _sipEnabled; //:		false
        std::string _urlCommand; //:		x-www-browser
        std::string _urlSipField; //:		X-sflphone-url

};


class AudioPreference : public Serializable
{

    public:

        AudioPreference();

        ~AudioPreference();

        virtual void serialize (Conf::YamlEmitter *emitter);

        virtual void unserialize (Conf::MappingNode *map);

        // alsa preference
        int getCardin (void) const {
            return _cardin;
        }
        void setCardin (int c) {
            _cardin = c;
        }

        int getCardout (void) const {
            return _cardout;
        }
        void setCardout (int c) {
            _cardout = c;
        }

        int getCardring (void) const {
            return _cardring;
        }
        void setCardring (int c) {
            _cardring = c;
        }

        std::string getPlugin (void) const {
            return _plugin;
        }
        void setPlugin (std::string p) {
            _plugin = p;
        }

        int getSmplrate (void) const {
            return _smplrate;
        }
        void setSmplrate (int r) {
            _smplrate = r;
        }

        //pulseaudio preference
        std::string getDevicePlayback (void) const {
            return _devicePlayback;
        }
        void setDevicePlayback (const std::string &p) {
            _devicePlayback = p;
        }

        std::string getDeviceRecord (void) const {
            return _deviceRecord;
        }
        void setDeviceRecord (const std::string &r) {
            _deviceRecord = r;
        }

        std::string getDeviceRingtone (void) const {
            return _deviceRingtone;
        }
        void setDeviceRingtone (std::string r) {
            _deviceRingtone = r;
        }

        // general preference
        std::string getRecordpath (void) const {
            return _recordpath;
        }
        void setRecordpath (const std::string &r) {
            _recordpath = r;
        }

        bool getIsAlwaysRecording(void) const {
        	return _alwaysRecording;
        }

        void setIsAlwaysRecording(bool rec) {
        	_alwaysRecording = rec;
        }

        int getVolumemic (void) const {
            return _volumemic;
        }
        void setVolumemic (int m) {
            _volumemic = m;
        }

        int getVolumespkr (void) const {
            return _volumespkr;
        }
        void setVolumespkr (int s) {
            _volumespkr = s;
        }

        bool getNoiseReduce (void) const {
            return _noisereduce;
        }

        void setNoiseReduce (bool noise) {
            _noisereduce = noise;
        }

        bool getEchoCancel(void) const {
        	return _echocancel;
        }

        void setEchoCancel(bool echo) {
        	_echocancel = echo;
        }

        int getEchoCancelTailLength(void) const {
        	return _echoCancelTailLength;
        }

        void setEchoCancelTailLength(int length) {
        	_echoCancelTailLength = length;
        }

        int getEchoCancelDelay(void) const {
        	return _echoCancelDelay;
        }

        void setEchoCancelDelay(int delay) {
        	_echoCancelDelay = delay;
        }

    private:

        // alsa preference
        int _cardin; // 0
        int _cardout; // 0
        int _cardring;// 0
        std::string _plugin; // default
        int _smplrate;// 44100

        //pulseaudio preference
        std::string _devicePlayback;//:
        std::string _deviceRecord; //:
        std::string _deviceRingtone; //:

        // general preference
        std::string _recordpath; //: /home/msavard/Bureau
        bool _alwaysRecording;
        int _volumemic; //:  100
        int _volumespkr; //: 100

        bool _noisereduce;
        bool _echocancel;
        int _echoCancelTailLength;
        int _echoCancelDelay;
};


class ShortcutPreferences : public Serializable
{

    public:

        ShortcutPreferences();

        ~ShortcutPreferences();

        virtual void serialize (Conf::YamlEmitter *emitter);

        virtual void unserialize (Conf::MappingNode *map);

        void setShortcuts (std::map<std::string, std::string> shortcuts);
        std::map<std::string, std::string> getShortcuts (void) const;

        std::string getHangup (void) const {
            return _hangup;
        }
        void setHangup (std::string hangup) {
            _hangup = hangup;
        }

        std::string getPickup (void) const {
            return _pickup;
        }
        void setPickup (std::string pickup) {
            _pickup = pickup;
        }

        std::string getPopup (void) const {
            return _popup;
        }
        void setPopup (std::string popup) {
            _popup = popup;
        }

        std::string getToggleHold (void) const {
            return _toggleHold;
        }
        void setToggleHold (std::string hold) {
            _toggleHold = hold;
        }

        std::string getTogglePickupHangup (void) const {
            return _togglePickupHangup;
        }
        void setTogglePickupHangup (std::string toggle) {
            _togglePickupHangup = toggle;
        }

    private:

        std::string _hangup;
        std::string _pickup;
        std::string _popup;
        std::string _toggleHold;
        std::string _togglePickupHangup;
};

#endif
