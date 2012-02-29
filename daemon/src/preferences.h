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
#include <string>
#include <map>

// general preferences
static const char * const orderKey = "order";
static const char * const audioApiKey = "audioApi";
static const char * const historyLimitKey = "historyLimit";
static const char * const historyMaxCallsKey = "historyMaxCalls";
static const char * const notifyMailsKey = "notifyMails";
static const char * const zoneToneChoiceKey = "zoneToneChoice";
static const char * const registrationExpireKey = "registrationexpire";
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

class Preferences : public Serializable {
    public:
        static const char * const DFT_ZONE;

        Preferences();

        virtual void serialize(Conf::YamlEmitter *emitter);

        virtual void unserialize(const Conf::MappingNode *map);

        std::string getAccountOrder() const {
            return accountOrder_;
        }

        void setAccountOrder(const std::string &ord) {
            accountOrder_ = ord;
        }

        int getHistoryLimit() const {
            return historyLimit_;
        }

        void setHistoryLimit(int lim) {
            historyLimit_ = lim;
        }

        int getHistoryMaxCalls() const {
            return historyMaxCalls_;
        }

        void setHistoryMaxCalls(int max) {
            historyMaxCalls_ = max;
        }

        bool getNotifyMails() const {
            return notifyMails_;
        }

        void setNotifyMails(bool mails) {
            notifyMails_ = mails;
        }

        std::string getZoneToneChoice() const {
            return zoneToneChoice_;
        }

        void setZoneToneChoice(const std::string &str) {
            zoneToneChoice_ = str;
        }

        int getRegistrationExpire() const {
            return registrationExpire_;
        }

        void setRegistrationExpire(int exp) {
            registrationExpire_ = exp;
        }

        int getPortNum() const {
            return portNum_;
        }

        void setPortNum(int port) {
            portNum_ = port;
        }

        bool getSearchBarDisplay() const {
            return searchBarDisplay_;
        }

        void setSearchBarDisplay(bool search) {
            searchBarDisplay_ = search;
        }

        bool getZeroConfenable() const {
            return zeroConfenable_;
        }
        void setZeroConfenable(bool enable) {
            zeroConfenable_ = enable;
        }

        bool getMd5Hash() const {
            return md5Hash_;
        }
        void setMd5Hash(bool md5) {
            md5Hash_ = md5;
        }

    private:
        std::string accountOrder_;
        int historyLimit_;
        int historyMaxCalls_;
        bool notifyMails_;
        std::string zoneToneChoice_;
        int registrationExpire_;
        int portNum_;
        bool searchBarDisplay_;
        bool zeroConfenable_;
        bool md5Hash_;
};

class VoipPreference : public Serializable {
    public:
        VoipPreference();

        virtual void serialize(Conf::YamlEmitter *emitter);

        virtual void unserialize(const Conf::MappingNode *map);

        bool getPlayDtmf() const {
            return playDtmf_;
        }

        void setPlayDtmf(bool dtmf) {
            playDtmf_ = dtmf;
        }

        bool getPlayTones() const {
            return playTones_;
        }

        void setPlayTones(bool tone) {
            playTones_ = tone;
        }

        int getPulseLength() const {
            return pulseLength_;
        }

        void setPulseLength(int length) {
            pulseLength_ = length;
        }

        bool getSymmetricRtp() const {
            return symmetricRtp_;
        }
        void setSymmetricRtp(bool sym) {
            symmetricRtp_ = sym;
        }

        std::string getZidFile() const {
            return zidFile_;
        }
        void setZidFile(const std::string &file) {
            zidFile_ = file;
        }

    private:

        bool playDtmf_;
        bool playTones_;
        int pulseLength_;
        bool symmetricRtp_;
        std::string zidFile_;
};

class AddressbookPreference : public Serializable {
    public:
        AddressbookPreference();

        virtual void serialize(Conf::YamlEmitter *emitter);

        virtual void unserialize(const Conf::MappingNode *map);

        bool getPhoto() const {
            return photo_;
        }

        void setPhoto(bool p) {
            photo_ = p;
        }

        bool getEnabled() const {
            return enabled_;
        }

        void setEnabled(bool e) {
            enabled_ = e;
        }

        std::string getList() const {
            return list_;
        }

        void setList(const std::string &l) {
            list_ = l;
        }

        int getMaxResults() const {
            return maxResults_;
        }

        void setMaxResults(int r) {
            maxResults_ = r;
        }

        bool getBusiness() const {
            return business_;
        }

        void setBusiness(bool b) {
            business_ = b;
        }

        bool getHome() const {
            return home_;
        }
        void setHone(bool h) {
            home_ = h;
        }

        bool getMobile() const {
            return mobile_;
        }
        void setMobile(bool m) {
            mobile_ = m;
        }

    private:

        bool photo_;
        bool enabled_;
        std::string list_;
        int maxResults_;
        bool business_;
        bool home_;
        bool mobile_;

};


class HookPreference : public Serializable {
    public:
        HookPreference();

        virtual void serialize(Conf::YamlEmitter *emitter);

        virtual void unserialize(const Conf::MappingNode *map);

        bool getIax2Enabled() const {
            return iax2Enabled_;
        }

        void setIax2Enabled(bool i) {
            iax2Enabled_ = i;
        }

        std::string getNumberAddPrefix() const {
            return numberAddPrefix_;
        }

        void setNumberAddPrefix(const std::string &n) {
            numberAddPrefix_ = n;
        }

        bool getNumberEnabled() const {
            return numberEnabled_;
        }

        void setNumberEnabled(bool n) {
            numberEnabled_ = n;
        }

        bool getSipEnabled() const {
            return sipEnabled_;
        }

        void setSipEnabled(bool s) {
            sipEnabled_ = s;
        }

        std::string getUrlCommand() const {
            return urlCommand_;
        }
        void setUrlCommand(const std::string &u) {
            urlCommand_ = u;
        }

        std::string getUrlSipField() const {
            return urlSipField_;
        }
        void setUrlSipField(const std::string &u) {
            urlSipField_ = u;
        }

    private:
        bool iax2Enabled_;
        std::string numberAddPrefix_;
        bool numberEnabled_;
        bool sipEnabled_;
        std::string urlCommand_;
        std::string urlSipField_;

};

class AudioPreference : public Serializable {
    public:
        AudioPreference();
        AudioLayer *createAudioLayer();
        AudioLayer *switchAndCreateAudioLayer();

        std::string getAudioApi() const {
            return audioApi_;
        }

        void setAudioApi(const std::string &api) {
            audioApi_ = api;
        }

        virtual void serialize(Conf::YamlEmitter *emitter);

        virtual void unserialize(const Conf::MappingNode *map);

        // alsa preference
        int getCardin() const {
            return cardin_;
        }
        void setCardin(int c) {
            cardin_ = c;
        }

        int getCardout() const {
            return cardout_;
        }

        void setCardout(int c) {
            cardout_ = c;
        }

        int getCardring() const {
            return cardring_;
        }

        void setCardring(int c) {
            cardring_ = c;
        }

        std::string getPlugin() const {
            return plugin_;
        }

        void setPlugin(const std::string &p) {
            plugin_ = p;
        }

        int getSmplrate() const {
            return smplrate_;
        }
        void setSmplrate(int r) {
            smplrate_ = r;
        }

        //pulseaudio preference
        std::string getDevicePlayback() const {
            return devicePlayback_;
        }

        void setDevicePlayback(const std::string &p) {
            devicePlayback_ = p;
        }

        std::string getDeviceRecord() const {
            return deviceRecord_;
        }
        void setDeviceRecord(const std::string &r) {
            deviceRecord_ = r;
        }

        std::string getDeviceRingtone() const {
            return deviceRingtone_;
        }

        void setDeviceRingtone(const std::string &r) {
            deviceRingtone_ = r;
        }

        // general preference
        std::string getRecordpath() const {
            return recordpath_;
        }
        void setRecordpath(const std::string &r) {
            recordpath_ = r;
        }

        bool getIsAlwaysRecording() const {
            return alwaysRecording_;
        }

        void setIsAlwaysRecording(bool rec) {
            alwaysRecording_ = rec;
        }

        int getVolumemic() const {
            return volumemic_;
        }
        void setVolumemic(int m) {
            volumemic_ = m;
        }

        int getVolumespkr() const {
            return volumespkr_;
        }
        void setVolumespkr(int s) {
            volumespkr_ = s;
        }

        bool getNoiseReduce() const {
            return noisereduce_;
        }

        void setNoiseReduce(bool noise) {
            noisereduce_ = noise;
        }

        bool getEchoCancel() const {
            return echocancel_;
        }

        void setEchoCancel(bool echo) {
            echocancel_ = echo;
        }

        int getEchoCancelTailLength() const {
            return echoCancelTailLength_;
        }

        void setEchoCancelTailLength(int length) {
            echoCancelTailLength_ = length;
        }

        int getEchoCancelDelay() const {
            return echoCancelDelay_;
        }

        void setEchoCancelDelay(int delay) {
            echoCancelDelay_ = delay;
        }

    private:
        std::string audioApi_;

        // alsa preference
        int cardin_;
        int cardout_;
        int cardring_;
        std::string plugin_;
        int smplrate_;

        //pulseaudio preference
        std::string devicePlayback_;
        std::string deviceRecord_;
        std::string deviceRingtone_;

        // general preference
        std::string recordpath_; //: /home/msavard/Bureau
        bool alwaysRecording_;
        int volumemic_;
        int volumespkr_;

        bool noisereduce_;
        bool echocancel_;
        int echoCancelTailLength_;
        int echoCancelDelay_;
};

class ShortcutPreferences : public Serializable {
    public:
        ShortcutPreferences();
        virtual void serialize(Conf::YamlEmitter *emitter);
        virtual void unserialize(const Conf::MappingNode *map);

        void setShortcuts(std::map<std::string, std::string> shortcuts);
        std::map<std::string, std::string> getShortcuts() const;

        std::string getHangup() const {
            return hangup_;
        }

        void setHangup(const std::string &hangup) {
            hangup_ = hangup;
        }

        std::string getPickup() const {
            return pickup_;
        }

        void setPickup(const std::string &pickup) {
            pickup_ = pickup;
        }

        std::string getPopup() const {
            return popup_;
        }

        void setPopup(const std::string &popup) {
            popup_ = popup;
        }

        std::string getToggleHold() const {
            return toggleHold_;
        }

        void setToggleHold(const std::string &hold) {
            toggleHold_ = hold;
        }

        std::string getTogglePickupHangup() const {
            return togglePickupHangup_;
        }

        void setTogglePickupHangup(const std::string &toggle) {
            togglePickupHangup_ = toggle;
        }

    private:
        std::string hangup_;
        std::string pickup_;
        std::string popup_;
        std::string toggleHold_;
        std::string togglePickupHangup_;
};

#endif
