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
#include "audio/alsa/alsalayer.h"
#include "audio/pulseaudio/pulselayer.h"
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

    preferencemap.setKeyValue(orderKey, &order);
    preferencemap.setKeyValue(historyLimitKey, &historyLimit);
    preferencemap.setKeyValue(historyMaxCallsKey, &historyMaxCalls);
    preferencemap.setKeyValue(notifyMailsKey, &notifyMails);
    preferencemap.setKeyValue(zoneToneChoiceKey, &zoneToneChoice);
    preferencemap.setKeyValue(registrationExpireKey, &registrationExpire);
    preferencemap.setKeyValue(portNumKey, &portNum);
    preferencemap.setKeyValue(searchBarDisplayKey, &searchBarDisplay);
    preferencemap.setKeyValue(zeroConfenableKey, &zeroConfenable);
    preferencemap.setKeyValue(md5HashKey, &md5Hash);

    emiter->serializePreference(&preferencemap);
}

void Preferences::unserialize(Conf::MappingNode *map)
{
    if (map == NULL) {
        ERROR("Preference: Error: Preference map is NULL");
        return;
    }

    map->getValue(orderKey, &accountOrder_);
    map->getValue(historyLimitKey, &historyLimit_);
    map->getValue(historyMaxCallsKey, &historyMaxCalls_);
    map->getValue(notifyMailsKey, &notifyMails_);
    map->getValue(zoneToneChoiceKey, &zoneToneChoice_);
    map->getValue(registrationExpireKey, &registrationExpire_);
    map->getValue(portNumKey, &portNum_);
    map->getValue(searchBarDisplayKey, &searchBarDisplay_);
    map->getValue(zeroConfenableKey, &zeroConfenable_);
    map->getValue(md5HashKey, &md5Hash_);
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

    preferencemap.setKeyValue(playDtmfKey, &playDtmf);
    preferencemap.setKeyValue(playTonesKey, &playTones);
    preferencemap.setKeyValue(pulseLengthKey, &pulseLength);
    preferencemap.setKeyValue(symmetricRtpKey, &symmetricRtp);
    preferencemap.setKeyValue(zidFileKey, &zidFile);

    emitter->serializeVoipPreference(&preferencemap);
}

void VoipPreference::unserialize(Conf::MappingNode *map)
{
    if (!map) {
        ERROR("VoipPreference: Error: Preference map is NULL");
        return;
    }

    map->getValue(playDtmfKey, &playDtmf_);
    map->getValue(playTonesKey, &playTones_);
    map->getValue(pulseLengthKey, &pulseLength_);
    map->getValue(symmetricRtpKey, &symmetricRtp_);
    map->getValue(zidFileKey, &zidFile_);
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

    preferencemap.setKeyValue(photoKey, &photo);
    preferencemap.setKeyValue(enabledKey, &enabled);
    preferencemap.setKeyValue(listKey, &list);
    preferencemap.setKeyValue(maxResultsKey, &maxResults);
    preferencemap.setKeyValue(businessKey, &business);
    preferencemap.setKeyValue(homeKey, &home);
    preferencemap.setKeyValue(mobileKey, &mobile);

    emitter->serializeAddressbookPreference(&preferencemap);

}

void AddressbookPreference::unserialize(Conf::MappingNode *map)
{
    if (!map) {
        ERROR("Addressbook: Error: Preference map is NULL");
        return;
    }

    map->getValue(photoKey, &photo_);
    map->getValue(enabledKey, &enabled_);
    map->getValue(listKey, &list_);
    map->getValue(maxResultsKey, &maxResults_);
    map->getValue(businessKey, &business_);
    map->getValue(homeKey, &home_);
    map->getValue(mobileKey, &mobile_);
}

HookPreference::HookPreference() : iax2Enabled_(false)
    , numberAddPrefix_("")
    , numberEnabled_(false)
    , sipEnabled_(false)
    , urlCommand_("x-www-browser")
    , urlSipField_("X-sflphone-url")
{}

void HookPreference::serialize(Conf::YamlEmitter *emitter)
{
    Conf::MappingNode preferencemap(NULL);

    Conf::ScalarNode iax2Enabled(iax2Enabled_);
    Conf::ScalarNode numberAddPrefix(numberAddPrefix_);
    Conf::ScalarNode numberEnabled(numberEnabled_);
    Conf::ScalarNode sipEnabled(sipEnabled_);
    Conf::ScalarNode urlCommand(urlCommand_);
    Conf::ScalarNode urlSipField(urlSipField_);

    preferencemap.setKeyValue(iax2EnabledKey, &iax2Enabled);
    preferencemap.setKeyValue(numberAddPrefixKey, &numberAddPrefix);
    preferencemap.setKeyValue(numberEnabledKey, &numberEnabled);
    preferencemap.setKeyValue(sipEnabledKey, &sipEnabled);
    preferencemap.setKeyValue(urlCommandKey, &urlCommand);
    preferencemap.setKeyValue(urlSipFieldKey, &urlSipField);

    emitter->serializeHooksPreference(&preferencemap);
}

void HookPreference::unserialize(Conf::MappingNode *map)
{
    if (!map) {
        ERROR("Hook: Error: Preference map is NULL");
        return;
    }

    map->getValue(iax2EnabledKey, &iax2Enabled_);
    map->getValue(numberAddPrefixKey, &numberAddPrefix_);
    map->getValue(numberEnabledKey, &numberEnabled_);
    map->getValue(sipEnabledKey, &sipEnabled_);
    map->getValue(urlCommandKey, &urlCommand_);
    map->getValue(urlSipFieldKey, &urlSipField_);
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
    DEBUG("************************************************** serialize echotail %d", echoCancelTailLength_);
    tailstr << echoCancelTailLength_;
    Conf::ScalarNode echotail(tailstr.str());
    std::stringstream delaystr;
    DEBUG("************************************************** serialize echodelay %d", echoCancelTailLength_);
    delaystr << echoCancelDelay_;
    Conf::ScalarNode echodelay(delaystr.str());

    preferencemap.setKeyValue(audioApiKey, &audioapi);
    preferencemap.setKeyValue(recordpathKey, &recordpath);
    preferencemap.setKeyValue(alwaysRecordingKey, &alwaysRecording);
    preferencemap.setKeyValue(volumemicKey, &volumemic);
    preferencemap.setKeyValue(volumespkrKey, &volumespkr);

    preferencemap.setKeyValue(alsamapKey, &alsapreferencemap);
    alsapreferencemap.setKeyValue(cardinKey, &cardin);
    alsapreferencemap.setKeyValue(cardoutKey, &cardout);
    alsapreferencemap.setKeyValue(cardringKey, &cardring);
    alsapreferencemap.setKeyValue(pluginKey, &plugin);
    alsapreferencemap.setKeyValue(smplrateKey, &smplrate);

    preferencemap.setKeyValue(pulsemapKey, &pulsepreferencemap);
    pulsepreferencemap.setKeyValue(devicePlaybackKey, &devicePlayback);
    pulsepreferencemap.setKeyValue(deviceRecordKey, &deviceRecord);
    pulsepreferencemap.setKeyValue(deviceRingtoneKey, &deviceRingtone);

    preferencemap.setKeyValue(noiseReduceKey, &noise);
    preferencemap.setKeyValue(echoCancelKey, &echo);
    preferencemap.setKeyValue(echoTailKey, &echotail);
    preferencemap.setKeyValue(echoDelayKey, &echodelay);

    emitter->serializeAudioPreference(&preferencemap);
}

void AudioPreference::unserialize(Conf::MappingNode *map)
{
    assert(map);

    map->getValue(audioApiKey, &audioApi_);
    map->getValue(recordpathKey, &recordpath_);
    map->getValue(alwaysRecordingKey, &alwaysRecording_);
    map->getValue(volumemicKey, &volumemic_);
    map->getValue(volumespkrKey, &volumespkr_);
    map->getValue(noiseReduceKey, &noisereduce_);
    map->getValue(echoCancelKey, &echocancel_);

    Conf::MappingNode *alsamap =(Conf::MappingNode *)(map->getValue("alsa"));

    if (alsamap) {
        alsamap->getValue(cardinKey, &cardin_);
        alsamap->getValue(cardoutKey, &cardout_);
        alsamap->getValue(cardringKey, &cardring_);
        alsamap->getValue(smplrateKey, &smplrate_);
        alsamap->getValue(pluginKey, &plugin_);
    }

    Conf::MappingNode *pulsemap =(Conf::MappingNode *)(map->getValue("pulse"));

    if (pulsemap) {
        pulsemap->getValue(devicePlaybackKey, &devicePlayback_);
        pulsemap->getValue(deviceRecordKey, &deviceRecord_);
        pulsemap->getValue(deviceRingtoneKey, &deviceRingtone_);
    }
}

ShortcutPreferences::ShortcutPreferences() : hangup_(), pickup_(), popup_(),
    toggleHold_(), togglePickupHangup_() {}

std::map<std::string, std::string> ShortcutPreferences::getShortcuts() const
{
    std::map<std::string, std::string> shortcutsMap;

    shortcutsMap[hangupShortKey] = hangup_;
    shortcutsMap[pickupShortKey] = pickup_;
    shortcutsMap[popupShortKey] = popup_;
    shortcutsMap[toggleHoldShortKey] = toggleHold_;
    shortcutsMap[togglePickupHangupShortKey] = togglePickupHangup_;

    return shortcutsMap;
}


void ShortcutPreferences::setShortcuts(std::map<std::string, std::string> map)
{
    hangup_ = map[hangupShortKey];
    pickup_ = map[pickupShortKey];
    popup_ = map[popupShortKey];
    toggleHold_ = map[toggleHoldShortKey];
    togglePickupHangup_ = map[togglePickupHangupShortKey];
}


void ShortcutPreferences::serialize(Conf::YamlEmitter *emitter)
{
    Conf::MappingNode preferencemap(NULL);

    Conf::ScalarNode hangup(hangup_);
    Conf::ScalarNode pickup(pickup_);
    Conf::ScalarNode popup(popup_);
    Conf::ScalarNode toggleHold(toggleHold_);
    Conf::ScalarNode togglePickupHangup(togglePickupHangup_);

    preferencemap.setKeyValue(hangupShortKey, &hangup);
    preferencemap.setKeyValue(pickupShortKey, &pickup);
    preferencemap.setKeyValue(popupShortKey, &popup);
    preferencemap.setKeyValue(toggleHoldShortKey, &toggleHold);
    preferencemap.setKeyValue(togglePickupHangupShortKey, &togglePickupHangup);

    emitter->serializeShortcutPreference(&preferencemap);
}

void ShortcutPreferences::unserialize(Conf::MappingNode *map)
{
    if (map == NULL) {
        ERROR("ShortcutPreference: Error: Preference map is NULL");
        return;
    }

    map->getValue(hangupShortKey, &hangup_);
    map->getValue(pickupShortKey, &pickup_);
    map->getValue(popupShortKey, &popup_);
    map->getValue(toggleHoldShortKey, &toggleHold_);
    map->getValue(togglePickupHangupShortKey, &togglePickupHangup_);
}

