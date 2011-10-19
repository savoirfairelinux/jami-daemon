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

#include "config.h"
#include "../global.h"
#include <fstream>
#include <cstdlib>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <iostream>
#include <string.h>
#include "yamlparser.h"

namespace Conf {

ConfigTree::~ConfigTree()
{
    // erase every new ItemMap (by CreateSection)
    SectionMap::iterator iter = sections_.begin();

    while (iter != sections_.end()) {
        delete iter->second;
        iter->second = NULL;
        iter++;
    }
}

void ConfigTree::addDefaultValue(const std::pair<std::string, std::string>& token, std::string section)
{
    defaultValueMap_.insert(token);

    if (not section.empty())
        addConfigTreeItem(section, ConfigTreeItem(token.first, token.second, token.second, "string"));
}

std::string ConfigTree::getDefaultValue(const std::string& key) const
{
    std::map<std::string, std::string>::const_iterator it;
    it = defaultValueMap_.find(key);

    if (it == defaultValueMap_.end())
        return "";

    return it->second;
}

/**
 * Create the section only if it doesn't exists
 */
void
ConfigTree::createSection(const std::string& section)
{
    // if we doesn't find the item, create it
    if (sections_.find(section) == sections_.end())
        sections_[section] = new ItemMap;
}

/**
 * Remove the section only if it exists
 */
void
ConfigTree::removeSection(const std::string& section)
{
    // if we doesn't find the item, create it
    SectionMap::iterator iter = sections_.find(section);

    if (iter != sections_.end())
        sections_.erase(iter);
}

/** Retrieve the sections as an array */
TokenList
ConfigTree::getSections()
{
    TokenList sections;

    SectionMap::iterator iter = sections_.begin();

    while (iter != sections_.end()) {
        // add to token list the: iter->second;
        sections.push_back(iter->first);
        iter++;
    }

    return sections;
}

/**
 * Add the config item only if it exists..
 * If the section doesn't exists, create it
 */
void
ConfigTree::addConfigTreeItem(const std::string& section, const ConfigTreeItem item)
{
    // if we doesn't find the item, create it
    SectionMap::iterator iter = sections_.find(section);

    if (iter == sections_.end()) {
        sections_[section] = new ItemMap;
        iter = sections_.find(section);
    }

    // be prudent here
    if (iter != sections_.end()) {
        std::string name(item.getName());

        if (iter->second->find(name) == iter->second->end()) {
            (*(iter->second))[name] = item;
        }
    }
}

std::string
ConfigTree::getConfigTreeItemValue(const std::string& section, const std::string& itemName) const
{
    const ConfigTreeItem* item = getConfigTreeItem(section, itemName);

    if (item)
        return item->getValue();

    return getDefaultValue(itemName);
}

// throw a ConfigTreeItemException if not found
int
ConfigTree::getConfigTreeItemIntValue(const std::string& section, const std::string& itemName) const
{
    std::string configItem = getConfigTreeItemValue(section, itemName);
    int retval = atoi(configItem.data());

    return retval;
}

bool
ConfigTree::getConfigTreeItemBoolValue(const std::string& section, const std::string& itemName) const
{
    std::string configItem = getConfigTreeItemValue(section, itemName);

    if (configItem == "true") {
        return true;
    }

    return false;
}

bool
ConfigTree::getConfigTreeItemToken(const std::string& section, const std::string& itemName, TokenList& arg) const
{
    const ConfigTreeItem *item = getConfigTreeItem(section, itemName);

    if (item) {
        arg.clear();
        arg.push_back(section);
        arg.push_back(itemName);
        arg.push_back(item->getType());
        arg.push_back(item->getValue());
        arg.push_back(item->getDefaultValue());
        return true;
    }

    return false;
}

/**
 * Return a ConfigTreeItem or NULL if not found
 */
const ConfigTreeItem*
ConfigTree::getConfigTreeItem(const std::string& section, const std::string& itemName) const
{
    SectionMap::const_iterator iter = sections_.find(section);

    if (iter == sections_.end())
        return NULL;

    ItemMap::const_iterator iterItem = iter->second->find(itemName);

    if (iterItem == iter->second->end())
        return NULL;

    return & (iterItem->second);
}

/**
 * Set the configItem if found, if not, *CREATE IT*
 *
 * @todo Élimier les 45,000 classes qui servent à rien pour Conf.
 * The true/false logic is useless here.
 */
bool
ConfigTree::setConfigTreeItem(const std::string& section,
                              const std::string& itemName,
                              const std::string& value)
{

    SectionMap::iterator iter = sections_.find(section);

    if (iter == sections_.end()) {
        // Not found, create section
        sections_[section] = new ItemMap;
        iter = sections_.find(section);
    }

    ItemMap::iterator iterItem = iter->second->find(itemName);

    if (iterItem == iter->second->end()) {
        // If not found, search in our default list to find
        // something that would fit.
        std::string defaultValue = getDefaultValue(itemName);
        addConfigTreeItem(section, ConfigTreeItem(itemName, value, defaultValue));
        return true;
    }

    // Use default value if the value is empty.
    if (value.empty() == true) {
        iterItem->second.setValue(getDefaultValue(itemName));
        return true;
    }

    iterItem->second.setValue(value);

    return true;
}

// Save config to a file (ini format)
// return false if empty, no config, or enable to open
// return true if everything is ok
bool
ConfigTree::saveConfigTree(const std::string& fileName)
{
    DEBUG("ConfigTree: Save %s", fileName.c_str());

    if (fileName.empty() and sections_.begin() == sections_.end())
        return false;

    std::fstream file;

    file.open(fileName.data(), std::fstream::out);

    if (!file.is_open()) {
        ERROR("ConfigTree: Error: Could not open %s configuration file", fileName.c_str());
        return false;
    }

    // for each section, for each item...
    SectionMap::iterator iter = sections_.begin();

    while (iter != sections_.end()) {
        file << "[" << iter->first << "]" << std::endl;
        ItemMap::iterator iterItem = iter->second->begin();

        while (iterItem != iter->second->end()) {
            file << iterItem->first << "=" << iterItem->second.getValue() << std::endl;
            iterItem++;
        }

        file << std::endl;

        iter++;
    }

    file.close();

    if (chmod(fileName.c_str(), S_IRUSR | S_IWUSR))
        ERROR("ConfigTree: Error: Failed to set permission on configuration: %m");

    return true;
}

// Create the tree from an existing ini file
// 0 = error
// 1 = OK
// 2 = unable to open
int
ConfigTree::populateFromFile(const std::string& fileName)
{
    DEBUG("ConfigTree: Populate from file %s", fileName.c_str());

    if (fileName.empty())
        return 0;

    std::fstream file;

    file.open(fileName.data(), std::fstream::in);

    if (!file.is_open()) {
        file.open(fileName.data(), std::fstream::out);

        if (!file.is_open())
            return 0;

        file.close();

        return 2;
    }

    // get length of file:
    file.seekg(0, std::ios::end);

    int length = file.tellg();

    file.seekg(0, std::ios::beg);

    if (length == 0) {
        file.close();
        return 2; // should load config
    }

    std::string line;

    std::string section("");
    std::string key("");
    std::string val("");
    std::string::size_type pos;

    while (!file.eof()) {
        // Read the file line by line
        std::getline(file, line);

        if (!line.empty()) {
            if (line[0] == '[') {
                // If the line is a section
                pos = line.find(']');
                section = line.substr(1, pos - 1);
            } else if (line[0] != '#') {
                // If the line is "key=value" and doesn't begin with '#'(comments)

                pos = line.find('=');
                key = line.substr(0, pos);
                val = line.substr(pos + 1, line.length() - pos);

                if (key.length() > 0 && val.length() > 0)
                    setConfigTreeItem(section, key, val);
            }
        }
    }

    file.close();

    if (chmod(fileName.c_str(), S_IRUSR | S_IWUSR))
        DEBUG("Failed to set permission on configuration file because: %m");

    return 1;
}

TokenList
ConfigTreeIterator::begin()
{
    TokenList tk;
    iter_ = tree_->sections_.begin();

    if (iter_!=tree_->sections_.end()) {
        iterItem_ = iter_->second->begin();

        if (iterItem_!=iter_->second->end()) {
            tk.push_back(iter_->first);
            tk.push_back(iterItem_->first);
            tk.push_back(iterItem_->second.getType());
            tk.push_back(iterItem_->second.getValue());
            tk.push_back(iterItem_->second.getDefaultValue());
        }
    }

    return tk;
}

TokenList
ConfigTreeIterator::next()
{
    TokenList tk;
    // we return tk empty if we are at the end of the list...

    if (iter_ == tree_->sections_.end())
        return tk;

    if (iterItem_ != iter_->second->end())
        iterItem_++;

    if (iterItem_ == iter_->second->end()) {
        // if we increment, and we are at the end of a section
        iter_++;

        if (iter_ != tree_->sections_.end()) {
            iterItem_ = iter_->second->begin();

            if (iterItem_ != iter_->second->end()) {
                tk.push_back(iter_->first);
                tk.push_back(iterItem_->first);
                tk.push_back(iterItem_->second.getType());
                tk.push_back(iterItem_->second.getValue());
                tk.push_back(iterItem_->second.getDefaultValue());
            }
        }
    } else {
        tk.push_back(iter_->first);
        tk.push_back(iterItem_->first);
        tk.push_back(iterItem_->second.getType());
        tk.push_back(iterItem_->second.getValue());
        tk.push_back(iterItem_->second.getDefaultValue());
    }

    return tk;
}
} // end namespace ConfigTree

