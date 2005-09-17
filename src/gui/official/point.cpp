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

#include "../../configurationtree.h"
#include "point.h"

#include <string>
using namespace std;

/**
 * Create a config-tree from file 'filename'
 */
Point::Point (const string& filename) {
	int opened = 1;
	skinConfigTree = new ConfigurationTree();
	opened = skinConfigTree->populateFromFile (filename);
	if (opened != 1) {
	// If opening failed, stop the application
		exit(0);
	}
}

Point::~Point (void) {
	if (skinConfigTree != NULL) {
		delete skinConfigTree;
		skinConfigTree = NULL;
	}
}


/**
 * Extract the substring before the comma
 */
string
Point::getSubstrX (const char* key) {
	string value = skinConfigTree->getValue("", string(key));
	int index = value.find(',');
	return value.substr(0, index);
}

/**
 * Extract the substring after the comma
 */
string
Point::getSubstrY (const char* key) {
	string value = skinConfigTree->getValue("", string(key));
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
