//
// (c) 2004 Jerome Oufella <jerome.oufella@savoirfairelinux.com>
// (c) 2004 Savoir-faire Linux inc.
//
//
#ifndef __CONFIG_ITEM_H__
#define __CONFIG_ITEM_H__

#include <iostream>
#include <fstream>

#include <string>
#include <stdio.h>

using namespace std;

class ConfigItem {
public:
	ConfigItem  (void);
	ConfigItem  (const string& );
	ConfigItem  (const string& , const string& );
	~ConfigItem (void);
	string*		key (void) { return _key; }
	string*		value (void) { return _value; }
	ConfigItem* head (void) { return _head; }
	void		setHead (ConfigItem *h) { _head = h; }
	string*		getValueByKey   (const string& );
	ConfigItem* getItemByKey	(const string& );
	void		setValue		(const string& );
	void		setValueByKey   (const string& , const string& );
	void		saveToFile		(fstream*);
	
private:
	string*		_key;
	string*		_value;
	ConfigItem* _next;
	ConfigItem* _head;
	void init   (void);
};

typedef ConfigItem ConfigSection;

#endif



