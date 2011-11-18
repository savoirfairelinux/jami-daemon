/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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

#ifndef CONF_CONFIG_H__
#define CONF_CONFIG_H__

#include <map>
#include <string>
#include <list>
#include "noncopyable.h"

/**
 * @file config.h
 * @brief Configuration namespace for ConfigTree object (like .ini files)
 */

namespace Conf {
static const char * const TRUE_STR = "true";

class ConfigTreeItem;
typedef std::map<std::string, ConfigTreeItem> ItemMap;
typedef std::map<std::string, ItemMap*> SectionMap;
typedef std::list<std::string> TokenList;

class ConfigTreeItemException {
};

class ConfigTree;

class ConfigTreeIterator {

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
        const TokenList& end() const {
            return endToken_;
        }

        /**
         * Parsing method
         * @return TokenList
         */
        TokenList next();

    private:
        friend class ConfigTree;
        ConfigTreeIterator(ConfigTree *configTree) : tree_(configTree), endToken_(), iter_(), iterItem_() {}

        NON_COPYABLE(ConfigTreeIterator);

        ConfigTree* tree_;
        TokenList endToken_;
        SectionMap::iterator iter_;
        ItemMap::iterator iterItem_;
};

class ConfigTree {
    public:
        ConfigTree() : sections_(), defaultValueMap_() {}
        ~ConfigTree();
        /**
         * Add a default value for a given key.
         * It looks in a map of default values when
         * the value for a given key cannot be found.
         *
         * @param section the section under which the given key/value pair
                          should go under. Note that this has no effect
                          when searching for a default value later on. Only
                          one possible value is actually supported as a default
                          value for a given key.
           @param token   A default key/value pair.
         */
        void addDefaultValue(const std::pair<std::string, std::string>& token, std::string section = "");

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
         * If the key cannot be found in  the actual file representation in
         * memory, it check for a default value in the default value map. If it's
         * not found there, it will return an empty string.
         *
         * @param section The name of the [section] in the .ini file.
         * @param itemName The name of the item= in the .ini file.
         * @return The value of the corresponding item. The default value if the section exists
         *         but the item doesn't.
         */
        std::string getConfigTreeItemValue(const std::string& section, const std::string& itemName) const;
        int getConfigTreeItemIntValue(const std::string& section, const std::string& itemName) const;
        bool getConfigTreeItemBoolValue(const std::string& section, const std::string& itemName) const;

        /**
         * Flush data to .ini file
         */
        bool saveConfigTree(const std::string& fileName);

        /**
         * Load data (and fill ConfigTree) from disk
         */
        int  populateFromFile(const std::string& fileName);

        bool getConfigTreeItemToken(const std::string& section, const std::string& itemName, TokenList& arg) const;

    private:
        std::string getDefaultValue(const std::string& key) const;
        const ConfigTreeItem* getConfigTreeItem(const std::string& section, const std::string& itemName) const;

        /**
         * List of sections. Each sections has an ItemList as child
         */
        SectionMap sections_;

        std::map<std::string, std::string> defaultValueMap_;

        friend class ConfigTreeIterator;

        NON_COPYABLE(ConfigTree);
    public:
        ConfigTreeIterator createIterator() {
            return ConfigTreeIterator(this);
        }
};

class ConfigTreeItem {

    public:
        ConfigTreeItem() : name_(""), value_(""), defaultValue_(""), type_("string") {}

        // defaultvalue = value
        ConfigTreeItem(const std::string& name, const std::string& value, const std::string& type) :
            name_(name), value_(value),
            defaultValue_(value), type_(type) {}

        ConfigTreeItem(const std::string& name, const std::string& value, const std::string& defaultValue, const std::string& type) :
            name_(name), value_(value),
            defaultValue_(defaultValue), type_(type) {}

        void setValue(const std::string& value) {
            value_ = value;
        }

        const std::string getName() const {
            return name_;
        }

        const std::string getValue() const  {
            return value_;
        }

        const std::string getDefaultValue() const  {
            return defaultValue_;
        }

        const std::string getType() const  {
            return type_;
        }

    private:
        std::string name_;
        std::string value_;
        std::string defaultValue_;
        std::string type_;
};

} // end namespace ConfigTree

#endif // __CONFIG_CONFIG_H__
