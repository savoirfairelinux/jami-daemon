//
// (c) 2004 Savoir-faire Linux inc.
// Author: Jerome Oufella <jerome.oufella@savoirfairelinux.com>
//
//
#ifndef __CONFIG_ITEM_H__
#define __CONFIG_ITEM_H__

#include <iostream>
#include <fstream>

#include <cc++/string.h>
#include <stdio.h>

using namespace ost;
using namespace std;

class ConfigItem {
public:
	ConfigItem  (void);
	ConfigItem  (const char *);
	ConfigItem  (const char *, const char *);
	~ConfigItem (void);
	String*		key (void) { return _key; }
	String*		value (void) { return _value; }
	ConfigItem* head (void) { return _head; }
	void		setHead (ConfigItem *h) { _head = h; }
	String*		getValueByKey   (const char *);
	ConfigItem* getItemByKey	(const char *);
	void		setValue		(const char *);
	void		setValueByKey   (const char *, const char *);
	void		saveToFile		(fstream*);
	
private:
	String*		_key;
	String*		_value;
	ConfigItem* _next;
	ConfigItem* _head;
	void init   (void);
};

typedef ConfigItem ConfigSection;

#endif



