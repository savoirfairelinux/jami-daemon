/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "preferences.h"
#include "logger.h"
#include "audio/audiolayer.h"
#if HAVE_OPENSL
#include "audio/opensl/opensllayer.h"
#else
#if HAVE_ALSA
#include "audio/alsa/alsalayer.h"
#endif
#if HAVE_JACK
#include "audio/jack/jacklayer.h"
#endif
#if HAVE_PULSE
#include "audio/pulseaudio/pulselayer.h"
#endif
#if HAVE_COREAUDIO
#ifdef __APPLE__
#include <TargetConditionals.h>
#endif
#if TARGET_OS_IOS
#include "audio/coreaudio/ios/corelayer.h"
#else
#include "audio/coreaudio/osx/corelayer.h"
#endif /* TARGET_OS_IOS */
#endif /* HAVE_COREAUDIO */
#if HAVE_PORTAUDIO
#include "audio/portaudio/portaudiolayer.h"
#endif
#endif /* HAVE_OPENSL */

#ifdef ENABLE_VIDEO
#include "client/videomanager.h"
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <yaml-cpp/yaml.h>
#pragma GCC diagnostic pop

#include "config/yamlparser.h"
#include "hooks/urlhook.h"
#include "sip/sip_utils.h"
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include "fileutils.h"
#include "string_utils.h"

namespace jami {

using yaml_utils::parseValue;

constexpr const char * const Preferences::CONFIG_LABEL;
const char * const Preferences::DFT_ZONE = "North America";
const char * const Preferences::REGISTRATION_EXPIRE_KEY = "registrationexpire";

// general preferences
static constexpr const char* ORDER_KEY {"order"};
static constexpr const char* AUDIO_API_KEY {"audioApi"};
static constexpr const char* HISTORY_LIMIT_KEY {"historyLimit"};
static constexpr const char* RINGING_TIMEOUT {"ringingTimeout"};
static constexpr const char* HISTORY_MAX_CALLS_KEY {"historyMaxCalls"};
static constexpr const char* ZONE_TONE_CHOICE_KEY {"zoneToneChoice"};
static constexpr const char* PORT_NUM_KEY {"portNum"};
static constexpr const char* SEARCH_BAR_DISPLAY_KEY {"searchBarDisplay"};
static constexpr const char* MD5_HASH_KEY {"md5Hash"};

// voip preferences
constexpr const char * const VoipPreference::CONFIG_LABEL;
static constexpr const char* PLAY_DTMF_KEY {"playDtmf"};
static constexpr const char* PLAY_TONES_KEY {"playTones"};
static constexpr const char* PULSE_LENGTH_KEY {"pulseLength"};
static constexpr const char* SYMMETRIC_RTP_KEY {"symmetric"};
static constexpr const char* ZID_FILE_KEY {"zidFile"};

// hooks preferences
constexpr const char * const HookPreference::CONFIG_LABEL;
static constexpr const char* NUMBER_ADD_PREFIX_KEY {"numberAddPrefix"};
static constexpr const char* NUMBER_ENABLED_KEY {"numberEnabled"};
static constexpr const char* SIP_ENABLED_KEY {"sipEnabled"};
static constexpr const char* URL_COMMAND_KEY {"urlCommand"};
static constexpr const char* URL_SIP_FIELD_KEY {"urlSipField"};

// audio preferences
constexpr const char * const AudioPreference::CONFIG_LABEL;
static constexpr const char* ALSAMAP_KEY {"alsa"};
static constexpr const char* PULSEMAP_KEY {"pulse"};
static constexpr const char* CARDIN_KEY {"cardIn"};
static constexpr const char* CARDOUT_KEY {"cardOut"};
static constexpr const char* CARDRING_KEY {"cardRing"};
static constexpr const char* PLUGIN_KEY {"plugin"};
static constexpr const char* SMPLRATE_KEY {"smplRate"};
static constexpr const char* DEVICE_PLAYBACK_KEY {"devicePlayback"};
static constexpr const char* DEVICE_RECORD_KEY {"deviceRecord"};
static constexpr const char* DEVICE_RINGTONE_KEY {"deviceRingtone"};
static constexpr const char* RECORDPATH_KEY {"recordPath"};
static constexpr const char* ALWAYS_RECORDING_KEY {"alwaysRecording"};
static constexpr const char* VOLUMEMIC_KEY {"volumeMic"};
static constexpr const char* VOLUMESPKR_KEY {"volumeSpkr"};
static constexpr const char* NOISE_REDUCE_KEY {"noiseReduce"};
static constexpr const char* AGC_KEY {"automaticGainControl"};
static constexpr const char* CAPTURE_MUTED_KEY {"captureMuted"};
static constexpr const char* PLAYBACK_MUTED_KEY {"playbackMuted"};

// shortcut preferences
constexpr const char * const ShortcutPreferences::CONFIG_LABEL;
static constexpr const char* HANGUP_SHORT_KEY {"hangUp"};
static constexpr const char* PICKUP_SHORT_KEY {"pickUp"};
static constexpr const char* POPUP_SHORT_KEY {"popupWindow"};
static constexpr const char* TOGGLE_HOLD_SHORT_KEY {"toggleHold"};
static constexpr const char* TOGGLE_PICKUP_HANGUP_SHORT_KEY {"togglePickupHangup"};

#ifdef ENABLE_VIDEO
// video preferences
constexpr const char * const VideoPreferences::CONFIG_LABEL;
static constexpr const char* DECODING_ACCELERATED_KEY {"decodingAccelerated"};
static constexpr const char* ENCODING_ACCELERATED_KEY {"encodingAccelerated"};
#endif

static constexpr int PULSE_LENGTH_DEFAULT {250}; /** Default DTMF length */
#ifndef _MSC_VER
static constexpr const char* ALSA_DFT_CARD {"0"};          /** Default sound card index */
#else
static constexpr const char* ALSA_DFT_CARD {"-1"};         /** Default sound card index (Portaudio) */
#endif // _MSC_VER


Preferences::Preferences() :
    accountOrder_("")
    , historyLimit_(0)
    , historyMaxCalls_(20)
    , ringingTimeout_(30)
    , zoneToneChoice_(DFT_ZONE) // DFT_ZONE
    , registrationExpire_(180)
    , portNum_(sip_utils::DEFAULT_SIP_PORT)
    , searchBarDisplay_(true)
    , md5Hash_(false)
{}

void Preferences::verifyAccountOrder(const std::vector<std::string> &accountIDs)
{
    std::vector<std::string> tokens;
    std::string token;
    bool drop = false;

    for (const auto c : accountOrder_) {
        if (c != '/') {
            token += c;
        } else {
            if (find(accountIDs.begin(), accountIDs.end(), token) != accountIDs.end())
                tokens.push_back(token);
            else {
                JAMI_DBG("Dropping nonexistent account %s", token.c_str());
                drop = true;
            }
            token.clear();
        }
    }

    if (drop) {
        accountOrder_.clear();
        for (const auto &t : tokens)
            accountOrder_ += t + "/";
    }
}

void Preferences::addAccount(const std::string &newAccountID)
{
    // Add the newly created account in the account order list
    if (not accountOrder_.empty())
        accountOrder_.insert(0, newAccountID + "/");
    else
        accountOrder_ = newAccountID + "/";
}

void Preferences::removeAccount(const std::string &oldAccountID)
{
    // include the slash since we don't want to remove a partial match
    const size_t start = accountOrder_.find(oldAccountID + "/");
    if (start != std::string::npos)
        accountOrder_.erase(start, oldAccountID.length() + 1);
}

void Preferences::serialize(YAML::Emitter &out) const
{
    out << YAML::Key << CONFIG_LABEL << YAML::Value << YAML::BeginMap;

    out << YAML::Key << HISTORY_LIMIT_KEY << YAML::Value << historyLimit_;
    out << YAML::Key << RINGING_TIMEOUT << YAML::Value << ringingTimeout_;
    out << YAML::Key << HISTORY_MAX_CALLS_KEY << YAML::Value << historyMaxCalls_;
    out << YAML::Key << MD5_HASH_KEY << YAML::Value << md5Hash_;
    out << YAML::Key << ORDER_KEY << YAML::Value << accountOrder_;
    out << YAML::Key << PORT_NUM_KEY << YAML::Value << portNum_;
    out << YAML::Key << REGISTRATION_EXPIRE_KEY << YAML::Value << registrationExpire_;
    out << YAML::Key << SEARCH_BAR_DISPLAY_KEY << YAML::Value << searchBarDisplay_;
    out << YAML::Key << ZONE_TONE_CHOICE_KEY << YAML::Value << zoneToneChoice_;
    out << YAML::EndMap;
}

void Preferences::unserialize(const YAML::Node &in)
{
    const auto &node = in[CONFIG_LABEL];

    parseValue(node, ORDER_KEY, accountOrder_);
    parseValue(node, HISTORY_LIMIT_KEY, historyLimit_);
    parseValue(node, RINGING_TIMEOUT, ringingTimeout_);
    parseValue(node, HISTORY_MAX_CALLS_KEY, historyMaxCalls_);
    parseValue(node, ZONE_TONE_CHOICE_KEY, zoneToneChoice_);
    parseValue(node, REGISTRATION_EXPIRE_KEY, registrationExpire_);
    parseValue(node, PORT_NUM_KEY, portNum_);
    parseValue(node, SEARCH_BAR_DISPLAY_KEY, searchBarDisplay_);
    parseValue(node, MD5_HASH_KEY, md5Hash_);
}

VoipPreference::VoipPreference() :
    playDtmf_(true)
    , playTones_(true)
    , pulseLength_(PULSE_LENGTH_DEFAULT)
    , symmetricRtp_(true)
{}

void VoipPreference::serialize(YAML::Emitter &out) const
{
    out << YAML::Key << CONFIG_LABEL << YAML::Value << YAML::BeginMap;
    out << YAML::Key << PLAY_DTMF_KEY << YAML::Value << playDtmf_;
    out << YAML::Key << PLAY_TONES_KEY << YAML::Value << playTones_;
    out << YAML::Key << PULSE_LENGTH_KEY << YAML::Value << pulseLength_;
    out << YAML::Key << SYMMETRIC_RTP_KEY << YAML::Value << symmetricRtp_;
    out << YAML::Key << ZID_FILE_KEY << YAML::Value << zidFile_;
    out << YAML::EndMap;
}

void VoipPreference::unserialize(const YAML::Node &in)
{
    const auto &node = in[CONFIG_LABEL];
    parseValue(node, PLAY_DTMF_KEY, playDtmf_);
    parseValue(node, PLAY_TONES_KEY, playTones_);
    parseValue(node, PULSE_LENGTH_KEY, pulseLength_);
    parseValue(node, SYMMETRIC_RTP_KEY, symmetricRtp_);
    parseValue(node, ZID_FILE_KEY, zidFile_);
}

HookPreference::HookPreference()
    : numberAddPrefix_("")
    , numberEnabled_(false)
    , sipEnabled_(false)
    , urlCommand_("x-www-browser")
    , urlSipField_("X-ring-url")
{}

HookPreference::HookPreference(const std::map<std::string, std::string> &settings)
    : numberAddPrefix_(settings.find("PHONE_NUMBER_HOOK_ADD_PREFIX")->second)
    , numberEnabled_(settings.find("PHONE_NUMBER_HOOK_ENABLED")->second == "true")
    , sipEnabled_(settings.find("URLHOOK_SIP_ENABLED")->second == "true")
    , urlCommand_(settings.find("URLHOOK_COMMAND")->second)
    , urlSipField_(settings.find("URLHOOK_SIP_FIELD")->second)
{}

std::map<std::string, std::string> HookPreference::toMap() const
{
    std::map<std::string, std::string> settings;
    settings["PHONE_NUMBER_HOOK_ADD_PREFIX"] = numberAddPrefix_;
    settings["PHONE_NUMBER_HOOK_ENABLED"] = numberEnabled_ ? "true" : "false";
    settings["URLHOOK_SIP_ENABLED"] = sipEnabled_ ? "true" : "false";
    settings["URLHOOK_COMMAND"] = urlCommand_;
    settings["URLHOOK_SIP_FIELD"] = urlSipField_;

    return settings;
}

void HookPreference::serialize(YAML::Emitter &out) const
{
    out << YAML::Key << CONFIG_LABEL << YAML::Value << YAML::BeginMap;
    out << YAML::Key << NUMBER_ADD_PREFIX_KEY << YAML::Value << numberAddPrefix_;
    out << YAML::Key << SIP_ENABLED_KEY << YAML::Value << sipEnabled_;
    out << YAML::Key << URL_COMMAND_KEY << YAML::Value << urlCommand_;
    out << YAML::Key << URL_SIP_FIELD_KEY << YAML::Value << urlSipField_;
    out << YAML::EndMap;
}

void HookPreference::unserialize(const YAML::Node &in)
{
    const auto &node = in[CONFIG_LABEL];

    parseValue(node, NUMBER_ADD_PREFIX_KEY, numberAddPrefix_);
    parseValue(node, SIP_ENABLED_KEY, sipEnabled_);
    parseValue(node, URL_COMMAND_KEY, urlCommand_);
    parseValue(node, URL_SIP_FIELD_KEY, urlSipField_);
}

void HookPreference::runHook(pjsip_msg *msg)
{
    if (sipEnabled_) {
        const std::string header(sip_utils::fetchHeaderValue(msg, urlSipField_));
        UrlHook::runAction(urlCommand_, header);
    }
}

AudioPreference::AudioPreference() :
    audioApi_(PULSEAUDIO_API_STR)
    , alsaCardin_(atoi(ALSA_DFT_CARD))
    , alsaCardout_(atoi(ALSA_DFT_CARD))
    , alsaCardring_(atoi(ALSA_DFT_CARD))
    , alsaPlugin_("default")
    , alsaSmplrate_(44100)
    , pulseDevicePlayback_("")
    , pulseDeviceRecord_("")
    , pulseDeviceRingtone_("")
    , recordpath_("")
    , alwaysRecording_(false)
    , volumemic_(1.0)
    , volumespkr_(1.0)
    , denoise_(false)
    , agcEnabled_(false)
    , captureMuted_(false)
    , playbackMuted_(false)
{}

#if HAVE_ALSA

static const int ALSA_DFT_CARD_ID = 0; // Index of the default soundcard

static void
checkSoundCard(int &card, DeviceType type)
{
    if (not AlsaLayer::soundCardIndexExists(card, type)) {
        JAMI_WARN(" Card with index %d doesn't exist or is unusable.", card);
        card = ALSA_DFT_CARD_ID;
    }
}
#endif

AudioLayer*
AudioPreference::createAudioLayer()
{
#if HAVE_OPENSL
    return new OpenSLLayer(*this);
#else

#if HAVE_JACK
    if (audioApi_ == JACK_API_STR) {
        try {
            if (auto ret = system("jack_lsp > /dev/null"))
                throw std::runtime_error("Error running jack_lsp: " + std::to_string(ret));
            return new JackLayer(*this);
        } catch (const std::runtime_error& e) {
            JAMI_ERR("%s", e.what());
#if HAVE_PULSE
            audioApi_ = PULSEAUDIO_API_STR;
#elif HAVE_ALSA
            audioApi_ = ALSA_API_STR;
#elif HAVE_COREAUDIO
            audioApi_ = COREAUDIO_API_STR;
#elif HAVE_PORTAUDIO
            audioApi_ = PORTAUDIO_API_STR;
#else
            throw;
#endif // HAVE_PULSE
        }
    }
#endif // HAVE_JACK

#if HAVE_PULSE

    if (audioApi_ == PULSEAUDIO_API_STR) {
        try {
            return new PulseLayer(*this);
        } catch (const std::runtime_error &e) {
            JAMI_WARN("Could not create pulseaudio layer, falling back to ALSA");
        }
    }

#endif

#if HAVE_ALSA

    audioApi_ = ALSA_API_STR;
    checkSoundCard(alsaCardin_, DeviceType::CAPTURE);
    checkSoundCard(alsaCardout_, DeviceType::PLAYBACK);
    checkSoundCard(alsaCardring_, DeviceType::RINGTONE);

    return new AlsaLayer(*this);
#endif

#if HAVE_COREAUDIO
    audioApi_ = COREAUDIO_API_STR;
    try {
        return new CoreLayer(*this);
    } catch (const std::runtime_error &e) {
        JAMI_WARN("Could not create coreaudio layer. There will be no sound.");
    }
    return NULL;
#endif

#if HAVE_PORTAUDIO
    audioApi_ = PORTAUDIO_API_STR;
    try {
        return new PortAudioLayer(*this);
    } catch (const std::runtime_error &e) {
        JAMI_WARN("Could not create PortAudio layer. There will be no sound.");
    }
    return nullptr;
#endif
#endif // __ANDROID__

    JAMI_WARN("No audio layer provided");
    return nullptr;
}

void AudioPreference::serialize(YAML::Emitter &out) const
{
    out << YAML::Key << CONFIG_LABEL << YAML::Value << YAML::BeginMap;
    // alsa submap
    out << YAML::Key << ALSAMAP_KEY << YAML::Value << YAML::BeginMap;
    out << YAML::Key << CARDIN_KEY << YAML::Value << alsaCardin_;
    out << YAML::Key << CARDOUT_KEY << YAML::Value << alsaCardout_;
    out << YAML::Key << CARDRING_KEY << YAML::Value << alsaCardring_;
    out << YAML::Key << PLUGIN_KEY << YAML::Value << alsaPlugin_;
    out << YAML::Key << SMPLRATE_KEY << YAML::Value << alsaSmplrate_;
    out << YAML::EndMap;

    // common options
    out << YAML::Key << ALWAYS_RECORDING_KEY << YAML::Value << alwaysRecording_;
    out << YAML::Key << AUDIO_API_KEY << YAML::Value << audioApi_;
    out << YAML::Key << AGC_KEY << YAML::Value << agcEnabled_;
    out << YAML::Key << CAPTURE_MUTED_KEY << YAML::Value << captureMuted_;
    out << YAML::Key << NOISE_REDUCE_KEY << YAML::Value << denoise_;
    out << YAML::Key << PLAYBACK_MUTED_KEY << YAML::Value << playbackMuted_;

    // pulse submap
    out << YAML::Key << PULSEMAP_KEY << YAML::Value << YAML::BeginMap;
    out << YAML::Key << DEVICE_PLAYBACK_KEY << YAML::Value << pulseDevicePlayback_;
    out << YAML::Key << DEVICE_RECORD_KEY << YAML::Value << pulseDeviceRecord_;
    out << YAML::Key << DEVICE_RINGTONE_KEY << YAML::Value << pulseDeviceRingtone_;
    out << YAML::EndMap;

    // more common options!
    out << YAML::Key << RECORDPATH_KEY << YAML::Value << recordpath_;
    out << YAML::Key << VOLUMEMIC_KEY << YAML::Value << volumemic_;
    out << YAML::Key << VOLUMESPKR_KEY << YAML::Value << volumespkr_;

    out << YAML::EndMap;
}

bool
AudioPreference::setRecordPath(const std::string &r)
{
    std::string path = fileutils::expand_path(r);
    if (fileutils::isDirectoryWritable(path)) {
        recordpath_ = path;
        return true;
    } else {
        JAMI_ERR("%s is not writable, cannot be the recording path", path.c_str());
        return false;
    }
}

void AudioPreference::unserialize(const YAML::Node &in)
{
    const auto &node = in[CONFIG_LABEL];

    // alsa submap
    const auto &alsa = node[ALSAMAP_KEY];

    parseValue(alsa, CARDIN_KEY, alsaCardin_);
    parseValue(alsa, CARDOUT_KEY, alsaCardout_);
    parseValue(alsa, CARDRING_KEY, alsaCardring_);
    parseValue(alsa, PLUGIN_KEY, alsaPlugin_);
    parseValue(alsa, SMPLRATE_KEY, alsaSmplrate_);

    // common options
    parseValue(node, ALWAYS_RECORDING_KEY, alwaysRecording_);
    parseValue(node, AUDIO_API_KEY, audioApi_);
    parseValue(node, AGC_KEY, agcEnabled_);
    parseValue(node, CAPTURE_MUTED_KEY, captureMuted_);
    parseValue(node, NOISE_REDUCE_KEY, denoise_);
    parseValue(node, PLAYBACK_MUTED_KEY, playbackMuted_);

    // pulse submap
    const auto &pulse = node[PULSEMAP_KEY];
    parseValue(pulse, DEVICE_PLAYBACK_KEY, pulseDevicePlayback_);
    parseValue(pulse, DEVICE_RECORD_KEY, pulseDeviceRecord_);
    parseValue(pulse, DEVICE_RINGTONE_KEY, pulseDeviceRingtone_);

    // more common options!
    parseValue(node, RECORDPATH_KEY, recordpath_);
    parseValue(node, VOLUMEMIC_KEY, volumemic_);
    parseValue(node, VOLUMESPKR_KEY, volumespkr_);
}

ShortcutPreferences::ShortcutPreferences() : hangup_(), pickup_(), popup_(),
    toggleHold_(), togglePickupHangup_() {}

std::map<std::string, std::string> ShortcutPreferences::getShortcuts() const
{
    std::map<std::string, std::string> shortcutsMap;

    shortcutsMap[HANGUP_SHORT_KEY] = hangup_;
    shortcutsMap[PICKUP_SHORT_KEY] = pickup_;
    shortcutsMap[POPUP_SHORT_KEY] = popup_;
    shortcutsMap[TOGGLE_HOLD_SHORT_KEY] = toggleHold_;
    shortcutsMap[TOGGLE_PICKUP_HANGUP_SHORT_KEY] = togglePickupHangup_;

    return shortcutsMap;
}

void ShortcutPreferences::setShortcuts(std::map<std::string, std::string> map)
{
    hangup_ = map[HANGUP_SHORT_KEY];
    pickup_ = map[PICKUP_SHORT_KEY];
    popup_ = map[POPUP_SHORT_KEY];
    toggleHold_ = map[TOGGLE_HOLD_SHORT_KEY];
    togglePickupHangup_ = map[TOGGLE_PICKUP_HANGUP_SHORT_KEY];
}


void ShortcutPreferences::serialize(YAML::Emitter &out) const
{
    out << YAML::Key << CONFIG_LABEL << YAML::Value << YAML::BeginMap;
    out << YAML::Key << HANGUP_SHORT_KEY << YAML::Value << hangup_;
    out << YAML::Key << PICKUP_SHORT_KEY << YAML::Value << pickup_;
    out << YAML::Key << POPUP_SHORT_KEY << YAML::Value << popup_;
    out << YAML::Key << TOGGLE_HOLD_SHORT_KEY << YAML::Value << toggleHold_;
    out << YAML::Key << TOGGLE_PICKUP_HANGUP_SHORT_KEY << YAML::Value << togglePickupHangup_;
    out << YAML::EndMap;
}

void ShortcutPreferences::unserialize(const YAML::Node &in)
{
    const auto &node = in[CONFIG_LABEL];

    parseValue(node, HANGUP_SHORT_KEY, hangup_);
    parseValue(node, PICKUP_SHORT_KEY, pickup_);
    parseValue(node, POPUP_SHORT_KEY, popup_);
    parseValue(node, TOGGLE_HOLD_SHORT_KEY, toggleHold_);
    parseValue(node, TOGGLE_PICKUP_HANGUP_SHORT_KEY, togglePickupHangup_);
}

#ifdef ENABLE_VIDEO
VideoPreferences::VideoPreferences()
    : decodingAccelerated_(true)
    , encodingAccelerated_(false)
{
}

void VideoPreferences::serialize(YAML::Emitter &out) const
{
    out << YAML::Key << CONFIG_LABEL << YAML::Value << YAML::BeginMap;
#ifdef RING_ACCEL
    out << YAML::Key << DECODING_ACCELERATED_KEY << YAML::Value << decodingAccelerated_;
    out << YAML::Key << ENCODING_ACCELERATED_KEY << YAML::Value << encodingAccelerated_;
#endif
    getVideoDeviceMonitor().serialize(out);
    out << YAML::EndMap;
}

void VideoPreferences::unserialize(const YAML::Node &in)
{
    const auto &node = in[CONFIG_LABEL];
#ifdef RING_ACCEL
    // value may or may not be present
    try {
        parseValue(node, DECODING_ACCELERATED_KEY, decodingAccelerated_);
        parseValue(node, ENCODING_ACCELERATED_KEY, encodingAccelerated_);
    } catch (...) {
        decodingAccelerated_ = true;
        encodingAccelerated_ = false;
    }
#endif
    getVideoDeviceMonitor().unserialize(in);
}
#endif // ENABLE_VIDEO

} // namespace jami
