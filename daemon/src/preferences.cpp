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
#include "logger.h"
#include "audio/alsa/alsalayer.h"
#include "audio/pulseaudio/pulselayer.h"
#include "config/yamlemitter.h"
#include "config/yamlnode.h"
#include "hooks/urlhook.h"
#include "sip/sip_utils.h"
#include <sstream>
#include "global.h"
#include <cassert>

const char * const Preferences::DFT_ZONE = "North America";

namespace {
static const char * const DFT_PULSE_LENGTH_STR ="250";  /** Default DTMF lenght */
static const char * const ZRTP_ZIDFILE = "zidFile";     /** The filename used for storing ZIDs */
static const char * const ALSA_DFT_CARD	= "0";          /** Default sound card index */
static const char * const DFT_VOL_SPKR_STR = "100";     /** Default speaker volume */
static const char * const DFT_VOL_MICRO_STR	= "100";    /** Default mic volume */
} // end anonymous namespace

Preferences::Preferences() : accountOrder_("")
    , historyLimit_(30)
    , historyMaxCalls_(20)
    , notifyMails_(false)
    , zoneToneChoice_(DFT_ZONE) // DFT_ZONE
    , registrationExpire_(180)
    , portNum_(5060)
    , searchBarDisplay_(true)
    , zeroConfenable_(false)
    , md5Hash_(false)
{}

void Preferences::serialize(Conf::YamlEmitter *emiter)
{
    Conf::MappingNode preferencemap(NULL);

    Conf::ScalarNode order(accountOrder_);
    std::stringstream histlimitstr;
    histlimitstr << historyLimit_;
    Conf::ScalarNode historyLimit(histlimitstr.str());
    std::stringstream histmaxstr;
    histmaxstr << historyMaxCalls_;
    Conf::ScalarNode historyMaxCalls(histmaxstr.str());
    Conf::ScalarNode notifyMails(notifyMails_);
    Conf::ScalarNode zoneToneChoice(zoneToneChoice_);
    std::stringstream expirestr;
    expirestr << registrationExpire_;
    Conf::ScalarNode registrationExpire(expirestr.str());
    std::stringstream portstr;
    portstr << portNum_;
    Conf::ScalarNode portNum(portstr.str());
    Conf::ScalarNode searchBarDisplay(searchBarDisplay_);
    Conf::ScalarNode zeroConfenable(zeroConfenable_);
    Conf::ScalarNode md5Hash(md5Hash_);

    preferencemap.setKeyValue(ORDER_KEY, &order);
    preferencemap.setKeyValue(HISTORY_LIMIT_KEY, &historyLimit);
    preferencemap.setKeyValue(HISTORY_MAX_CALLS_KEY, &historyMaxCalls);
    preferencemap.setKeyValue(NOTIFY_MAILS_KEY, &notifyMails);
    preferencemap.setKeyValue(ZONE_TONE_CHOICE_KEY, &zoneToneChoice);
    preferencemap.setKeyValue(REGISTRATION_EXPIRE_KEY, &registrationExpire);
    preferencemap.setKeyValue(PORT_NUM_KEY, &portNum);
    preferencemap.setKeyValue(SEARCH_BAR_DISPLAY_KEY, &searchBarDisplay);
    preferencemap.setKeyValue(ZEROCONF_ENABLE_KEY, &zeroConfenable);
    preferencemap.setKeyValue(MD5_HASH_KEY, &md5Hash);

    emiter->serializePreference(&preferencemap);
}

void Preferences::unserialize(const Conf::MappingNode *map)
{
    if (map == NULL) {
        ERROR("Preference: Error: Preference map is NULL");
        return;
    }

    map->getValue(ORDER_KEY, &accountOrder_);
    map->getValue(HISTORY_LIMIT_KEY, &historyLimit_);
    map->getValue(HISTORY_MAX_CALLS_KEY, &historyMaxCalls_);
    map->getValue(NOTIFY_MAILS_KEY, &notifyMails_);
    map->getValue(ZONE_TONE_CHOICE_KEY, &zoneToneChoice_);
    map->getValue(REGISTRATION_EXPIRE_KEY, &registrationExpire_);
    map->getValue(PORT_NUM_KEY, &portNum_);
    map->getValue(SEARCH_BAR_DISPLAY_KEY, &searchBarDisplay_);
    map->getValue(ZEROCONF_ENABLE_KEY, &zeroConfenable_);
    map->getValue(MD5_HASH_KEY, &md5Hash_);
}

VoipPreference::VoipPreference() : playDtmf_(true)
    , playTones_(true)
    , pulseLength_(atoi(DFT_PULSE_LENGTH_STR))
    , symmetricRtp_(true)
    , zidFile_(ZRTP_ZIDFILE)
{}

void VoipPreference::serialize(Conf::YamlEmitter *emitter)
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

    emitter->serializeVoipPreference(&preferencemap);
}

void VoipPreference::unserialize(const Conf::MappingNode *map)
{
    if (!map) {
        ERROR("VoipPreference: Error: Preference map is NULL");
        return;
    }

    map->getValue(PLAY_DTMF_KEY, &playDtmf_);
    map->getValue(PLAY_TONES_KEY, &playTones_);
    map->getValue(PULSE_LENGTH_KEY, &pulseLength_);
    map->getValue(SYMMETRIC_RTP_KEY, &symmetricRtp_);
    map->getValue(ZID_FILE_KEY, &zidFile_);
}

AddressbookPreference::AddressbookPreference() : photo_(true)
    , enabled_(true)
    , list_("")
    , maxResults_(25)
    , business_(true)
    , home_(true)
    , mobile_(true)
{}

void AddressbookPreference::serialize(Conf::YamlEmitter *emitter)
{
    Conf::MappingNode preferencemap(NULL);

    Conf::ScalarNode photo(photo_);
    Conf::ScalarNode enabled(enabled_);
    Conf::ScalarNode list(list_);
    std::stringstream maxresultstr;
    maxresultstr << maxResults_;
    Conf::ScalarNode maxResults(maxresultstr.str());
    Conf::ScalarNode business(business_);
    Conf::ScalarNode home(home_);
    Conf::ScalarNode mobile(mobile_);

    preferencemap.setKeyValue(PHOTO_KEY, &photo);
    preferencemap.setKeyValue(ENABLED_KEY, &enabled);
    preferencemap.setKeyValue(LIST_KEY, &list);
    preferencemap.setKeyValue(MAX_RESULTS_KEY, &maxResults);
    preferencemap.setKeyValue(BUSINESS_KEY, &business);
    preferencemap.setKeyValue(HOME_KEY, &home);
    preferencemap.setKeyValue(MOBILE_KEY, &mobile);

    emitter->serializeAddressbookPreference(&preferencemap);

}

void AddressbookPreference::unserialize(const Conf::MappingNode *map)
{
    if (!map) {
        ERROR("Addressbook: Error: Preference map is NULL");
        return;
    }

    map->getValue(PHOTO_KEY, &photo_);
    map->getValue(ENABLED_KEY, &enabled_);
    map->getValue(LIST_KEY, &list_);
    map->getValue(MAX_RESULTS_KEY, &maxResults_);
    map->getValue(BUSINESS_KEY, &business_);
    map->getValue(HOME_KEY, &home_);
    map->getValue(MOBILE_KEY, &mobile_);
}

HookPreference::HookPreference() : iax2Enabled_(false)
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

void HookPreference::serialize(Conf::YamlEmitter *emitter)
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

    emitter->serializeHooksPreference(&preferencemap);
}

void HookPreference::unserialize(const Conf::MappingNode *map)
{
    if (!map) {
        ERROR("Hook: Error: Preference map is NULL");
        return;
    }

    map->getValue(IAX2_ENABLED_KEY, &iax2Enabled_);
    map->getValue(NUMBER_ADD_PREFIX_KEY, &numberAddPrefix_);
    map->getValue(NUMBER_ENABLED_KEY, &numberEnabled_);
    map->getValue(SIP_ENABLED_KEY, &sipEnabled_);
    map->getValue(URL_COMMAND_KEY, &urlCommand_);
    map->getValue(URL_SIP_FIELD_KEY, &urlSipField_);
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
    , cardin_(atoi(ALSA_DFT_CARD)) // ALSA_DFT_CARD
    , cardout_(atoi(ALSA_DFT_CARD)) // ALSA_DFT_CARD
    , cardring_(atoi(ALSA_DFT_CARD)) // ALSA_DFT_CARD
    , plugin_("default") // PCM_DEFAULT
    , smplrate_(44100) // DFT_SAMPLE_RATE
    , devicePlayback_("")
    , deviceRecord_("")
    , deviceRingtone_("")
    , recordpath_("") // DFT_RECORD_PATH
    , alwaysRecording_(false)
    , volumemic_(atoi(DFT_VOL_SPKR_STR)) // DFT_VOL_SPKR_STR
    , volumespkr_(atoi(DFT_VOL_MICRO_STR)) // DFT_VOL_MICRO_STR
    , noisereduce_(true)
    , echocancel_(false)
    , echoCancelTailLength_(100)
    , echoCancelDelay_(0)
{}

namespace {
void checkSoundCard(int &card, int stream)
{
    if (not AlsaLayer::soundCardIndexExists(card, stream)) {
        WARN(" Card with index %d doesn't exist or is unusable.", card);
        card = ALSA_DFT_CARD_ID;
    }
}
}

AudioLayer* AudioPreference::createAudioLayer()
{
    if (audioApi_ == PULSEAUDIO_API_STR and system("ps -C pulseaudio > /dev/null") == 0)
        return new PulseLayer;
    else {
        audioApi_ = ALSA_API_STR;
        checkSoundCard(cardin_, SFL_PCM_CAPTURE);
        checkSoundCard(cardout_, SFL_PCM_PLAYBACK);
        checkSoundCard(cardring_, SFL_PCM_RINGTONE);
        return new AlsaLayer;
    }
}

AudioLayer* AudioPreference::switchAndCreateAudioLayer()
{
    if (audioApi_ == PULSEAUDIO_API_STR)
        audioApi_ = ALSA_API_STR;
    else
        audioApi_ = PULSEAUDIO_API_STR;

    return createAudioLayer();
}

void AudioPreference::serialize(Conf::YamlEmitter *emitter)
{
    Conf::MappingNode preferencemap(NULL);
    Conf::MappingNode alsapreferencemap(NULL);
    Conf::MappingNode pulsepreferencemap(NULL);

    // alsa preference
    std::stringstream instr;
    instr << cardin_;
    Conf::ScalarNode cardin(instr.str());
    std::stringstream outstr;
    outstr << cardout_;
    Conf::ScalarNode cardout(outstr.str());
    std::stringstream ringstr;
    ringstr << cardring_;
    Conf::ScalarNode cardring(ringstr.str());
    Conf::ScalarNode plugin(plugin_);

    std::stringstream ratestr;
    ratestr << smplrate_;
    Conf::ScalarNode smplrate(ratestr.str());

    //pulseaudio preference
    Conf::ScalarNode devicePlayback(devicePlayback_);
    Conf::ScalarNode deviceRecord(deviceRecord_);
    Conf::ScalarNode deviceRingtone(deviceRingtone_);

    // general preference
    Conf::ScalarNode audioapi(audioApi_);
    Conf::ScalarNode recordpath(recordpath_); //: /home/msavard/Bureau
    Conf::ScalarNode alwaysRecording(alwaysRecording_);
    std::stringstream micstr;
    micstr << volumemic_;
    Conf::ScalarNode volumemic(micstr.str()); //:  100
    std::stringstream spkrstr;
    spkrstr << volumespkr_;
    Conf::ScalarNode volumespkr(spkrstr.str()); //: 100
    Conf::ScalarNode noise(noisereduce_);
    Conf::ScalarNode echo(echocancel_);
    std::stringstream tailstr;
    tailstr << echoCancelTailLength_;
    Conf::ScalarNode echotail(tailstr.str());
    std::stringstream delaystr;
    delaystr << echoCancelDelay_;
    Conf::ScalarNode echodelay(delaystr.str());

    preferencemap.setKeyValue(AUDIO_API_KEY, &audioapi);
    preferencemap.setKeyValue(RECORDPATH_KEY, &recordpath);
    preferencemap.setKeyValue(ALWAYS_RECORDING_KEY, &alwaysRecording);
    preferencemap.setKeyValue(VOLUMEMIC_KEY, &volumemic);
    preferencemap.setKeyValue(VOLUMESPKR_KEY, &volumespkr);

    preferencemap.setKeyValue(ALSAMAP_KEY, &alsapreferencemap);
    alsapreferencemap.setKeyValue(CARDIN_KEY, &cardin);
    alsapreferencemap.setKeyValue(CARDOUT_KEY, &cardout);
    alsapreferencemap.setKeyValue(CARDRING_KEY, &cardring);
    alsapreferencemap.setKeyValue(PLUGIN_KEY, &plugin);
    alsapreferencemap.setKeyValue(SMPLRATE_KEY, &smplrate);

    preferencemap.setKeyValue(PULSEMAP_KEY, &pulsepreferencemap);
    pulsepreferencemap.setKeyValue(DEVICE_PLAYBACK_KEY, &devicePlayback);
    pulsepreferencemap.setKeyValue(DEVICE_RECORD_KEY, &deviceRecord);
    pulsepreferencemap.setKeyValue(DEVICE_RINGTONE_KEY, &deviceRingtone);

    preferencemap.setKeyValue(NOISE_REDUCE_KEY, &noise);
    preferencemap.setKeyValue(ECHO_CANCEL_KEY, &echo);
    preferencemap.setKeyValue(ECHO_TAIL_KEY, &echotail);
    preferencemap.setKeyValue(ECHO_DELAY_KEY, &echodelay);

    emitter->serializeAudioPreference(&preferencemap);
}

void AudioPreference::unserialize(const Conf::MappingNode *map)
{
    assert(map);

    map->getValue(AUDIO_API_KEY, &audioApi_);
    map->getValue(RECORDPATH_KEY, &recordpath_);
    map->getValue(ALWAYS_RECORDING_KEY, &alwaysRecording_);
    map->getValue(VOLUMEMIC_KEY, &volumemic_);
    map->getValue(VOLUMESPKR_KEY, &volumespkr_);
    map->getValue(NOISE_REDUCE_KEY, &noisereduce_);
    map->getValue(ECHO_CANCEL_KEY, &echocancel_);

    Conf::MappingNode *alsamap =(Conf::MappingNode *)(map->getValue("alsa"));

    if (alsamap) {
        alsamap->getValue(CARDIN_KEY, &cardin_);
        alsamap->getValue(CARDOUT_KEY, &cardout_);
        alsamap->getValue(CARDRING_KEY, &cardring_);
        alsamap->getValue(SMPLRATE_KEY, &smplrate_);
        alsamap->getValue(PLUGIN_KEY, &plugin_);
    }

    Conf::MappingNode *pulsemap =(Conf::MappingNode *)(map->getValue("pulse"));

    if (pulsemap) {
        pulsemap->getValue(DEVICE_PLAYBACK_KEY, &devicePlayback_);
        pulsemap->getValue(DEVICE_RECORD_KEY, &deviceRecord_);
        pulsemap->getValue(DEVICE_RINGTONE_KEY, &deviceRingtone_);
    }
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


void ShortcutPreferences::serialize(Conf::YamlEmitter *emitter)
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

    emitter->serializeShortcutPreference(&preferencemap);
}

void ShortcutPreferences::unserialize(const Conf::MappingNode *map)
{
    if (map == NULL) {
        ERROR("ShortcutPreference: Error: Preference map is NULL");
        return;
    }

    map->getValue(HANGUP_SHORT_KEY, &hangup_);
    map->getValue(PICKUP_SHORT_KEY, &pickup_);
    map->getValue(POPUP_SHORT_KEY, &popup_);
    map->getValue(TOGGLE_HOLD_SHORT_KEY, &toggleHold_);
    map->getValue(TOGGLE_PICKUP_HANGUP_SHORT_KEY, &togglePickupHangup_);
}

