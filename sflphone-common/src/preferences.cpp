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

Preferences::Preferences() :  _accountOrder("")
			   , _audioApi(0)
			   , _historyLimit(30)
			   , _historyMaxCalls(20)
			   , _notifyMails(false)
			   , _zoneToneChoice("North America")
			   , _registrationExpire(180)
			   , _ringtoneEnabled(true)
			   , _portNum(5060)
			   , _searchBarDisplay(true)
			   , _zeroConfenable(false)
                           , _md5Hash(false)
{

}

Preferences::~Preferences() {}


void Preferences::serialize(Conf::YamlEmitter *emiter) 
{

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
  Conf::ScalarNode ringtoneEnabled(_ringtoneEnabled ? "true" : "false");
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
  preferencemap.setKeyValue(ringtoneEnabledKey, &ringtoneEnabled);
  preferencemap.setKeyValue(portNumKey, &portNum);
  preferencemap.setKeyValue(searchBarDisplayKey, &searchBarDisplay);
  preferencemap.setKeyValue(zeroConfenableKey, &zeroConfenable);
  preferencemap.setKeyValue(md5HashKey, &md5Hash);

  emiter->serializePreference(&preferencemap);
}

void Preferences::unserialize(Conf::MappingNode *map)
{

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
  val = (Conf::ScalarNode *)(map->getValue(ringtoneEnabledKey));
  if(val) { _registrationExpire = atoi(val->getValue().data()); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(portNumKey));
  if(val) { _portNum = atoi(val->getValue().data()); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(searchBarDisplayKey));
  if(val) { _searchBarDisplay = (val->getValue().compare("true") == 0) ? true : false; val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(zeroConfenableKey));
  if(val) { _zeroConfenable = (val->getValue().compare("true") == 0) ? true : false; val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(md5HashKey));
  if(val) { _md5Hash = (val->getValue().compare("true") == 0) ? true : false; val = NULL; }


  
}
