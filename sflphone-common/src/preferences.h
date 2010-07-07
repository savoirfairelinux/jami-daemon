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

class Preferences : public Serializable {

 public:

  Preferences();

  ~Preferences();

  virtual void serialize(Engine *engine);

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

#endif
