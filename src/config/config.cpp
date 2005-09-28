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
#include <fstream>

namespace Conf {

// ctor
ConfigTree::ConfigTree() 
{
}

// dtor
ConfigTree::~ConfigTree() 
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
ConfigTree::createSection(const std::string& section) {
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
ConfigTree::addConfigTreeItem(const std::string& section, const ConfigTreeItem item) 
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

// throw a ConfigTreeItemException if not found
std::string 
ConfigTree::getConfigTreeItemValue(const std::string& section, const std::string& itemName) 
{
  ConfigTreeItem* item = getConfigTreeItem(section, itemName);
  if (item!=NULL) {
    return item->getValue();
  } else {
    throw new ConfigTreeItemException();
  }
  return "";
}

// throw a ConfigTreeItemException if not found
int 
ConfigTree::getConfigTreeItemIntValue(const std::string& section, const std::string& itemName) 
{
  ConfigTreeItem* item = getConfigTreeItem(section, itemName);
  if (item!=NULL && item->getType() == "int") {
    return atoi(item->getValue().data());
  } else {
    throw new ConfigTreeItemException();
  }
  return 0;
}

bool
ConfigTree::getConfigTreeItemToken(const std::string& section, const std::string& itemName, TokenList& arg) {
  ConfigTreeItem *item = getConfigTreeItem(section, itemName);
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
ConfigTreeItem* 
ConfigTree::getConfigTreeItem(const std::string& section, const std::string& itemName) {
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
bool 
ConfigTree::setConfigTreeItem(const std::string& section, const std::string& itemName, const std::string& value) {
  SectionMap::iterator iter = _sections.find(section);
  if ( iter == _sections.end()) {
    return false;
  }
  ItemMap::iterator iterItem = iter->second->find(itemName);
  if ( iterItem == iter->second->end()) {
    return false;
  }
  iterItem->second.setValue(value);
  return true;
}

// Save config to a file (ini format)
bool 
ConfigTree::saveConfigTree(const std::string& fileName) {
  if (fileName.empty() && _sections.begin() != _sections.end() ) {
    return false;
  }

  std::fstream file;
  file.open(fileName.data(), std::fstream::out);

  if (!file.is_open()) {
    return false;
  }

  // for each section, for each item...
  SectionMap::iterator iter = _sections.begin();
  while(iter != _sections.end()) {
    file << "[" << iter->first << "]" << std::endl;
    ItemMap::iterator iterItem = iter->second->begin();
    while ( iterItem == iter->second->end() ) {
      file << iterItem->first << iterItem->second.getValue() << std::endl;
      iterItem++;
    }
    file << std::endl;

    iter++;
  }

  file.close();
  return false;
}

// Create the tree from an existing ini file
// 0 = error
// 1 = OK
// 2 = unable to open
int
ConfigTree::populateFromFile(const std::string& fileName) {
  bool out = false;
  if (fileName.empty()) {
    return 0;
  }

  std::fstream file;
  file.open(fileName.data(), std::fstream::in);

  if (!file.is_open()) {
    file.open(fileName.data(), std::fstream::out);
    out = true;
    if (!file.is_open()) {
      return 0;
    }
    file.close();
    return 2;
  }

  std::string line;
  std::string section("");
  std::string key("");
  std::string val("");
  int pos;

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

        if (key.length() > 0 && val.length() > 0) {
          setConfigTreeItem(section, key, val);
        }
      }
    }
  }
  
  file.close();
  return 1;
}

TokenList
ConfigTreeIterator::begin()
{
  TokenList tk;
  _iter = _tree->_sections.begin();
  if (_iter!=_tree->_sections.end()) {
    _iterItem = _iter->second->begin();
    if (_iterItem!=_iter->second->end()) {
      tk.push_back(_iter->first);
      tk.push_back(_iterItem->first);
      tk.push_back(_iterItem->second.getType());
      tk.push_back(_iterItem->second.getValue());
      tk.push_back(_iterItem->second.getDefaultValue());
    }
  }
  return tk;
}

TokenList
ConfigTreeIterator::next()
{
  TokenList tk;
  // we return tk empty if we are at the end of the list...
  if (_iter==_tree->_sections.end()) {
    return tk;
  }
  if (_iterItem!=_iter->second->end()) {
    _iterItem++;
  }
  if (_iterItem==_iter->second->end()) {
    // if we increment, and we are at the end of a section
    _iter++;
    if (_iter!=_tree->_sections.end()) {
      _iterItem = _iter->second->begin();
      if (_iterItem!=_iter->second->end()) {
        tk.push_back(_iter->first);
        tk.push_back(_iterItem->first);
        tk.push_back(_iterItem->second.getType());
        tk.push_back(_iterItem->second.getValue());
        tk.push_back(_iterItem->second.getDefaultValue());
      }
    }
  } else {
    tk.push_back(_iter->first);
    tk.push_back(_iterItem->first);
    tk.push_back(_iterItem->second.getType());
    tk.push_back(_iterItem->second.getValue());
    tk.push_back(_iterItem->second.getDefaultValue());
  }
  return tk;
}

} // end namespace ConfigTree


