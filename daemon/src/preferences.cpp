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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "preferences.h"
#include "logger.h"
#include "audio/audiolayer.h"
#ifdef __ANDROID__
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
#endif
#include "config/yamlemitter.h"
#include "config/yamlnode.h"
#include "hooks/urlhook.h"
#include "sip/sip_utils.h"
#include <sstream>
#include <algorithm>
#include "global.h"
#include "fileutils.h"

const char * const Preferences::DFT_ZONE = "North America";
const char * const Preferences::REGISTRATION_EXPIRE_KEY = "registrationexpire";

// general preferences
static const char * const ORDER_KEY = "order";
static const char * const AUDIO_API_KEY = "audioApi";
static const char * const HISTORY_LIMIT_KEY = "historyLimit";
static const char * const HISTORY_MAX_CALLS_KEY = "historyMaxCalls";
static const char * const ZONE_TONE_CHOICE_KEY = "zoneToneChoice";
static const char * const PORT_NUM_KEY = "portNum";
static const char * const SEARCH_BAR_DISPLAY_KEY = "searchBarDisplay";
static const char * const MD5_HASH_KEY = "md5Hash";

// voip preferences
static const char * const PLAY_DTMF_KEY = "playDtmf";
static const char * const PLAY_TONES_KEY = "playTones";
static const char * const PULSE_LENGTH_KEY = "pulseLength";
static const char * const SYMMETRIC_RTP_KEY = "symmetric";
static const char * const ZID_FILE_KEY = "zidFile";

// hooks preferences
static const char * const IAX2_ENABLED_KEY = "iax2Enabled";
static const char * const NUMBER_ADD_PREFIX_KEY = "numberAddPrefix";
static const char * const NUMBER_ENABLED_KEY = "numberEnabled";
static const char * const SIP_ENABLED_KEY = "sipEnabled";
static const char * const URL_COMMAND_KEY = "urlCommand";
static const char * const URL_SIP_FIELD_KEY = "urlSipField";

// audio preferences
#if HAVE_ALSA
static const char * const ALSAMAP_KEY = "alsa";
#endif
#if HAVE_PULSE
static const char * const PULSEMAP_KEY = "pulse";
#endif
static const char * const CARDIN_KEY = "cardIn";
static const char * const CARDOUT_KEY = "cardOut";
static const char * const CARDRING_KEY = "cardRing";
static const char * const PLUGIN_KEY = "plugin";
static const char * const SMPLRATE_KEY = "smplRate";
static const char * const DEVICE_PLAYBACK_KEY = "devicePlayback";
static const char * const DEVICE_RECORD_KEY = "deviceRecord";
static const char * const DEVICE_RINGTONE_KEY = "deviceRingtone";
static const char * const RECORDPATH_KEY = "recordPath";
static const char * const ALWAYS_RECORDING_KEY = "alwaysRecording";
static const char * const VOLUMEMIC_KEY = "volumeMic";
static const char * const VOLUMESPKR_KEY = "volumeSpkr";
static const char * const NOISE_REDUCE_KEY = "noiseReduce";
static const char * const AGC_KEY = "automaticGainControl";
static const char * const CAPTURE_MUTED_KEY = "captureMuted";
static const char * const PLAYBACK_MUTED_KEY = "playbackMuted";

// shortcut preferences
static const char * const HANGUP_SHORT_KEY = "hangUp";
static const char * const PICKUP_SHORT_KEY = "pickUp";
static const char * const POPUP_SHORT_KEY = "popupWindow";
static const char * const TOGGLE_HOLD_SHORT_KEY = "toggleHold";
static const char * const TOGGLE_PICKUP_HANGUP_SHORT_KEY = "togglePickupHangup";

static const char * const DFT_PULSE_LENGTH_STR = "250"; /** Default DTMF lenght */
static const char * const ZRTP_ZIDFILE = "zidFile";     /** The filename used for storing ZIDs */
static const char * const ALSA_DFT_CARD    = "0";          /** Default sound card index */

Preferences::Preferences() :
    accountOrder_("")
    , historyLimit_(30)
    , historyMaxCalls_(20)
    , zoneToneChoice_(DFT_ZONE) // DFT_ZONE
    , registrationExpire_(180)
    , portNum_(5060)
    , searchBarDisplay_(true)
    , md5Hash_(false)
{}

void Preferences::verifyAccountOrder(const std::vector<std::string> &accountIDs)
{
    using namespace std;

    vector<string> tokens;
    string token;
    bool drop = false;

    for (const auto c : accountOrder_) {
        if (c != '/') {
            token += c;
        } else {
            if (find(accountIDs.begin(), accountIDs.end(), token) != accountIDs.end())
                tokens.push_back(token);
            else {
                DEBUG("Dropping nonexistent account %s", token.c_str());
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

void Preferences::serialize(Conf::YamlEmitter &emiter)
{
    Conf::MappingNode preferencemap(NULL);

    Conf::ScalarNode order(accountOrder_);
    std::stringstream histlimitstr;
    histlimitstr << historyLimit_;
    Conf::ScalarNode historyLimit(histlimitstr.str());
    std::stringstream histmaxstr;
    histmaxstr << historyMaxCalls_;
    Conf::ScalarNode historyMaxCalls(histmaxstr.str());
    Conf::ScalarNode zoneToneChoice(zoneToneChoice_);
    std::stringstream expirestr;
    expirestr << registrationExpire_;
    Conf::ScalarNode registrationExpire(expirestr.str());
    std::stringstream portstr;
    portstr << portNum_;
    Conf::ScalarNode portNum(portstr.str());
    Conf::ScalarNode searchBarDisplay(searchBarDisplay_);
    Conf::ScalarNode md5Hash(md5Hash_);

    preferencemap.setKeyValue(ORDER_KEY, &order);
    preferencemap.setKeyValue(HISTORY_LIMIT_KEY, &historyLimit);
    preferencemap.setKeyValue(HISTORY_MAX_CALLS_KEY, &historyMaxCalls);
    preferencemap.setKeyValue(ZONE_TONE_CHOICE_KEY, &zoneToneChoice);
    preferencemap.setKeyValue(REGISTRATION_EXPIRE_KEY, &registrationExpire);
    preferencemap.setKeyValue(PORT_NUM_KEY, &portNum);
    preferencemap.setKeyValue(SEARCH_BAR_DISPLAY_KEY, &searchBarDisplay);
    preferencemap.setKeyValue(MD5_HASH_KEY, &md5Hash);

    emiter.serializePreference(&preferencemap, "preferences");
}

void Preferences::unserialize(const Conf::YamlNode &map)
{
    map.getValue(ORDER_KEY, &accountOrder_);
    map.getValue(HISTORY_LIMIT_KEY, &historyLimit_);
    map.getValue(HISTORY_MAX_CALLS_KEY, &historyMaxCalls_);
    map.getValue(ZONE_TONE_CHOICE_KEY, &zoneToneChoice_);
    map.getValue(REGISTRATION_EXPIRE_KEY, &registrationExpire_);
    map.getValue(PORT_NUM_KEY, &portNum_);
    map.getValue(SEARCH_BAR_DISPLAY_KEY, &searchBarDisplay_);
    map.getValue(MD5_HASH_KEY, &md5Hash_);
}

VoipPreference::VoipPreference() :
    playDtmf_(true)
    , playTones_(true)
    , pulseLength_(atoi(DFT_PULSE_LENGTH_STR))
    , symmetricRtp_(true)
    , zidFile_(ZRTP_ZIDFILE)
{}

void VoipPreference::serialize(Conf::YamlEmitter &emitter)
{
    Conf::MappingNode preferencemap(NULL);

    Conf::ScalarNode playDtmf(playDtmf_);
    Conf::ScalarNode playTones(playTones_);
    std::stringstream pulselengthstr;
    pulselengthstr << pulseLength_;
    Conf::ScalarNode pulseLength(pulselengthstr.str());
    Conf::ScalarNode symmetricRtp(symmetricRtp_);
    Conf::ScalarNode zidFile(zidFile_.c_str());

    preferencemap.setKeyValue(PLAY_DTMF_KEY, &playDtmf);
    preferencemap.setKeyValue(PLAY_TONES_KEY, &playTones);
    preferencemap.setKeyValue(PULSE_LENGTH_KEY, &pulseLength);
    preferencemap.setKeyValue(SYMMETRIC_RTP_KEY, &symmetricRtp);
    preferencemap.setKeyValue(ZID_FILE_KEY, &zidFile);

    emitter.serializePreference(&preferencemap, "voipPreferences");
}

void VoipPreference::unserialize(const Conf::YamlNode &map)
{
    map.getValue(PLAY_DTMF_KEY, &playDtmf_);
    map.getValue(PLAY_TONES_KEY, &playTones_);
    map.getValue(PULSE_LENGTH_KEY, &pulseLength_);
    map.getValue(SYMMETRIC_RTP_KEY, &symmetricRtp_);
    map.getValue(ZID_FILE_KEY, &zidFile_);
}

HookPreference::HookPreference() :
    iax2Enabled_(false)
    , numberAddPrefix_("")
    , numberEnabled_(false)
    , sipEnabled_(false)
    , urlCommand_("x-www-browser")
    , urlSipField_("X-sflphone-url")
{}

HookPreference::HookPreference(const std::map<std::string, std::string> &settings) :
    iax2Enabled_(settings.find("URLHOOK_IAX2_ENABLED")->second == "true")
    , numberAddPrefix_(settings.find("PHONE_NUMBER_HOOK_ADD_PREFIX")->second)
    , numberEnabled_(settings.find("PHONE_NUMBER_HOOK_ENABLED")->second == "true")
    , sipEnabled_(settings.find("URLHOOK_SIP_ENABLED")->second == "true")
    , urlCommand_(settings.find("URLHOOK_COMMAND")->second)
    , urlSipField_(settings.find("URLHOOK_SIP_FIELD")->second)
{}

std::map<std::string, std::string> HookPreference::toMap() const
{
    std::map<std::string, std::string> settings;
    settings["URLHOOK_IAX2_ENABLED"] = iax2Enabled_ ? "true" : "false";
    settings["PHONE_NUMBER_HOOK_ADD_PREFIX"] = numberAddPrefix_;
    settings["PHONE_NUMBER_HOOK_ENABLED"] = numberEnabled_ ? "true" : "false";
    settings["URLHOOK_SIP_ENABLED"] = sipEnabled_ ? "true" : "false";
    settings["URLHOOK_COMMAND"] = urlCommand_;
    settings["URLHOOK_SIP_FIELD"] = urlSipField_;

    return settings;
}

void HookPreference::serialize(Conf::YamlEmitter &emitter)
{
    Conf::MappingNode preferencemap(NULL);

    Conf::ScalarNode iax2Enabled(iax2Enabled_);
    Conf::ScalarNode numberAddPrefix(numberAddPrefix_);
    Conf::ScalarNode numberEnabled(numberEnabled_);
    Conf::ScalarNode sipEnabled(sipEnabled_);
    Conf::ScalarNode urlCommand(urlCommand_);
    Conf::ScalarNode urlSipField(urlSipField_);

    preferencemap.setKeyValue(IAX2_ENABLED_KEY, &iax2Enabled);
    preferencemap.setKeyValue(NUMBER_ADD_PREFIX_KEY, &numberAddPrefix);
    preferencemap.setKeyValue(NUMBER_ENABLED_KEY, &numberEnabled);
    preferencemap.setKeyValue(SIP_ENABLED_KEY, &sipEnabled);
    preferencemap.setKeyValue(URL_COMMAND_KEY, &urlCommand);
    preferencemap.setKeyValue(URL_SIP_FIELD_KEY, &urlSipField);

    emitter.serializePreference(&preferencemap, "hooks");
}

void HookPreference::unserialize(const Conf::YamlNode &map)
{
    map.getValue(IAX2_ENABLED_KEY, &iax2Enabled_);
    map.getValue(NUMBER_ADD_PREFIX_KEY, &numberAddPrefix_);
    map.getValue(NUMBER_ENABLED_KEY, &numberEnabled_);
    map.getValue(SIP_ENABLED_KEY, &sipEnabled_);
    map.getValue(URL_COMMAND_KEY, &urlCommand_);
    map.getValue(URL_SIP_FIELD_KEY, &urlSipField_);
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
static void
checkSoundCard(int &card, DeviceType type)
{
    if (not AlsaLayer::soundCardIndexExists(card, type)) {
        WARN(" Card with index %d doesn't exist or is unusable.", card);
        card = ALSA_DFT_CARD_ID;
    }
}
#endif

#ifdef __ANDROID__
AudioLayer* AudioPreference::createAudioLayer()
{
    return new OpenSLLayer(*this);
}
#else

AudioLayer* AudioPreference::createAudioLayer()
{
#if HAVE_JACK
    if (audioApi_ == JACK_API_STR) {
        if (system("jack_lsp > /dev/null") == 0) {
            try {
                return new JackLayer(*this);
            } catch (const std::runtime_error &e) {
                ERROR("%s", e.what());
#if HAVE_PULSE
                WARN("falling back to pulseaudio");
                audioApi_ = PULSEAUDIO_API_STR;
#elif HAVE_ALSA
                audioApi_ = ALSA_API_STR;
#else
                throw;
#endif
            }
        }
    }
#endif

#if HAVE_PULSE

    if (audioApi_ == PULSEAUDIO_API_STR) {
        try {
            return new PulseLayer(*this);
        } catch (const std::runtime_error &e) {
            WARN("Could not create pulseaudio layer, falling back to ALSA");
        }
    }

#endif

#if HAVE_ALSA

    audioApi_ = ALSA_API_STR;
    checkSoundCard(alsaCardin_, DeviceType::CAPTURE);
    checkSoundCard(alsaCardout_, DeviceType::PLAYBACK);
    checkSoundCard(alsaCardring_, DeviceType::RINGTONE);

    return new AlsaLayer(*this);
#else
    return NULL;
#endif
}

#endif // __ANDROID__

void AudioPreference::serialize(Conf::YamlEmitter &emitter)
{
    // alsa preference
    std::stringstream instr;
    instr << alsaCardin_;
    Conf::ScalarNode cardin(instr.str());
    std::stringstream outstr;
    outstr << alsaCardout_;
    Conf::ScalarNode cardout(outstr.str());
    std::stringstream ringstr;
    ringstr << alsaCardring_;
    Conf::ScalarNode cardring(ringstr.str());
    Conf::ScalarNode plugin(alsaPlugin_);

    std::stringstream ratestr;
    ratestr << alsaSmplrate_;
    Conf::ScalarNode alsaSmplrate(ratestr.str());

    //pulseaudio preference
    Conf::ScalarNode pulseDevicePlayback(pulseDevicePlayback_);
    Conf::ScalarNode pulseDeviceRecord(pulseDeviceRecord_);
    Conf::ScalarNode pulseDeviceRingtone(pulseDeviceRingtone_);

    // general preference
    Conf::ScalarNode audioapi(audioApi_);
    Conf::ScalarNode recordpath(recordpath_); //: /home/msavard/Bureau
    Conf::ScalarNode alwaysRecording(alwaysRecording_);
    std::ostringstream micstr;
    micstr << volumemic_;
    Conf::ScalarNode volumemic(micstr.str()); //:  100
    std::ostringstream spkrstr;
    spkrstr << volumespkr_;
    Conf::ScalarNode volumespkr(spkrstr.str()); //: 100
    Conf::ScalarNode denoise(denoise_);
    Conf::ScalarNode agc(agcEnabled_);
    Conf::ScalarNode captureMuted(captureMuted_);
    Conf::ScalarNode playbackMuted(playbackMuted_);

    Conf::MappingNode preferencemap(NULL);
    preferencemap.setKeyValue(AUDIO_API_KEY, &audioapi);
    preferencemap.setKeyValue(RECORDPATH_KEY, &recordpath);
    preferencemap.setKeyValue(ALWAYS_RECORDING_KEY, &alwaysRecording);
    preferencemap.setKeyValue(VOLUMEMIC_KEY, &volumemic);
    preferencemap.setKeyValue(VOLUMESPKR_KEY, &volumespkr);
    preferencemap.setKeyValue(CAPTURE_MUTED_KEY, &captureMuted);
    preferencemap.setKeyValue(PLAYBACK_MUTED_KEY, &playbackMuted);

    Conf::MappingNode alsapreferencemap(NULL);
#if HAVE_ALSA
    preferencemap.setKeyValue(ALSAMAP_KEY, &alsapreferencemap);
    alsapreferencemap.setKeyValue(CARDIN_KEY, &cardin);
    alsapreferencemap.setKeyValue(CARDOUT_KEY, &cardout);
    alsapreferencemap.setKeyValue(CARDRING_KEY, &cardring);
    alsapreferencemap.setKeyValue(PLUGIN_KEY, &plugin);
    alsapreferencemap.setKeyValue(SMPLRATE_KEY, &alsaSmplrate);
#endif

#if HAVE_PULSE
    Conf::MappingNode pulsepreferencemap(NULL);
    preferencemap.setKeyValue(PULSEMAP_KEY, &pulsepreferencemap);
    pulsepreferencemap.setKeyValue(DEVICE_PLAYBACK_KEY, &pulseDevicePlayback);
    pulsepreferencemap.setKeyValue(DEVICE_RECORD_KEY, &pulseDeviceRecord);
    pulsepreferencemap.setKeyValue(DEVICE_RINGTONE_KEY, &pulseDeviceRingtone);
#endif

    preferencemap.setKeyValue(NOISE_REDUCE_KEY, &denoise);
    preferencemap.setKeyValue(AGC_KEY, &agc);
    emitter.serializePreference(&preferencemap, "audio");
}

bool
AudioPreference::setRecordPath(const std::string &r)
{
    std::string path = fileutils::expand_path(r);
    if (fileutils::isDirectoryWritable(path)) {
        recordpath_ = path;
        return true;
    } else {
        ERROR("%s is not writable, cannot be the recording path", path.c_str());
        return false;
    }
}

void AudioPreference::unserialize(const Conf::YamlNode &map)
{
    map.getValue(AUDIO_API_KEY, &audioApi_);
    std::string tmpRecordPath;
    map.getValue(RECORDPATH_KEY, &tmpRecordPath);

    if (not setRecordPath(tmpRecordPath))
        setRecordPath(fileutils::get_home_dir());

    map.getValue(ALWAYS_RECORDING_KEY, &alwaysRecording_);
    map.getValue(VOLUMEMIC_KEY, &volumemic_);

    const auto clamp = [] (double min, double max, double val) {
        return std::min(max, std::max(min, val));
    };

    volumemic_ = clamp(-1.0, 1.0, volumemic_);
    map.getValue(VOLUMESPKR_KEY, &volumespkr_);
    volumespkr_ = clamp(-1.0, 1.0, volumespkr_);
    map.getValue(NOISE_REDUCE_KEY, &denoise_);
    map.getValue(AGC_KEY, &agcEnabled_);
    map.getValue(CAPTURE_MUTED_KEY, &captureMuted_);
    map.getValue(PLAYBACK_MUTED_KEY, &playbackMuted_);

    Conf::MappingNode *alsamap = (Conf::MappingNode *) map.getValue("alsa");

    if (alsamap) {
        alsamap->getValue(CARDIN_KEY, &alsaCardin_);
        alsamap->getValue(CARDOUT_KEY, &alsaCardout_);
        alsamap->getValue(CARDRING_KEY, &alsaCardring_);
        alsamap->getValue(SMPLRATE_KEY, &alsaSmplrate_);
        alsamap->getValue(PLUGIN_KEY, &alsaPlugin_);
    }

#if HAVE_PULSE
    Conf::MappingNode *pulsemap = (Conf::MappingNode *)(map.getValue("pulse"));

    if (pulsemap) {
        pulsemap->getValue(DEVICE_PLAYBACK_KEY, &pulseDevicePlayback_);
        pulsemap->getValue(DEVICE_RECORD_KEY, &pulseDeviceRecord_);
        pulsemap->getValue(DEVICE_RINGTONE_KEY, &pulseDeviceRingtone_);
    }

#endif
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


void ShortcutPreferences::serialize(Conf::YamlEmitter &emitter)
{
    Conf::MappingNode preferencemap(NULL);

    Conf::ScalarNode hangup(hangup_);
    Conf::ScalarNode pickup(pickup_);
    Conf::ScalarNode popup(popup_);
    Conf::ScalarNode toggleHold(toggleHold_);
    Conf::ScalarNode togglePickupHangup(togglePickupHangup_);

    preferencemap.setKeyValue(HANGUP_SHORT_KEY, &hangup);
    preferencemap.setKeyValue(PICKUP_SHORT_KEY, &pickup);
    preferencemap.setKeyValue(POPUP_SHORT_KEY, &popup);
    preferencemap.setKeyValue(TOGGLE_HOLD_SHORT_KEY, &toggleHold);
    preferencemap.setKeyValue(TOGGLE_PICKUP_HANGUP_SHORT_KEY, &togglePickupHangup);

    emitter.serializePreference(&preferencemap, "shortcuts");
}

void ShortcutPreferences::unserialize(const Conf::YamlNode &map)
{
    map.getValue(HANGUP_SHORT_KEY, &hangup_);
    map.getValue(PICKUP_SHORT_KEY, &pickup_);
    map.getValue(POPUP_SHORT_KEY, &popup_);
    map.getValue(TOGGLE_HOLD_SHORT_KEY, &toggleHold_);
    map.getValue(TOGGLE_PICKUP_HANGUP_SHORT_KEY, &togglePickupHangup_);
}

