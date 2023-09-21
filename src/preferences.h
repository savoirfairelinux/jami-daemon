/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Philippe Gorley <philippe.gorley@savoirfairelinux.com>
 *  Author: Aline Gondim Santos <aline.gondimsantos@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be ful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#ifndef __PREFERENCE_H__
#define __PREFERENCE_H__

#include "config/serializable.h"
#include "client/ring_signal.h"
#include <string>
#include <map>
#include <set>
#include <vector>

namespace YAML {
class Emitter;
class Node;
} // namespace YAML

extern "C" {
struct pjsip_msg;
}

namespace jami {

class AudioLayer;

class Preferences : public Serializable
{
public:
    static const char* const DFT_ZONE;
    static const char* const REGISTRATION_EXPIRE_KEY;

    Preferences();

    void serialize(YAML::Emitter& out) const override;
    void unserialize(const YAML::Node& in) override;

    const std::string& getAccountOrder() const { return accountOrder_; }

    // flush invalid accountIDs from account order
    void verifyAccountOrder(const std::vector<std::string>& accounts);

    void addAccount(const std::string& acc);
    void removeAccount(const std::string& acc);

    void setAccountOrder(const std::string& ord) { accountOrder_ = ord; }

    int getHistoryLimit() const { return historyLimit_; }

    void setHistoryLimit(int lim) { historyLimit_ = lim; }

    int getRingingTimeout() const { return ringingTimeout_; }

    void setRingingTimeout(int timeout) { ringingTimeout_ = timeout; }

    int getHistoryMaxCalls() const { return historyMaxCalls_; }

    void setHistoryMaxCalls(int max) { historyMaxCalls_ = max; }

    std::string getZoneToneChoice() const { return zoneToneChoice_; }

    void setZoneToneChoice(const std::string& str) { zoneToneChoice_ = str; }

    int getPortNum() const { return portNum_; }

    void setPortNum(int port) { portNum_ = port; }

    bool getSearchBarDisplay() const { return searchBarDisplay_; }

    void setSearchBarDisplay(bool search) { searchBarDisplay_ = search; }

    bool getMd5Hash() const { return md5Hash_; }
    void setMd5Hash(bool md5) { md5Hash_ = md5; }

private:
    std::string accountOrder_;
    int historyLimit_;
    int historyMaxCalls_;
    int ringingTimeout_;
    std::string zoneToneChoice_;
    int portNum_;
    bool searchBarDisplay_;
    bool md5Hash_;
    constexpr static const char* const CONFIG_LABEL = "preferences";
};

class VoipPreference : public Serializable
{
public:
    VoipPreference();

    void serialize(YAML::Emitter& out) const override;
    void unserialize(const YAML::Node& in) override;

    bool getPlayDtmf() const { return playDtmf_; }

    void setPlayDtmf(bool dtmf) { playDtmf_ = dtmf; }

    bool getPlayTones() const { return playTones_; }

    void setPlayTones(bool tone) { playTones_ = tone; }

    int getPulseLength() const { return pulseLength_; }

    void setPulseLength(int length) { pulseLength_ = length; }

private:
    bool playDtmf_;
    bool playTones_;
    int pulseLength_;
    constexpr static const char* const CONFIG_LABEL = "voipPreferences";
};

class AudioPreference : public Serializable
{
public:
    AudioPreference();
    AudioLayer* createAudioLayer();

    static std::vector<std::string> getSupportedAudioManagers();

    const std::string& getAudioApi() const { return audioApi_; }

    void setAudioApi(const std::string& api) { audioApi_ = api; }

    void serialize(YAML::Emitter& out) const override;

    void unserialize(const YAML::Node& in) override;

    // alsa preference
    int getAlsaCardin() const { return alsaCardin_; }

    void setAlsaCardin(int c) { alsaCardin_ = c; }

    int getAlsaCardout() const { return alsaCardout_; }

    void setAlsaCardout(int c) { alsaCardout_ = c; }

    int getAlsaCardRingtone() const { return alsaCardRingtone_; }

    void setAlsaCardRingtone(int c) { alsaCardRingtone_ = c; }

    const std::string& getAlsaPlugin() const { return alsaPlugin_; }

    void setAlsaPlugin(const std::string& p) { alsaPlugin_ = p; }

    int getAlsaSmplrate() const { return alsaSmplrate_; }

    void setAlsaSmplrate(int r) { alsaSmplrate_ = r; }

    // pulseaudio preference
    const std::string& getPulseDevicePlayback() const { return pulseDevicePlayback_; }

    void setPulseDevicePlayback(const std::string& p) { pulseDevicePlayback_ = p; }

    const std::string& getPulseDeviceRecord() const { return pulseDeviceRecord_; }
    void setPulseDeviceRecord(const std::string& r) { pulseDeviceRecord_ = r; }

    const std::string& getPulseDeviceRingtone() const { return pulseDeviceRingtone_; }

    void setPulseDeviceRingtone(const std::string& r) { pulseDeviceRingtone_ = r; }

    // portaudio preference
    const std::string& getPortAudioDevicePlayback() const { return portaudioDevicePlayback_; }

    void setPortAudioDevicePlayback(const std::string& p) { portaudioDevicePlayback_ = p; }

    const std::string& getPortAudioDeviceRecord() const { return portaudioDeviceRecord_; }

    void setPortAudioDeviceRecord(const std::string& r) { portaudioDeviceRecord_ = r; }

    const std::string& getPortAudioDeviceRingtone() const { return portaudioDeviceRingtone_; }

    void setPortAudioDeviceRingtone(const std::string& r) { portaudioDeviceRingtone_ = r; }

    // general preference
    const std::string& getRecordPath() const { return recordpath_; }

    // Returns true if directory is writeable
    bool setRecordPath(const std::string& r);

    bool getIsAlwaysRecording() const { return alwaysRecording_; }

    void setIsAlwaysRecording(bool rec) { alwaysRecording_ = rec; }

    double getVolumemic() const { return volumemic_; }
    void setVolumemic(double m) { volumemic_ = m; }

    double getVolumespkr() const { return volumespkr_; }
    void setVolumespkr(double s) { volumespkr_ = s; }

    bool isAGCEnabled() const { return agcEnabled_; }

    void setAGCState(bool enabled) { agcEnabled_ = enabled; }

    const std::string& getNoiseReduce() const { return denoise_; }

    void setNoiseReduce(const std::string& enabled) { denoise_ = enabled; }

    bool getCaptureMuted() const { return captureMuted_; }

    void setCaptureMuted(bool muted) { captureMuted_ = muted; }

    bool getPlaybackMuted() const { return playbackMuted_; }

    void setPlaybackMuted(bool muted) { playbackMuted_ = muted; }

    const std::string& getAudioProcessor() const { return audioProcessor_; }

    void setAudioProcessor(const std::string& ap) { audioProcessor_ = ap; }

    bool getVadEnabled() const { return vadEnabled_; }

    void setVad(bool enable) { vadEnabled_ = enable; }

    const std::string& getEchoCanceller() const { return echoCanceller_; }

    void setEchoCancel(std::string& canceller) { echoCanceller_ = canceller; }

private:
    std::string audioApi_;

    // alsa preference
    int alsaCardin_;
    int alsaCardout_;
    int alsaCardRingtone_;
    std::string alsaPlugin_;
    int alsaSmplrate_;

    // pulseaudio preference
    std::string pulseDevicePlayback_;
    std::string pulseDeviceRecord_;
    std::string pulseDeviceRingtone_;

    // portaudio preference
    std::string portaudioDevicePlayback_;
    std::string portaudioDeviceRecord_;
    std::string portaudioDeviceRingtone_;

    // general preference
    std::string recordpath_;
    bool alwaysRecording_;
    double volumemic_;
    double volumespkr_;

    // audio processor preferences
    std::string audioProcessor_;
    std::string denoise_;
    bool agcEnabled_;
    bool vadEnabled_;
    std::string echoCanceller_;

    bool captureMuted_;
    bool playbackMuted_;
    constexpr static const char* const CONFIG_LABEL = "audio";
};

#ifdef ENABLE_VIDEO
class VideoPreferences : public Serializable
{
public:
    VideoPreferences();

    void serialize(YAML::Emitter& out) const override;
    void unserialize(const YAML::Node& in) override;

    bool getDecodingAccelerated() const { return decodingAccelerated_; }

    bool setDecodingAccelerated(bool decodingAccelerated)
    {
        if (decodingAccelerated_ != decodingAccelerated) {
            decodingAccelerated_ = decodingAccelerated;
            emitSignal<libjami::ConfigurationSignal::HardwareDecodingChanged>(decodingAccelerated_);
            return true;
        }
        return false;
    }

    bool getEncodingAccelerated() const { return encodingAccelerated_; }

    bool setEncodingAccelerated(bool encodingAccelerated)
    {
        if (encodingAccelerated_ != encodingAccelerated) {
            encodingAccelerated_ = encodingAccelerated;
            emitSignal<libjami::ConfigurationSignal::HardwareEncodingChanged>(encodingAccelerated_);
            return true;
        }
        return false;
    }

    bool getRecordPreview() const { return recordPreview_; }

    void setRecordPreview(bool rec) { recordPreview_ = rec; }

    int getRecordQuality() const { return recordQuality_; }

    void setRecordQuality(int rec) { recordQuality_ = rec; }

    const std::string& getConferenceResolution() const { return conferenceResolution_; }

    void setConferenceResolution(const std::string& res) { conferenceResolution_ = res; }

private:
    bool decodingAccelerated_;
    bool encodingAccelerated_;
    bool recordPreview_;
    int recordQuality_;
    std::string conferenceResolution_;
    constexpr static const char* const CONFIG_LABEL = "video";
};
#endif // ENABLE_VIDEO

#ifdef ENABLE_PLUGIN
class PluginPreferences : public Serializable
{
public:
    PluginPreferences();

    void serialize(YAML::Emitter& out) const override;
    void unserialize(const YAML::Node& in) override;

    bool getPluginsEnabled() const { return pluginsEnabled_; }

    void setPluginsEnabled(bool pluginsEnabled) { pluginsEnabled_ = pluginsEnabled; }

    std::vector<std::string> getLoadedPlugins() const
    {
        std::vector<std::string> v(loadedPlugins_.begin(), loadedPlugins_.end());
        return v;
    }

    std::vector<std::string> getInstalledPlugins() const
    {
        return std::vector<std::string>(installedPlugins_.begin(), installedPlugins_.end());
    }

    void saveStateLoadedPlugins(std::string plugin, bool loaded)
    {
        if (loaded) {
            if (loadedPlugins_.find(plugin) != loadedPlugins_.end())
                return;
            loadedPlugins_.emplace(plugin);
        } else {
            auto it = loadedPlugins_.find(plugin);
            if (it != loadedPlugins_.end())
                loadedPlugins_.erase(it);
        }
    }

private:
    bool pluginsEnabled_;
    std::set<std::string> installedPlugins_;
    std::set<std::string> loadedPlugins_;
    constexpr static const char* const CONFIG_LABEL = "plugins";
};
#endif // ENABLE_PLUGIN

} // namespace jami

#endif
