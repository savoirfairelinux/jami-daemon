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
static const char * const orderKey = "order";
static const char * const audioApiKey = "audioApi";
static const char * const historyLimitKey = "historyLimit";
static const char * const historyMaxCallsKey = "historyMaxCalls";
static const char * const  notifyMailsKey = "notifyMails";
static const char * const zoneToneChoiceKey = "zoneToneChoice";
static const char * const registrationExpireKey = "registrationExpire";
static const char * const portNumKey = "portNum";
static const char * const searchBarDisplayKey = "searchBarDisplay";
static const char * const zeroConfenableKey = "zeroConfenable";
static const char * const md5HashKey = "md5Hash";

// voip preferences
static const char * const playDtmfKey = "playDtmf";
static const char * const playTonesKey = "playTones";
static const char * const pulseLengthKey = "pulseLength";
static const char * const symmetricRtpKey = "symmetric";
static const char * const zidFileKey = "zidFile";

// addressbook preferences
static const char * const photoKey = "photo";
static const char * const enabledKey = "enabled";
static const char * const listKey = "list";
static const char * const maxResultsKey = "maxResults";
static const char * const businessKey = "business";
static const char * const homeKey = "home";
static const char * const mobileKey = "mobile";

// hooks preferences
static const char * const iax2EnabledKey = "iax2Enabled";
static const char * const numberAddPrefixKey = "numberAddPrefix";
static const char * const numberEnabledKey = "numberEnabled";
static const char * const sipEnabledKey = "sipEnabled";
static const char * const urlCommandKey = "urlCommand";
static const char * const urlSipFieldKey = "urlSipField";

// audio preferences
static const char * const alsamapKey = "alsa";
static const char * const pulsemapKey = "pulse";
static const char * const cardinKey = "cardIn";
static const char * const cardoutKey = "cardOut";
static const char * const cardringKey = "cardRing";
static const char * const pluginKey = "plugin";
static const char * const smplrateKey = "smplRate";
static const char * const devicePlaybackKey = "devicePlayback";
static const char * const deviceRecordKey = "deviceRecord";
static const char * const deviceRingtoneKey = "deviceRingtone";
static const char * const recordpathKey = "recordPath";
static const char * const alwaysRecordingKey = "alwaysRecording";
static const char * const volumemicKey = "volumeMic";
static const char * const volumespkrKey = "volumeSpkr";
static const char * const noiseReduceKey = "noiseReduce";
static const char * const echoCancelKey = "echoCancel";
static const char * const echoTailKey = "echoTailLength";
static const char * const echoDelayKey = "echoDelayLength";

// shortcut preferences
static const char * const hangupShortKey = "hangUp";
static const char * const pickupShortKey = "pickUp";
static const char * const popupShortKey = "popupWindow";
static const char * const toggleHoldShortKey = "toggleHold";
static const char * const togglePickupHangupShortKey = "togglePickupHangup";

class AudioLayer;

class Preferences : public Serializable
{

    public:

        static const char * const DFT_ZONE;

        Preferences();

        virtual void serialize (Conf::YamlEmitter *emitter);

        virtual void unserialize (Conf::MappingNode *map);


        std::string getAccountOrder (void) const {
            return _accountOrder;
        }
        void setAccountOrder (const std::string &ord) {
            _accountOrder = ord;
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
        void setZoneToneChoice (const std::string &str) {
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
        void setZidFile (const std::string &file) {
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
        void setList (const std::string &l) {
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
        void setNumberAddPrefix (const std::string &n) {
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
        void setUrlCommand (const std::string &u) {
            _urlCommand = u;
        }

        std::string getUrlSipField (void) const {
            return _urlSipField;
        }
        void setUrlSipField (const std::string &u) {
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
        AudioLayer *createAudioLayer();
        AudioLayer *switchAndCreateAudioLayer();

        std::string getAudioApi (void) const {
            return _audioApi;
        }

        void setAudioApi (const std::string &api) {
            _audioApi = api;
        }
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

        void setPlugin (const std::string &p) {
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

        void setDeviceRingtone (const std::string &r) {
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
        std::string _audioApi;

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
        virtual void serialize (Conf::YamlEmitter *emitter);
        virtual void unserialize (Conf::MappingNode *map);

        void setShortcuts (std::map<std::string, std::string> shortcuts);
        std::map<std::string, std::string> getShortcuts (void) const;

        std::string getHangup (void) const {
            return _hangup;
        }
        void setHangup (const std::string &hangup) {
            _hangup = hangup;
        }

        std::string getPickup (void) const {
            return _pickup;
        }
        void setPickup (const std::string &pickup) {
            _pickup = pickup;
        }

        std::string getPopup (void) const {
            return _popup;
        }
        void setPopup (const std::string &popup) {
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
        void setTogglePickupHangup (const std::string &toggle) {
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
