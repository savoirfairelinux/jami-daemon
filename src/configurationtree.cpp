//
// (c) 2004 Savoir-faire Linux inc.
// Author: Jerome Oufella <jerome.oufella@savoirfairelinux.com>
//
//

#include <iostream>
#include <fstream>

#include "configitem.h"
#include "configurationtree.h"

using namespace std;

// Default constructor
ConfigurationTree::ConfigurationTree (void) {
	this->_head = NULL;
}

// Construct with file name, and load from this file
ConfigurationTree::ConfigurationTree (const char *fileName) {
	createFromFile (fileName);
}

// Destructor
ConfigurationTree::~ConfigurationTree (void) {
	if (_head != NULL) {
		delete _head;
	}
}

// Create the tree from an existing ini file
int
ConfigurationTree::createFromFile (const char *fileName) {
	if (fileName == NULL) {
		return 0;
	}
	
	fstream file;
	file.open(fileName, fstream::in);
	 
	if (!file.is_open()) {
        cout << "Error opening file";
		return 0;
  	}

	char line[128];
	bzero (line, 128);
	
	const char* section;
	const char* key;
	const char* val;
	
	while (!file.eof()) {
		// Read the file line by line
		file.getline (line, 128 - 1);
		String* str = new String(line);
		
		if (*str[0] == '[') {
			// If the line is a section
			section = (str->token("]", 1)).data();
			if (section != NULL) {
				_head->setValue(section);
			}
		} else if (*str != NULL){
			// If the line is "key=value"
			key = (str->token("=", 0)).data();
			val = (str->token("=", String(key).length() + 2)).data();
			if (key != NULL and val != NULL) {
				setValue(section, key, val);
			}
		}
		delete str;
	}
	
	file.close();
	return 1;
}

// Save tree's contents to a file
int
ConfigurationTree::saveToFile (const char *fileName) {
	if (fileName == NULL || _head == NULL) {
		return 0;
	}
	
	fstream file;
	file.open(fileName, fstream::out);
	 
	if (!file.is_open()) {
        cout << "Error opening file";
		return 0;
  	}

	_head->saveToFile (&file);
	
	file.close();
	return 1;
}

// set [section]/key to "value"
int
ConfigurationTree::setValue (const char *section, const char *key,
							 const char *value) {
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
char *
ConfigurationTree::getValue (const char *section, const char *key) {
	if (_head != NULL) {
		return _head->getItemByKey(section)->getValueByKey(key)->data();
	} else {
		return (char *) NULL;
	}
}


// EOF
