//
// (c) 2004 Jerome Oufella <jerome.oufella@savoirfairelinux.com>
// (c) 2004 Savoir-faire Linux inc.
//
//

#include <iostream>
#include <string>

#include "configitem.h"
#include "configurationtree.h"

using namespace std;

// Default constructor
ConfigurationTree::ConfigurationTree (void) {
	this->_head = NULL;
}

// Construct with file name, and load from this file
ConfigurationTree::ConfigurationTree (const char *fileName) {
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
ConfigurationTree::populateFromFile (const char *fileName) {
	if (fileName == NULL) {
		printf("Filename is NULL\n");
		return 0;
	}
	
	fstream file;
	file.open(fileName, fstream::in);
	 
	if (!file.is_open()) {
        printf("\nConfig-file is creating ...\n");
		return 0;
  	}
	
	char line[128];
	bzero (line, 128);
	
	const char* section = NULL;
	const char* key;
	const char* val;
	String s;
	
	while (!file.eof()) {
		// Read the file line by line
		file.getline (line, 128 - 1);
		String* str = new String(line);
		if (*str[0] == '[') {
			// If the line is a section
			s = str->token("[",1);
			s.trim("]");
			section = s.data();
	
		} else if (*str != NULL and *str[0] != '#') {
			// If the line is "key=value" and doesn't begin with '#'(comments)
			String k = str->token("=", 0);
			key = k.data();
			String v = str->token("=", 0);
			val = v.data();
			if (String(key).length() > 0 and String(val).length() > 0) {
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
        printf("\nError opening file\n");
		return 0;
  	}

	_head->saveToFile (&file);
	
	file.close();
	return 1;
}

// set [section]/key to int value
#define TMPBUFSIZE	32
int
ConfigurationTree::setValue (const char *section, const char *key, int value) {
	char tmpBuf[TMPBUFSIZE];

	// Make string from int
	bzero(tmpBuf, TMPBUFSIZE);
	snprintf (tmpBuf, TMPBUFSIZE - 1, "%d", (int) value);

	return setValue(section, key, tmpBuf);
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
//	printf ("getValue(%s,%s)\n", section, key);
	if (_head != NULL) {
		String *valuePtr;
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
			return NULL;
		}
	} else {
		return (char *) NULL;
	}
}


// EOF
