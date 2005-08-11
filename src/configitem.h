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



