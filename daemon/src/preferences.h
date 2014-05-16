/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

#ifndef __PREFERENCE_H__
#define __PREFERENCE_H__

#include "config/serializable.h"
#include <string>
#include <map>
#include <vector>

class AudioLayer;

class Preferences : public Serializable {
    public:
        static const char * const DFT_ZONE;
        static const char * const REGISTRATION_EXPIRE_KEY;

        Preferences();

        virtual void serialize(Conf::YamlEmitter &emitter);
        virtual void unserialize(const Conf::YamlNode &map);

        std::string getAccountOrder() const {
            return accountOrder_;
        }

        // flush invalid accountIDs from account order
        void verifyAccountOrder(const std::vector<std::string> &accounts);

        void addAccount(const std::string &acc);
        void removeAccount(const std::string &acc);

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
        std::string zoneToneChoice_;
        int registrationExpire_;
        int portNum_;
        bool searchBarDisplay_;
        bool md5Hash_;
};

class VoipPreference : public Serializable {
    public:
        VoipPreference();

        virtual void serialize(Conf::YamlEmitter &emitter);
        virtual void unserialize(const Conf::YamlNode &map);

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

struct pjsip_msg;

class HookPreference : public Serializable {
    public:
        HookPreference();
        HookPreference(const std::map<std::string, std::string> &settings);

        virtual void serialize(Conf::YamlEmitter &emitter);
        virtual void unserialize(const Conf::YamlNode &map);

        std::string getNumberAddPrefix() const {
            if (numberEnabled_)
                return numberAddPrefix_;
            else
                return "";
        }

        std::map<std::string, std::string> toMap() const;
        void runHook(pjsip_msg *msg);

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

        virtual void serialize(Conf::YamlEmitter &emitter);
        virtual void unserialize(const Conf::YamlNode &map);

        // alsa preference
        int getAlsaCardin() const {
            return alsaCardin_;
        }
        void setAlsaCardin(int c) {
            alsaCardin_ = c;
        }

        int getAlsaCardout() const {
            return alsaCardout_;
        }

        void setAlsaCardout(int c) {
            alsaCardout_ = c;
        }

        int getAlsaCardring() const {
            return alsaCardring_;
        }

        void setAlsaCardring(int c) {
            alsaCardring_ = c;
        }

        std::string getAlsaPlugin() const {
            return alsaPlugin_;
        }

        void setAlsaPlugin(const std::string &p) {
            alsaPlugin_ = p;
        }

        int getAlsaSmplrate() const {
            return alsaSmplrate_;
        }
        void setAlsaSmplrate(int r) {
            alsaSmplrate_ = r;
        }

        //pulseaudio preference
        std::string getPulseDevicePlayback() const {
            return pulseDevicePlayback_;
        }

        void setPulseDevicePlayback(const std::string &p) {
            pulseDevicePlayback_ = p;
        }

        std::string getPulseDeviceRecord() const {
            return pulseDeviceRecord_;
        }
        void setPulseDeviceRecord(const std::string &r) {
            pulseDeviceRecord_ = r;
        }

        std::string getPulseDeviceRingtone() const {
            return pulseDeviceRingtone_;
        }

        void setPulseDeviceRingtone(const std::string &r) {
            pulseDeviceRingtone_ = r;
        }

        // general preference
        std::string getRecordPath() const {
            return recordpath_;
        }

        // Returns true if directory is writeable
        bool setRecordPath(const std::string &r);

        bool getIsAlwaysRecording() const {
            return alwaysRecording_;
        }

        void setIsAlwaysRecording(bool rec) {
            alwaysRecording_ = rec;
        }

        double getVolumemic() const {
            return volumemic_;
        }
        void setVolumemic(double m) {
            volumemic_ = m;
        }

        double getVolumespkr() const {
            return volumespkr_;
        }
        void setVolumespkr(double s) {
            volumespkr_ = s;
        }

        bool isAGCEnabled() const {
            return agcEnabled_;
        }

        void setAGCState(bool enabled) {
            agcEnabled_ = enabled;
        }

        bool getNoiseReduce() const {
            return denoise_;
        }

        void setNoiseReduce(bool enabled) {
            denoise_ = enabled;
        }

        bool getCaptureMuted() const {
            return captureMuted_;
        }

        void setCaptureMuted(bool muted) {
            captureMuted_ = muted;
        }

        bool getPlaybackMuted() const {
            return playbackMuted_;
        }

        void setPlaybackMuted(bool muted) {
            playbackMuted_= muted;
        }

    private:
        std::string audioApi_;

        // alsa preference
        int alsaCardin_;
        int alsaCardout_;
        int alsaCardring_;
        std::string alsaPlugin_;
        int alsaSmplrate_;

        //pulseaudio preference
        std::string pulseDevicePlayback_;
        std::string pulseDeviceRecord_;
        std::string pulseDeviceRingtone_;

        // general preference
        std::string recordpath_; //: /home/msavard/Bureau
        bool alwaysRecording_;
        double volumemic_;
        double volumespkr_;

        bool denoise_;
        bool agcEnabled_;
        bool captureMuted_;
        bool playbackMuted_;
};

class ShortcutPreferences : public Serializable {
    public:
        ShortcutPreferences();
        virtual void serialize(Conf::YamlEmitter &emitter);
        virtual void unserialize(const Conf::YamlNode &map);

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
