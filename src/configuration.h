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

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <string>

#include "global.h"

using namespace std;

class ConfigurationTree;
class Config {
public:
	Config					(void) {};
	~Config					(void) {};

	static string	gets	(const string&, const string&);
//	static string	getschar(const string&, const string&);
	static int		geti	(const string&, const string&);
//	static bool		getb	(const string&, const string&);
	
	static string	get		(const string&, const string&, const string&);	
	static string 	getchar	(const string&, const string&, const string&);	
	static int		get		(const string&, const string&, int);
	static bool		get		(const string&, const string&, bool);
	
	static int		set		(const string&, const string&, int);
	static bool		set		(const string&, const string&, bool);
	static string	set		(const string&, const string&, const string&);
	static string 	setchar	(const string&, const string&, const string&);

	static void		setTree (ConfigurationTree *);
	static void		deleteTree (void);
	static ConfigurationTree*	tree (void);
};

#endif // __CONFIG_H__
