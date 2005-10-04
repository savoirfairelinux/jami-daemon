/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
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

#include "point.h"

#include <string>
#include "../../skin.h"
#include <iostream>
using namespace std;

/**
 * Create a config-tree from file 'filename'
 */
Point::Point (const std::string& filename) {
	int opened = 1;
  std::string s = "SKIN";
  using namespace Conf;
  _config.addConfigTreeItem(s, ConfigTreeItem(LINE1, "", ""));
  _config.addConfigTreeItem(s, ConfigTreeItem(LINE2, "", ""));
  _config.addConfigTreeItem(s, ConfigTreeItem(LINE3, "", ""));
  _config.addConfigTreeItem(s, ConfigTreeItem(LINE4, "", ""));
  _config.addConfigTreeItem(s, ConfigTreeItem(LINE5, "", ""));
  _config.addConfigTreeItem(s, ConfigTreeItem(LINE6, "", ""));

  _config.addConfigTreeItem(s, ConfigTreeItem(VOICEMAIL, "", ""));
  _config.addConfigTreeItem(s, ConfigTreeItem(DIRECTORY, "", ""));
  _config.addConfigTreeItem(s, ConfigTreeItem(CONFERENCE, "", ""));
  _config.addConfigTreeItem(s, ConfigTreeItem(TRANSFER, "", ""));
  _config.addConfigTreeItem(s, ConfigTreeItem(CLOSE, "", ""));
  _config.addConfigTreeItem(s, ConfigTreeItem(MINIMIZE, "", ""));
  _config.addConfigTreeItem(s, ConfigTreeItem(SETUP, "", ""));
  _config.addConfigTreeItem(s, ConfigTreeItem(HANGUP, "", ""));
  _config.addConfigTreeItem(s, ConfigTreeItem(CONNECT, "", ""));
  _config.addConfigTreeItem(s, ConfigTreeItem(MUTE, "", ""));
  _config.addConfigTreeItem(s, ConfigTreeItem(DTMF_SHOW, "", ""));
  _config.addConfigTreeItem(s, ConfigTreeItem(VOLUME, "", ""));
  _config.addConfigTreeItem(s, ConfigTreeItem(VOL_MIC, "", ""));
  _config.addConfigTreeItem(s, ConfigTreeItem(VOL_SPKR, "", ""));
  _config.addConfigTreeItem(s, ConfigTreeItem(SCREEN, "", ""));

  _config.addConfigTreeItem(s, ConfigTreeItem(DTMF_0, "", ""));
  _config.addConfigTreeItem(s, ConfigTreeItem(DTMF_1, "", ""));
  _config.addConfigTreeItem(s, ConfigTreeItem(DTMF_2, "", ""));
  _config.addConfigTreeItem(s, ConfigTreeItem(DTMF_3, "", ""));
  _config.addConfigTreeItem(s, ConfigTreeItem(DTMF_4, "", ""));
  _config.addConfigTreeItem(s, ConfigTreeItem(DTMF_5, "", ""));
  _config.addConfigTreeItem(s, ConfigTreeItem(DTMF_6, "", ""));
  _config.addConfigTreeItem(s, ConfigTreeItem(DTMF_7, "", ""));
  _config.addConfigTreeItem(s, ConfigTreeItem(DTMF_8, "", ""));
  _config.addConfigTreeItem(s, ConfigTreeItem(DTMF_9, "", ""));
  _config.addConfigTreeItem(s, ConfigTreeItem(DTMF_STAR, "", ""));
  _config.addConfigTreeItem(s, ConfigTreeItem(DTMF_POUND, "", ""));
  _config.addConfigTreeItem(s, ConfigTreeItem(DTMF_CLOSE, "", ""));

	opened = _config.populateFromFile (filename);
	if (opened != 1) {
	// If opening failed, stop the application
    std::cerr << "Fatal Error: Unable to open skin configuration file\n";
		exit(0);
	}
}

Point::~Point (void) {
}


/**
 * Extract the substring before the comma
 */
string
Point::getSubstrX (const char* key) {
	std::string value = _config.getConfigTreeItemValue("SKIN", string(key));
	int index = value.find(',');
	return value.substr(0, index);
}

/**
 * Extract the substring after the comma
 */
string
Point::getSubstrY (const char* key) {
	string value = _config.getConfigTreeItemValue("SKIN", string(key));
	int index = value.find(',');
	return value.substr(index + 1, value.length() - index);
}

/**
 * Return the x-value of 'key'
 */
int
Point::getX (const char* key) {
	int index;
	string tmp = getSubstrX(key);
	
	if (getDirection(key) == HORIZONTAL) {
		index = tmp.find('-');
		return atoi((tmp.substr(0, tmp.length() - index)).data());
	} else {
		return atoi(tmp.data());
	}
}

/**
 * Return the y-value of 'key'
 */
int
Point::getY (const char* key) {
	int index;
	string tmp = getSubstrY(key);
	
	if (getDirection(key) == VERTICAL) {
		index = tmp.find('-');
		return atoi((tmp.substr(0, tmp.length() - index)).data());
	} else {
		return atoi(tmp.data());
	}
}

/**
 * Return the variation-value of 'key' (for volume)
 */
int
Point::getVariation (const char* key) {
	int index;
	string str;
	
	if (getDirection(key) == HORIZONTAL) {
		str = getSubstrX(key);
	} else if (getDirection(key) == VERTICAL) {
		str = getSubstrY(key);
	} 
	index = str.find('-');
	return atoi((str.substr(index + 1, str.length() - index)).data());
}


/**
 * Get the direction of the variation for 'key' 
 *
 * @return	1 -> horizontal or  2 -> vertical
 *			(0 if no variation)
 */
int
Point::getDirection (const char* key) {
	if (getSubstrX(key).find('-') != string::npos) {
		return HORIZONTAL;
	} else if (getSubstrY(key).find('-') != string::npos) {
		return VERTICAL;
	} else {
		return NO_DIRECTION;
	}
}
