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

#include "configurationtree.h"
#include "point.h"

#include <string>
using namespace std;

/**
 * Create a config-tree from file 'filename'
 */
Point::Point (const char* filename) {
	skinConfigTree = new ConfigurationTree();
	skinConfigTree->populateFromFile (filename);
}

Point::~Point (void) {
	delete skinConfigTree;
}

/**
 * Return the x-value of 'key'
 */
int
Point::getX (const char* key) {
	char * value = skinConfigTree->getValue(NULL, key);
	string tmp(value);
	int index = tmp.find(',');
	int toto = atoi((tmp.substr(0, index)).data());
	return toto;
}

/**
 * Return the y-value of 'key'
 */
int
Point::getY (const char* key) {
	char * value = skinConfigTree->getValue(NULL, key);
	string tmp(value);
	int index1, index2;

	index1 = tmp.find(',');
	if (tmp.find('-') == string::npos) {
		// If string tmp doesn't contain '-'
		return atoi((tmp.substr(index1 + 1, tmp.length() - index1)).data());
	} else {
		// If string tmp contains '-', it looks like 'y-variation'
		index2 = tmp.find('-');
		return atoi((tmp.substr(index1 + 1, index2 - index1)).data());
	}
}

/**
 * Return the variation-value of 'key' (for volume)
 */
int
Point::getVariation (const char* key) {
	char * value = skinConfigTree->getValue(NULL, key);
	string tmp(value);
	int index = tmp.find('-');
	return atoi((tmp.substr(index + 1, tmp.length() - index)).data());
}
