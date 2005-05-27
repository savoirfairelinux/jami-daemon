/**
 *  Copyright (C) 2004 Savoir-Faire Linux inc.
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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

#include <string>

#include "configuration.h"
#include "configurationtree.h"

using namespace std;

static ConfigurationTree	*globalConfigTree = NULL;


string
Config::gets (const string& section, const string& key) {
	return Config::get (section, key, string(""));
}


string
Config::get	(const string& section, const string& key, const string& defval) {
	string value = tree()->getValue (section, key);
	if (value.empty()) {
		tree()->setValue(section, key, defval);
		return defval;
	} else {
		return value;
	}
}

string 
Config::getchar	(const string& section, const string& key, const string& defval){
	string value = tree()->getValue (section, key);
	if (value == string("")) {
		tree()->setValue(section, key, defval);
		return defval;
	} else {
		return value;
	}
}

int
Config::geti (const string& section, const string& key) {
	return Config::get (section, key, 0);
}

int
Config::get	(const string& section, const string&  key, int defval) {
	string value = tree()->getValue(section, key);
	if (value == string("")) {
		tree()->setValue(section, key, defval);
		return defval;
	} else {
		return atoi(value.data());
	} 
}

string
Config::set	(const string& section, const string& key, const string& val) {
	tree()->setValue(section, key, val);
	return val;
}

string 
Config::setchar	(const string& section, const string& key, const string& val) {
	tree()->setValue(section, key, val);
	return val;
}

int
Config::set	(const string& section, const string& key, int val) {
	tree()->setValue(section, key, val);
	return val;
}

bool
Config::set	(const string& section, const string& key, bool val) {
	tree()->setValue(section, key, (int)val);
	return val;
}


void
Config::setTree (ConfigurationTree *t) {
	globalConfigTree = t;
}

void
Config::deleteTree (void) {
	delete globalConfigTree;
}

ConfigurationTree*
Config::tree(void) {
	return globalConfigTree;
}


// EOF
