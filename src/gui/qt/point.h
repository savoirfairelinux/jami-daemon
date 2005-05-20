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

#ifndef __POINT_H__
#define __POINT_H__

#include <string>
using namespace std;

#define NO_DIRECTION	0
#define HORIZONTAL		1
#define VERTICAL		2

class ConfigurationTree;

class Point {
public:
	Point (const string&);
	~Point (void);

	int	getX			(const char*);
	int	getY			(const char*);
	int	getVariation	(const char*);
	int getDirection	(const char*);	
	
private:
	ConfigurationTree* 	skinConfigTree;
	string 	getSubstrX	(const char*);	
	string 	getSubstrY	(const char*);	
};

#endif // __POINT_H__
