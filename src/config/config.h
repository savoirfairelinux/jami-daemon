/*
 *  Copyright (C) 2005-2007 Savoir-Faire Linux inc.
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
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
 */

#ifndef __CONFIG_CONFIG_H_
#define __CONFIG_CONFIG_H_

#include <map>
#include <string>
#include <list>

/**
 * @file config.h
 * @brief Configuration namespace for ConfigTree object (like .ini files)
 */
namespace Conf {

  class ConfigTreeItem;
  typedef std::map<std::string, ConfigTreeItem> ItemMap;
  typedef std::map<std::string, ItemMap*> SectionMap;
  typedef std::list<std::string> TokenList;

  class ConfigTreeItemException {
    public:
      /**
       * Constructor
       * */
      ConfigTreeItemException() {}
      
      /**
       * Destructor
       * */
      ~ConfigTreeItemException() {}
  };

  class ConfigTree;
  class ConfigTreeIterator 
  {
    public:
      /**
       * Parsing method
       * @return TokenList
       */
      TokenList begin();

      /**
       * Parsing method
       * @return TokenList
       */
      const TokenList& end() const { return _endToken; }

      /**
       * Parsing method
       * @return TokenList
       */
      TokenList next();

    private:
      friend class ConfigTree;
      ConfigTreeIterator(ConfigTree *configTree) : _tree(configTree) {}

      ConfigTree* _tree;
      TokenList _endToken;
      SectionMap::iterator _iter;
      ItemMap::iterator _iterItem;
  };

  class ConfigTree {
    public:
      ConfigTree();
      ~ConfigTree();

      void createSection(const std::string& section);
      void removeSection(const std::string& section);
      /**
       * Return an array of strings, listing the sections of the config file
       *
       * This will be mainly used to filter which sections are an
       * "Account" definition.
       *
       * @return array Strings of the sections
       */
      TokenList getSections();

      void addConfigTreeItem(const std::string& section, const ConfigTreeItem item);
      /**
       * Set a configuration value.
       *
       * @param section Write to this [section] of the .ini file
       * @param itemName The itemName= in the .ini file
       * @param value The value to assign to that itemName
       */
      bool setConfigTreeItem(const std::string& section, const std::string& itemName, const std::string& value);

      /**
       * Get a value.
       *
       * This function does all the validity tests, so none are needed throughout
       * the program.
       *
       * @param section The name of the [section] in the .ini file.
       * @param itemName The name of the item= in the .ini file.
       * @return The value of the corresponding item. The default value if the section exists
       *         but the item doesn't.
       */
      std::string getConfigTreeItemValue(const std::string& section, const std::string& itemName);
      int getConfigTreeItemIntValue(const std::string& section, const std::string& itemName);

      /**
       * Flush data to .ini file
       */
      bool saveConfigTree(const std::string& fileName);

      /**
       * Load data (and fill ConfigTree) from disk
       */
      int  populateFromFile(const std::string& fileName);

      bool getConfigTreeItemToken(const std::string& section, const std::string& itemName, TokenList& arg);

    private:
      ConfigTreeItem* getConfigTreeItem(const std::string& section, const std::string& itemName);

      /**
       * List of sections. Each sections has an ItemList as child
       */
      SectionMap _sections;
      friend class ConfigTreeIterator;

    public:
      ConfigTreeIterator createIterator() {
	return ConfigTreeIterator(this);
      }
  };

  class ConfigTreeItem {
    public:
      ConfigTreeItem() : _defaultValue(""), _type("string") {}
      // defaultvalue = value
      ConfigTreeItem(const std::string& name, const std::string& value, const std::string& type) : 
	_name(name), _value(value), 
	_defaultValue(value), _type(type) {}
      ConfigTreeItem(const std::string& name, const std::string& value, const std::string& defaultValue, const std::string& type) : 
	_name(name), _value(value), 
	_defaultValue(defaultValue), _type(type) {}
      ~ConfigTreeItem() {}

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

} // end namespace ConfigTree

#endif
