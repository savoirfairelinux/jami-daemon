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

// EOF
