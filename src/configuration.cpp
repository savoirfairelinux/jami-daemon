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

#include <qstring.h>
#include <qsettings.h>

#include "configuration.h"
#include "configurationtree.h"

static ConfigurationTree	*globalConfigTree = NULL;

#if 0
QString
Config::gets (QString key) {
	return Config::get (key, QString(""));
}

QString
Config::get	(QString key, QString defval) {
	QSettings settings;
	return settings.readEntry (QString(CFG_PFX) + QString("/") + key, defval);
}

int
Config::geti (QString key) {
	return Config::get (key, 0);
}

int
Config::get	(QString key, int defval) {
	QSettings settings;
	return settings.readNumEntry (QString(CFG_PFX) + QString("/") + key,defval);
}

bool
Config::getb (QString key) {
	return Config::get (key, false);
}

bool
Config::get	(QString key, bool defval) {
	QSettings settings;
	return settings.readBoolEntry (QString(CFG_PFX) + QString("/") + key,
			defval);
}

QString
Config::set	(QString key, QString val) {
	QSettings settings;
	settings.writeEntry (QString(CFG_PFX) + QString("/") + key, val);
	return val;
}

int
Config::set	(QString key, int val) {
	QSettings settings;
	settings.writeEntry (QString(CFG_PFX) + QString("/") + key, val);
	return val;
}

bool
Config::set	(QString key, bool val) {
	QSettings settings;
	settings.writeEntry (QString(CFG_PFX) + QString("/") + key, val);
	return val;
}
#endif
string
Config::gets (const char *section, const char *key) {
	return Config::get (section, key, "");
}

string
Config::get	(const char *section, const char *key, const char *defval) {
	char *value = tree()->getValue (section, key);
	if (value == NULL) {
		tree()->setValue(section, key, defval);
		return string(defval);
	} else {
		return string(value);
	}
}

char *
Config::getschar (const char *section, const char *key) {
	return Config::getchar (section, key, "");
}

char *
Config::getchar	(const char *section, const char *key, const char *defval) {
	char *value = tree()->getValue (section, key);
	if (value == NULL) {
		tree()->setValue(section, key, defval);
		return (char*)defval;
	} else {
		return value;
	}
}

bool
Config::getb (const char *section, const char *key) {
	return (bool)Config::get (section, key, 0);
}


int
Config::geti (const char *section, const char *key) {
	return Config::get (section, key, 0);
}

int
Config::get	(const char *section, const char * key, int defval) {
	char *value = tree()->getValue(section, key);
	if (value == NULL) {
		tree()->setValue(section, key, defval);
		return defval;
	} else {
		return atoi(value);
	} 
}

string
Config::set	(const char *section, const char *key, const char *val) {
	tree()->setValue(section, key, val);
	return string(val);
}

char *
Config::setchar	(const char *section, const char *key, const char *val) {
	tree()->setValue(section, key, val);
	return (char*)val;
}

int
Config::set	(const char *section, const char *key, int val) {
	tree()->setValue(section, key, val);
	return val;
}

bool
Config::set	(const char *section, const char *key, bool val) {
	tree()->setValue(section, key, (int)val);
	return val;
}


void
Config::setTree (ConfigurationTree *t) {
	globalConfigTree = t;
}

ConfigurationTree*
Config::tree(void) {
	return globalConfigTree;
}


// EOF
