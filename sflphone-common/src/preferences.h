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

#ifndef __PREFERENCE_H__
#define __PREFERENCE_H__

#include "config/serializable.h"

// general preferences
const Conf::Key orderKey("order");                          // :	1234/2345/
const Conf::Key audioApiKey("audioApi");                    // :	0
const Conf::Key historyLimitKey("historyLimit");            // :	30
const Conf::Key historyMaxCallsKey("historyMaxCalls");      // :	20
const Conf::Key notifyMailsKey("notifyMails");              // :	false
const Conf::Key zoneToneChoiceKey("zoneToneChoice");        // :	North America
const Conf::Key registrationExpireKey("registrationExpire");// :	180
const Conf::Key ringtoneEnabledKey("ringtoneEnabled");      // :	true
const Conf::Key portNumKey("portNum");                      // :	5060
const Conf::Key searchBarDisplayKey("searchBarDisplay");    // :	true
const Conf::Key zeroConfenableKey("zeroConfenable");        // :	false
const Conf::Key md5HashKey("md5Hash");                      // :	false

// voip preferences
const Conf::Key playDtmfKey("playDtmf"); // true                    true
const Conf::Key playTonesKey("playTones");  // true
const Conf::Key pulseLengthKey("pulseLength"); //=250
const Conf::Key sendDtmfAsKey("sendDtmfAs");// =0
const Conf::Key symmetricRtpKey("symmetric");// =true
const Conf::Key zidFileKey("zidFile");// =sfl.zid

// addressbook preferences
const Conf::Key photoKey("photo");//		false
const Conf::Key enabledKey("enabled");//		true
const Conf::Key listKey("list");//		1243608768.30329.0@emilou-desktop/1243456917.15690.23@emilou-desktop/
const Conf::Key maxResultsKey("maxResults");//		25
const Conf::Key businessKey("business");//		true
const Conf::Key homeKey("home");//		false
const Conf::Key mobileKey("mobile");//		false

// hooks preferences
const Conf::Key iax2EnabledKey("iax2Enabled");// :		false
const Conf::Key numberAddPrefixKey("numberAddPrefix");//:	false
const Conf::Key numberEnabledKey("numberEnabled"); //:	false
const Conf::Key sipEnabledKey("sipEnabled"); //:		false
const Conf::Key urlCommandKey("urlCommand"); //:		x-www-browser
const Conf::Key urlSipFieldKey("urlSipField"); //:		X-sflphone-url


const Conf::Key alsamapKey("alsa");
const Conf::Key pulsemapKey("pulse");
const Conf::Key cardinKey("cardin");// : 0
const Conf::Key cardoutKey("cardout");// 0
const Conf::Key cardringKey("cardring");// : 0
const Conf::Key framesizeKey("framesize");// : 20
const Conf::Key pluginKey("plugin"); //: default
const Conf::Key smplrateKey("smplrate");//: 44100
const Conf::Key devicePlaybackKey("devicePlayback");//:
const Conf::Key deviceRecordKey("deviceRecord");// :
const Conf::Key deviceRingtoneKey("deviceRingtone");// :
const Conf::Key recordpathKey("recordpath");//: /home/msavard/Bureau
const Conf::Key ringchoiceKey("ringchoice");//: /usr/share/sflphone/ringtones/konga.ul
const Conf::Key volumemicKey("volumemic");//:  100
const Conf::Key volumespkrKey("volumespkr");//: 100


class Preferences : public Serializable {

 public:

  Preferences();

  ~Preferences();

  virtual void serialize(Conf::YamlEmitter *emitter);

  virtual void unserialize(Conf::MappingNode *map);


  std::string getAccountOrder(void) { return _accountOrder; }
  void setAccountOrder(std::string ord) { _accountOrder = ord; }

  int getAudioApi(void) { return _audioApi; }
  void setAudioApi(int api) { _audioApi = api; }

  int getHistoryLimit(void) { return _historyLimit; }
  void setHistoryLimit(int lim) { _historyLimit = lim; }

  int getHistoryMaxCalls(void) { return _historyMaxCalls; }
  void setHistoryMaxCalls(int max) { _historyMaxCalls = max; }

  bool getNotifyMails(void) { return _notifyMails; }
  void setNotifyMails(bool mails) { _notifyMails = mails; }

  std::string getZoneToneChoice(void) { return _zoneToneChoice; }
  void setZoneToneChoice(std::string str) { _zoneToneChoice = str; }

  int getRegistrationExpire(void) { return _registrationExpire; }
  void setRegistrationExpire(int exp) { _registrationExpire = exp; }

  bool getRingtoneEnabled(void) { return _ringtoneEnabled; }
  void setRingtoneEnabled(bool ring) { _ringtoneEnabled = ring; }

  int getPortNum(void) { return _portNum; }
  void setPortNum(int port) { _portNum = port; }

  bool getSearchBarDisplay(void) { return _searchBarDisplay; }
  void setSearchBarDisplay(bool search) { _searchBarDisplay = search; }

  bool getZeroConfenable(void) { return _zeroConfenable; }
  void setZeroConfenable(bool enable) { _zeroConfenable = enable; }

  bool getMd5Hash(void) { return _md5Hash; }
  void setMd5Hash(bool md5) { _md5Hash = md5; }

 private:

  // account order
  std::string _accountOrder;

  int _audioApi;
  int _historyLimit;
  int _historyMaxCalls;
  bool _notifyMails;
  std::string _zoneToneChoice;
  int _registrationExpire;
  bool _ringtoneEnabled;
  int _portNum;
  bool _searchBarDisplay;
  bool _zeroConfenable;
  bool _md5Hash;

};


class VoipPreference : public Serializable {

 public:

  VoipPreference();

  ~VoipPreference();

  virtual void serialize(Conf::YamlEmitter *emitter);

  virtual void unserialize(Conf::MappingNode *map);

  bool getPlayDtmf(void) { return _playDtmf; }
  void setPlayDtmf(bool dtmf) { _playDtmf = dtmf; }

  bool getPlayTones(void) { return _playTones; }
  void setPlayTones(bool tone) { _playTones = tone; }

  int getPulseLength(void) { return _pulseLength; }
  void setPulseLength(int length) { _pulseLength = length; }

  int getSendDtmfAs(void) { return _sendDtmfAs; }
  void setSendDtmfAs(int dtmf) { _sendDtmfAs = dtmf; }

  bool getSymmetricRtp(void) { return _symmetricRtp; }
  void setSymmetricRtp(bool sym) { _symmetricRtp = sym; }

  std::string getZidFile(void) { return _zidFile; }
  void setZidFile(std::string file) { _zidFile = file; }

 private:

  bool _playDtmf;
  bool _playTones;
  int _pulseLength;
  int _sendDtmfAs;
  bool _symmetricRtp;
  std::string _zidFile;

};

class AddressbookPreference : public Serializable {

 public:

  AddressbookPreference();

  ~AddressbookPreference();

  virtual void serialize(Conf::YamlEmitter *emitter);

  virtual void unserialize(Conf::MappingNode *map);

  bool getPhoto(void) { return _photo;}
  void setPhoto(bool p) { _photo = p; }

  bool getEnabled(void) { return _enabled; }
  void setEnabled(bool e) { _enabled = e; }

  std::string getList(void) { return _list; }
  void setList(std::string l) { _list = l; }

  int getMaxResults(void) { return _maxResults; }
  void setMaxResults(int r) { _maxResults = r; }

  bool getBusiness(void) { return _business; }
  void setBusiness(bool b) { _business = b; }

  bool getHome(void) { return _home; }
  void setHone(bool h) { _home = h; }

  bool getMobile(void) { return _mobile; }
  void setMobile(bool m) { _mobile = m; }

 private:

  bool _photo;
  bool _enabled;
  std::string _list;
  int _maxResults;
  bool _business;
  bool _home;
  bool _mobile;

};


class HookPreference : public Serializable {

 public:

  HookPreference();

  ~HookPreference();

  virtual void serialize(Conf::YamlEmitter *emitter);

  virtual void unserialize(Conf::MappingNode *map);

  bool getIax2Enabled(void) { return _iax2Enabled; }
  void setIax2Enabled( bool i) { _iax2Enabled = i; }

  std::string getNumberAddPrefix(void) { return _numberAddPrefix; }
  void setNumberAddPrefix(std::string n) { _numberAddPrefix = n; }

  bool getNumberEnabled(void) { return _numberEnabled; }
  void setNumberEnabled(bool n) { _numberEnabled = n; }

  bool getSipEnabled(void) { return _sipEnabled; }
  void setSipEnabled(bool s) { _sipEnabled = s; }

  std::string getUrlCommand(void) { return _urlCommand; }
  void setUrlCommand(std::string u) { _urlCommand = u; }

  std::string getUrlSipField(void) { return _urlSipField; }
  void setUrlSipField(std::string u) { _urlSipField = u; }

 private:

  bool _iax2Enabled;// :		false
  std::string _numberAddPrefix;//:	false
  bool _numberEnabled; //:	false
  bool _sipEnabled; //:		false
  std::string _urlCommand; //:		x-www-browser
  std::string _urlSipField; //:		X-sflphone-url

};


class AudioPreference : public Serializable {

 public:

  AudioPreference();

  ~AudioPreference();

  virtual void serialize(Conf::YamlEmitter *emitter);

  virtual void unserialize(Conf::MappingNode *map);

  // alsa preference
  int getCardin(void) { return _cardin; }
  void setCardin(int c) { _cardin = c; }

  int getCardout(void) { return _cardout; }
  void setCardout(int c) { _cardout = c; }

  int getCardring(void) { return _cardring; }
  void setCardring(int c) { _cardring = c; }

  int getFramesize(void) { return _framesize; }
  void setFramesize(int f) { _framesize = f; }
  
  std::string getPlugin(void) { return _plugin; }
  void setPlugin(std::string p) { _plugin = p; }
  
  int getSmplrate(void) { return _smplrate; }
  void setSmplrate(int r) { _smplrate = r; }
   
  //pulseaudio preference
  std::string getDevicePlayback(void) { return _devicePlayback; }
  void setDevicePlayback(std::string p) { _devicePlayback = p; }
  
  std::string getDeviceRecord(void) { return _deviceRecord; }
  void setDeviceRecord(std::string r) { _deviceRecord = r; }

  std::string getDeviceRingtone(void) { return _deviceRingtone; }
  void setDeviceRingtone(std::string r) { _deviceRingtone = r; }

  // general preference
  std::string getRecordpath(void) { return _recordpath; }
  void setRecordpath(std::string r) { _recordpath = r; }

  std::string getRingchoice(void) { return _ringchoice; }
  void setRingchoice(std::string r) { _ringchoice = r; }

  int getVolumemic(void) { return _volumemic; }
  void setVolumemic(int m) { _volumemic = m; }

  int getVolumespkr(void) { return _volumespkr; }
  void setVolumespkr(int s) { _volumespkr = s; }

 private:
   
  // alsa preference
  int _cardin; // 0
  int _cardout; // 0
  int _cardring;// 0
  int _framesize; // 20
  std::string _plugin; // default
  int _smplrate;// 44100
   
  //pulseaudio preference
  std::string _devicePlayback;//:
  std::string _deviceRecord; //:
  std::string _deviceRingtone; //:

  // general preference
  std::string _recordpath; //: /home/msavard/Bureau
  std::string _ringchoice; // : /usr/share/sflphone/ringtones/konga.ul
  int _volumemic; //:  100
  int _volumespkr; //: 100
  
};

#endif
