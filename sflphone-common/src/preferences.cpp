/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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
#include "user_cfg.h"

Preferences::Preferences() :  _accountOrder("")
			   , _audioApi(0)
			   , _historyLimit(30)
			   , _historyMaxCalls(20)
			   , _notifyMails(false)
			   , _zoneToneChoice(DFT_ZONE) // DFT_ZONE
			   , _registrationExpire(180)
			   , _portNum(5060)
			   , _searchBarDisplay(true)
			   , _zeroConfenable(false)
                           , _md5Hash(false)
{

}

Preferences::~Preferences() {}


void Preferences::serialize(Conf::YamlEmitter *emiter) 
{

  _debug("Preference: Serialize configuration");

  Conf::MappingNode preferencemap(NULL);
  
  Conf::ScalarNode order(_accountOrder);
  std::stringstream audiostr; audiostr << _audioApi; 
  Conf::ScalarNode audioapi(audiostr.str());
  std::stringstream histlimitstr; histlimitstr << _historyLimit;
  Conf::ScalarNode historyLimit(histlimitstr.str());
  std::stringstream histmaxstr; histmaxstr << _historyMaxCalls;
  Conf::ScalarNode historyMaxCalls(histmaxstr.str());
  Conf::ScalarNode notifyMails(_notifyMails ? "true" : "false");
  Conf::ScalarNode zoneToneChoice(_zoneToneChoice);
  std::stringstream expirestr; expirestr << _registrationExpire;
  Conf::ScalarNode registrationExpire(expirestr.str());
  std::stringstream portstr; portstr << _portNum;
  Conf::ScalarNode portNum(portstr.str());
  Conf::ScalarNode searchBarDisplay(_searchBarDisplay ? "true" : "false");
  Conf::ScalarNode zeroConfenable(_zeroConfenable ? "true" : "false");
  Conf::ScalarNode md5Hash(_md5Hash ? "true" : "false");

  preferencemap.setKeyValue(orderKey, &order);
  preferencemap.setKeyValue(audioApiKey, &audioapi);
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

  _debug("Preference: Unserialize configuration");

  Conf::ScalarNode *val;

  val = (Conf::ScalarNode *)(map->getValue(orderKey));
  if(val) { _accountOrder = val->getValue(); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(audioApiKey));
  if(val) { _audioApi = atoi(val->getValue().data()); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(historyLimitKey));
  if(val) { _historyLimit = atoi(val->getValue().data()); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(historyMaxCallsKey));
  if(val) { _historyMaxCalls = atoi(val->getValue().data()); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(notifyMailsKey));
  if(val) { _notifyMails = atoi(val->getValue().data()); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(zoneToneChoiceKey));
  if(val) { _zoneToneChoice = val->getValue(); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(registrationExpireKey));
  if(val) { _registrationExpire = atoi(val->getValue().data()); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(portNumKey));
  if(val) { _portNum = atoi(val->getValue().data()); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(searchBarDisplayKey));
  if(val && !val->getValue().empty()) { _searchBarDisplay = (val->getValue().compare("true") == 0) ? true : false; val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(zeroConfenableKey));
  if(val && !val->getValue().empty()) { _zeroConfenable = (val->getValue().compare("true") == 0) ? true : false; val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(md5HashKey));
  if(val && !val->getValue().empty()) { _md5Hash = (val->getValue().compare("true") == 0) ? true : false; val = NULL; }


  
}


VoipPreference::VoipPreference() :  _playDtmf(true)
				 , _playTones(true)
				 , _pulseLength(atoi(DFT_PULSE_LENGTH_STR))// DFT_PULSE_LENGTH_STR   
				 , _sendDtmfAs(0)
				 , _symmetricRtp(true)
                                 , _zidFile(ZRTP_ZIDFILE)// ZRTP_ZID_FILENAME
{

}

VoipPreference::~VoipPreference() {}


void VoipPreference::serialize(Conf::YamlEmitter *emitter) 
{
  _debug("VoipPreference: Serialize configuration");

  Conf::MappingNode preferencemap(NULL);

  Conf::ScalarNode playDtmf(_playDtmf ? "true" : "false");
  Conf::ScalarNode playTones(_playTones ? "true" : "false");
  std::stringstream pulselengthstr; pulselengthstr << _pulseLength;
  Conf::ScalarNode pulseLength(pulselengthstr.str());
  std::stringstream senddtmfstr; senddtmfstr << _sendDtmfAs;
  Conf::ScalarNode sendDtmfAs(senddtmfstr.str());
  Conf::ScalarNode symmetricRtp(_symmetricRtp ? "true" : "false");
  Conf::ScalarNode zidFile(_zidFile.c_str());

  preferencemap.setKeyValue(playDtmfKey, &playDtmf);
  preferencemap.setKeyValue(playTonesKey, &playTones);
  preferencemap.setKeyValue(pulseLengthKey, &pulseLength);
  preferencemap.setKeyValue(sendDtmfAsKey, &sendDtmfAs);
  preferencemap.setKeyValue(symmetricRtpKey, &symmetricRtp);
  preferencemap.setKeyValue(zidFileKey, &zidFile);

  emitter->serializeVoipPreference(&preferencemap);
}

void VoipPreference::unserialize(Conf::MappingNode *map) 
{

  _debug("VoipPreference: Unserialize configuration");

  Conf::ScalarNode *val = NULL;

  val = (Conf::ScalarNode *)(map->getValue(playDtmfKey));
  if(val && !val->getValue().empty()) { _playDtmf = (val->getValue().compare("true") == 0) ? true : false; val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(playTonesKey));
  if(val && !val->getValue().empty()) { _playTones = (val->getValue().compare("true") == 0) ? true : false; val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(pulseLengthKey));
  if(val) { _pulseLength = atoi(val->getValue().data()); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(sendDtmfAsKey));
  if(val) { _sendDtmfAs = atoi(val->getValue().data()); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(symmetricRtpKey));
  if(val && !val->getValue().empty()) { _symmetricRtp = (val->getValue().compare("true") == 0) ? true : false; val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(zidFileKey));
  if(val) { _zidFile = val->getValue().c_str(); val = NULL; }
  
}



AddressbookPreference::AddressbookPreference() : _photo(true) 
					       , _enabled(true)
					       , _list("")
					       , _maxResults(25)
					       , _business(true)
					       , _home(true)
					       , _mobile(true)
{

}

AddressbookPreference::~AddressbookPreference() {}

void AddressbookPreference::serialize(Conf::YamlEmitter *emitter)
{
  _debug("Addressbook: Serialize configuration");
  
  Conf::MappingNode preferencemap(NULL);

  Conf::ScalarNode photo(_photo ? "true" : "false");
  Conf::ScalarNode enabled(_enabled ? "true" : "false");
  Conf::ScalarNode list(_list);
  std::stringstream maxresultstr; maxresultstr << _maxResults;
  Conf::ScalarNode maxResults(maxresultstr.str());
  Conf::ScalarNode business(_business ? "true" : "false");
  Conf::ScalarNode home(_home ? "true" : "false");
  Conf::ScalarNode mobile(_mobile ? "true" : "false");

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
  _debug("Addressbook: Unserialize configuration");

  Conf::ScalarNode *val = NULL;

  val = (Conf::ScalarNode *)(map->getValue(photoKey));
  if(val && !(val->getValue().empty())) { _photo = (val->getValue() == "true") ? true : false; val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(enabledKey));
  if(val && !val->getValue().empty()) { _enabled = (val->getValue() == "true") ? true : false; val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(listKey));
  if(val) { _list = val->getValue(); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(maxResultsKey));
  if(val) { _maxResults = atoi(val->getValue().data()); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(businessKey));
  if(val && !val->getValue().empty()) { _business = (val->getValue() == "true") ? true : false; val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(homeKey));
  if(val && !val->getValue().empty()) { _home = (val->getValue() == "true") ? true : false; val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(mobileKey));
  if(val && !val->getValue().empty()) { _mobile = (val->getValue() == "true") ? true : false; val = NULL; }
  
}


HookPreference::HookPreference() : _iax2Enabled(false)
				 , _numberAddPrefix("")
				 , _numberEnabled(false)
				 , _sipEnabled(false)
				 , _urlCommand("x-www-browser")
				 , _urlSipField("X-sflphone-url")
{

}

HookPreference::~HookPreference() {}

void HookPreference::serialize(Conf::YamlEmitter *emitter) 
{
  _debug("Hook: Serialize configuration");

  Conf::MappingNode preferencemap(NULL);

  Conf::ScalarNode iax2Enabled(_iax2Enabled ? "true" : "false");
  Conf::ScalarNode numberAddPrefix(_numberAddPrefix);
  Conf::ScalarNode numberEnabled(_numberEnabled ? "true" : "false");
  Conf::ScalarNode sipEnabled(_sipEnabled ? "true" : "false");
  Conf::ScalarNode urlCommand(_urlCommand);
  Conf::ScalarNode urlSipField(_urlSipField);

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
  Conf::ScalarNode *val = NULL;

  _debug("Hook: Unserialize preference");

  val = (Conf::ScalarNode *)(map->getValue(iax2EnabledKey));
  if(val) { _iax2Enabled = (val->getValue() == "true") ? true : false; val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(numberAddPrefixKey));
  if(val) { _numberAddPrefix = val->getValue(); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(numberEnabledKey));
  if(val) { _numberEnabled = (val->getValue() == "true") ? true : false; val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(sipEnabledKey));
  if(val) { _sipEnabled = (val->getValue() == "true") ? true : false; val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(urlCommandKey));
  if(val) { _urlCommand = val->getValue(); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(urlSipFieldKey));
  if(val) { _urlSipField = val->getValue(); val = NULL; }
  

}



AudioPreference::AudioPreference() : _cardin(atoi(ALSA_DFT_CARD)) // ALSA_DFT_CARD
				   , _cardout(atoi(ALSA_DFT_CARD)) // ALSA_DFT_CARD
				   , _cardring(atoi(ALSA_DFT_CARD)) // ALSA_DFT_CARD
				   , _framesize(atoi(DFT_FRAME_SIZE)) // DFT_FRAME_SIZE
				   , _plugin("default") // PCM_DEFAULT
				   , _smplrate(44100) // DFT_SAMPLE_RATE
				   , _devicePlayback("")
				   , _deviceRecord("")
				   , _deviceRingtone("")
				   , _recordpath("") // DFT_RECORD_PATH
				   , _volumemic(atoi(DFT_VOL_SPKR_STR)) // DFT_VOL_SPKR_STR
				   , _volumespkr(atoi(DFT_VOL_MICRO_STR)) // DFT_VOL_MICRO_STR
{

}

AudioPreference::~AudioPreference() {}

void AudioPreference::serialize(Conf::YamlEmitter *emitter) 
{
  _debug("AudioPreference: Serialize configuration");

  Conf::MappingNode preferencemap(NULL);
  Conf::MappingNode alsapreferencemap(NULL);
  Conf::MappingNode pulsepreferencemap(NULL);

    // alsa preference
  std::stringstream instr; instr << _cardin;
  Conf::ScalarNode cardin(instr.str()); // 0
  std::stringstream outstr; outstr << _cardout;
  Conf::ScalarNode cardout(outstr.str()); // 0
  std::stringstream ringstr; ringstr << _cardring;
  Conf::ScalarNode cardring(ringstr.str());// 0
  std::stringstream framestr; framestr << _framesize;
  Conf::ScalarNode framesize(framestr.str()); // 20
  Conf::ScalarNode plugin(_plugin); // default
  std::stringstream ratestr; ratestr << _smplrate;
  Conf::ScalarNode smplrate(ratestr.str());// 44100
   
  //pulseaudio preference
  Conf::ScalarNode devicePlayback(_devicePlayback);//:
  Conf::ScalarNode deviceRecord(_deviceRecord); //:
  Conf::ScalarNode deviceRingtone(_deviceRingtone); //:

  // general preference
  Conf::ScalarNode recordpath(_recordpath); //: /home/msavard/Bureau
  std::stringstream micstr; micstr << _volumemic;
  Conf::ScalarNode volumemic(micstr.str()); //:  100
  std::stringstream spkrstr; spkrstr << _volumespkr;
  Conf::ScalarNode volumespkr(spkrstr.str()); //: 100

  preferencemap.setKeyValue(recordpathKey, &recordpath);
  preferencemap.setKeyValue(volumemicKey, &volumemic);
  preferencemap.setKeyValue(volumespkrKey, &volumespkr);
  
  preferencemap.setKeyValue(alsamapKey, &alsapreferencemap);
  alsapreferencemap.setKeyValue(cardinKey, &cardin);
  alsapreferencemap.setKeyValue(cardoutKey, &cardout);
  alsapreferencemap.setKeyValue(cardringKey, &cardring);
  alsapreferencemap.setKeyValue(framesizeKey, &framesize);
  alsapreferencemap.setKeyValue(pluginKey, &plugin);
  alsapreferencemap.setKeyValue(smplrateKey, &smplrate);

  preferencemap.setKeyValue(pulsemapKey, &pulsepreferencemap);
  pulsepreferencemap.setKeyValue(devicePlaybackKey, &devicePlayback);
  pulsepreferencemap.setKeyValue(deviceRecordKey, &deviceRecord);
  pulsepreferencemap.setKeyValue(deviceRingtoneKey, &deviceRingtone);

  emitter->serializeAudioPreference(&preferencemap);
  
}

void AudioPreference::unserialize(Conf::MappingNode *map) 
{
  _debug("AudioPreference: Unserialize configuration");

  Conf::ScalarNode *val = NULL;

  val = (Conf::ScalarNode *)(map->getValue(cardinKey));
  if(val) { _cardin = atoi(val->getValue().data()); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(cardoutKey));
  if(val) { _cardout = atoi(val->getValue().data()); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(cardringKey));
  if(val) { _cardring = atoi(val->getValue().data()); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(framesizeKey));
  if(val) { _framesize = atoi(val->getValue().data()); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(smplrateKey));
  if(val) { _smplrate = atoi(val->getValue().data()); val = NULL; }

  val = (Conf::ScalarNode *)(map->getValue(devicePlaybackKey));
  if(val) { _devicePlayback = val->getValue(); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(deviceRecordKey));
  if(val) { _deviceRecord = val->getValue(); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(deviceRingtoneKey));
  if(val) { _deviceRingtone = val->getValue(); val = NULL; }

  val = (Conf::ScalarNode *)(map->getValue(recordpathKey));
  if(val) { _recordpath = val->getValue(); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(volumemicKey));
  if(val) { _volumemic = atoi(val->getValue().data()); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(volumespkrKey));
  if(val) { _volumespkr = atoi(val->getValue().data()); val = NULL; }

}
