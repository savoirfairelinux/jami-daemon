/**
 *  Copyright (C) 2005 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *                                                                              
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
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
 */
#include "DNSServiceTXTRecord.h"
#include "../global.h" // for _debug


/**
 * Simple constructor
 */
DNSServiceTXTRecord::DNSServiceTXTRecord() 
{
}
 
/**
 * Simple destructor
 */
DNSServiceTXTRecord::~DNSServiceTXTRecord() 
{
}

/**
 * add a pair of key/value inside the associative std::map
 * @param key    unique key inside the std::map
 * @param value  value associated to the key
 */
void 
DNSServiceTXTRecord::addKeyValue(const std::string &key, const std::string &value) 
{
  _map[key] = value;
}
/**
 * get a value from a key
 * @param key    unique key inside the std::map
 * @return the value or empty
 */
const std::string &
DNSServiceTXTRecord::getValue(const std::string &key) 
{
  return _map[key]; // return std::string("") if it's not there
}

/**
 * get a value from a key
 * @param key    unique key inside the std::map
 * @return the value or empty
 */
const std::string &
DNSServiceTXTRecord::getValue(const char* key) 
{
  return getValue(std::string(key));
}

void 
DNSServiceTXTRecord::listValue() 
{
  std::map<std::string, std::string>::iterator iter;
  for (iter=_map.begin(); iter != _map.end(); iter++) {
    _debug ( "\t%s:%s\n", iter->first.c_str(), iter->second.c_str());
  }
}
