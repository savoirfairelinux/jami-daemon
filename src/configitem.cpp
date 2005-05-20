//
// (c) 2004 Jerome Oufella <jerome.oufella@savoirfairelinux.com>
// (c) 2004 Savoir-faire Linux inc.
//
//
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "configitem.h"

using namespace std;

ConfigItem::ConfigItem (void) {
	init();
}

// Create a new ConfigItem, with key name keyName
ConfigItem::ConfigItem (const string& keyName) {
	init();
	this->_key = new string(keyName);
}

// Create a new ConfigItem, with key name keyName and value keyVal.
ConfigItem::ConfigItem (const string& keyName, const string& keyVal) {
	init();
	this->_key = new string(keyName);
	this->_value = new string(keyVal);
}

ConfigItem::~ConfigItem (void) {
	if (this->_key != NULL) delete this->_key;
	if (this->_value != NULL) delete this->_value;
}


// Get the value of the passed key
string*
ConfigItem::getValueByKey (const string& keyName) {
	assert (_key != NULL);
	
	if (*_key == keyName) {
		return _value;
	} else if (_next) {
		return _next->getValueByKey(keyName);
	}

	return NULL;
}

// Get item pointer using a key value.
// If key value not found, new item is appended to list.
ConfigItem*
ConfigItem::getItemByKey (const string& keyName) {
	assert (_key != NULL);

	if (*_key == keyName) {
		return this;
	} else if (_next) {
		return _next->getItemByKey(keyName);
	}

	// Create new ConfigItem in list if non-existent
	_next = new ConfigItem(keyName);
	return _next;
}

// This saves a section/item list to fd
void
ConfigItem::saveToFile (fstream *fd) {
	if (fd == NULL) return;
	
	// If we're a section, save our contents first.
	if (_head != NULL) {
		if (_key != NULL) {
			*fd << "[" << _key->data() << "]\n";
		}
		_head->saveToFile (fd);
		*fd << "\n";
	}
	
	// Save the items list to file.
	if (_next != NULL) {
		_next->saveToFile(fd);
	}
	
	// Must save key=value only if not a section.
	if (_head == NULL) {
		*fd << _key->data() << "=" <<_value->data() << "\n";
	}
}

// Set the current objects value
void
ConfigItem::setValue (const string& newValue) {
	if (_value != NULL) {
		delete _value;
	}
	
	_value = new string(newValue);
}

// Set a value given its key name
void
ConfigItem::setValueByKey (const string& key, const string& value) {
	getItemByKey(key)->setValue(value);
}

// Set the default values
void
ConfigItem::init (void) {
	this->_key = NULL;
	this->_value = NULL;
	this->_next = NULL;
	this->_head = NULL;
}


// EOF
