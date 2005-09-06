/**
  *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
  *  Author: Jerome Oufella <jerome.oufella@savoirfairelinux.com>
  *                                                                               *  This program is free software; you can redistribute it and/or modify
  *  it under the terms of the GNU General Public License as published by
  *  the Free Software Foundation; either version 2 of the License, or
  *  (at your option) any later version.
  *                                                                               *  This program is distributed in the hope that it will be useful,
  *  but WITHOUT ANY WARRANTY; without even the implied warranty of
  *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  *  GNU General Public License for more details.
  *                                                                               *  You should have received a copy of the GNU General Public License
  *  along with this program; if not, write to the Free Software
  *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
  */

#include <iostream>
#include <string>

#include "global.h"
#include "configitem.h"
#include "configurationtree.h"

using namespace std;

// Default constructor
ConfigurationTree::ConfigurationTree (void) {
	this->_head = NULL;
}

// Construct with file name, and load from this file
ConfigurationTree::ConfigurationTree (const string& fileName) {
	populateFromFile (fileName);
}

// Destructor
ConfigurationTree::~ConfigurationTree (void) {
	if (_head != NULL) {
		delete _head;
	}
}

// Create the tree from an existing ini file
int
ConfigurationTree::populateFromFile (const string& fileName) {
	bool out = false;
	if (fileName.empty()) {
		return 0;
	}
	
	fstream file;
	file.open(fileName.data(), fstream::in);
	 
	if (!file.is_open()) {
		file.open(fileName.data(), fstream::out);
		out = true;
		if (!file.is_open()) {
		  _debug("(%s:%d) Error opening file: %s\n", __FILE__, __LINE__, 
			 fileName.c_str());
			return 0;
		}
    file.close();
		return 2;
  	}
	
	char line[128];
	bzero (line, 128);
	
	string section("");
	string key("");
	string val("");
	string s;
	int pos;
	
	while (!file.eof()) {
		// Read the file line by line
		file.getline (line, 128 - 1);
		string str(line);
		if (str[0] == '[') {
			// If the line is a section
			pos = str.find(']');
			section = str.substr(1, pos - 1);
		} else if (!str.empty() and str[0] != '#') {
			// If the line is "key=value" and doesn't begin with '#'(comments)
			pos = str.find('=');
			key = str.substr(0, pos);
			val = str.substr(pos + 1, str.length() - pos);
		
			if (key.length() > 0 and val.length() > 0) {
				setValue(section, key, val);
			} 
		}
	}

	file.close();
	return 1;
}

// Save tree's contents to a file
int
ConfigurationTree::saveToFile (const string& fileName) {
	if (fileName.empty() || _head == NULL) {
		return 0;
	}
	
	fstream file;
	file.open(fileName.data(), fstream::out);
	 
	if (!file.is_open()) {
	  _debug("(%s:%d) Error opening file: %s\n",
			 __FILE__, 
			 __LINE__, 
			 fileName.c_str());
		return 0;
  	}

	_head->saveToFile (&file);
	file.close();
	return 1;
}

// set [section]/key to int value
#define TMPBUFSIZE	32
int
ConfigurationTree::setValue (const string& section, const string& key, int value) {
	char tmpBuf[TMPBUFSIZE];

	// Make string from int
	bzero(tmpBuf, TMPBUFSIZE);
	snprintf (tmpBuf, TMPBUFSIZE - 1, "%d", (int) value);

	return setValue(section, key, tmpBuf);
}

// set [section]/key to "value"
int
ConfigurationTree::setValue (const string& section, const string& key,
							 const string& value) {

	if (_head != NULL) {
		ConfigSection *list;
		ConfigItem		*item;

		list = _head->getItemByKey(section);
		item = list->head();

		if (item == NULL) {
			// getItemByKey creates a new section if it does not exist.
			// If this is a new section, create its contents
			list->setHead(new ConfigItem(key,value));
		} else {
			// Section already exists, set 'key' to 'value' 
			item->setValueByKey(key,value);
		}
		
	} else {
		// Create the first section :
		// And its first item :
		_head = new ConfigItem (section);
		_head->setHead(new ConfigItem(key, value));
	}

	return 1;
}

// get [section]/key's value
string
ConfigurationTree::getValue (const string& section, const string& key) {
	if (_head != NULL) {
		string *valuePtr;
		if (_head->getItemByKey(section)->head() != 0) {
			// If config file exist
			valuePtr = _head->getItemByKey(section)->head()->getValueByKey(key);
		} else {
			// If config-file not exist
			valuePtr = _head->getItemByKey(section)->getValueByKey(key);
		}

		if (valuePtr != NULL) {
			return valuePtr->data();
		} else {
			return "";
		}
	} else {
		return "";
	}
}


// EOF
