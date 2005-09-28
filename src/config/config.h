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

#ifndef __CONFIG_CONFIG_H_
#define __CONFIG_CONFIG_H_

#include <map>
#include <string>

namespace Conf {

class ConfigItem;
typedef std::map<std::string, ConfigItem> ItemMap;
typedef std::map<std::string, ItemMap*> SectionMap;

class ConfigItemException {
public:
  ConfigItemException() {}
  ~ConfigItemException() {}
};

class Config {
public:
  Config();
  ~Config();

  void createSection(const std::string& section);
  void addConfigItem(const std::string& section, const ConfigItem &item);
  void setConfigItem(const std::string& section, const std::string& itemName, const std::string& value);

  // throw a ConfigItemException if not found
  std::string getConfigItemValue(const std::string& section, const std::string& itemName);
  int getConfigItemIntValue(const std::string& section, const std::string& itemName);

private:
  ConfigItem* getConfigItem(const std::string& section, const std::string& itemName);

  SectionMap _sections;
};

class ConfigItem {
public:
  ConfigItem() : _defaultValue(""), _type("string") {}
  ConfigItem(const std::string& name, const std::string& value, const std::string& defaultValue, const std::string& type) : 
    _name(name), _value(value), 
    _defaultValue(defaultValue), _type(type) {}
  ~ConfigItem();

  void setValue(const std::string& value) { _value = value; }
  const std::string getName() const { return _name; }
  const std::string getValue() const  { return _value; }
  const std::string getDefaultValue() const  { return _defaultValue; }
  const std::string getType() const  { return _type; }

private:
  std::string _name;
  std::string _value;
  std::string _defaultValue;
  std::string _type;
};


} // end namespace Config

#endif
