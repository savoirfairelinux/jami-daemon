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

#include "config.h"
namespace Conf {

// ctor
Config::Config() 
{
}

// dtor
Config::~Config() 
{
  // erase every new ItemMap (by CreateSection)
  SectionMap::iterator iter = _sections.begin();
  while(iter != _sections.end()) {
    delete iter->second;
    iter->second = NULL;
    iter++;
  }
}

/**
 * Create the section only if it doesn't exists
 */
void
Config::createSection(const std::string& section) {
  // if we doesn't find the item, create it
  if (_sections.find(section) == _sections.end()) {
    _sections[section] = new ItemMap;
  }
}

/**
 * Add the config item only if it exists..
 * If the section doesn't exists, create it
 */
void 
Config::addConfigItem(const std::string& section, const ConfigItem& item) 
{
  // if we doesn't find the item, create it
  SectionMap::iterator iter = _sections.find(section);
  if ( iter == _sections.end()) {
    _sections[section] = new ItemMap;
    iter = _sections.find(section);
  }
  // be prudent here
  if (iter!=NULL && iter != _sections.end()) {
    std::string name = item.getName();

    if ( iter->second->find(name) == iter->second->end()) {
      (*(iter->second))[name] = item;
    }
  }
}

// throw a ConfigItemException if not found
std::string 
Config::getConfigItemValue(const std::string& section, const std::string& itemName) 
{
  ConfigItem* item = getConfigItem(section, itemName);
  if (item!=NULL) {
    return item->getValue();
  } else {
    throw new ConfigItemException();
  }
  return "";
}

// throw a ConfigItemException if not found
int 
Config::getConfigItemIntValue(const std::string& section, const std::string& itemName) 
{
  ConfigItem* item = getConfigItem(section, itemName);
  if (item!=NULL && item->getType() == "int") {
    return atoi(item->getValue().data());
  } else {
    throw new ConfigItemException();
  }
  return 0;
}

/**
 * Return a ConfigItem or NULL if not found
 */
ConfigItem* 
Config::getConfigItem(const std::string& section, const std::string& itemName) {
  SectionMap::iterator iter = _sections.find(section);
  if ( iter == _sections.end()) {
    return NULL;
  }
  ItemMap::iterator iterItem = iter->second->find(itemName);
  if ( iterItem == iter->second->end()) {
    return NULL;
  } 
  return &(iterItem->second);
}

/**
 * Set the configItem if found, else do nothing
 */
void 
Config::setConfigItem(const std::string& section, const std::string& itemName, const std::string& value) {
  SectionMap::iterator iter = _sections.find(section);
  if ( iter == _sections.end()) {
    return;
  }
  ItemMap::iterator iterItem = iter->second->find(itemName);
  if ( iterItem == iter->second->end()) {
    return;
  }
  iterItem->second.setValue(value);
}

} // end namespace Config
