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

#include <qsettings.h>
#include <qstring.h>

#include "global.h"

#include <string>
using namespace std;

class ConfigurationTree;
class Config {
public:
	Config					(void) {};
	~Config					(void) {};

	static std::string	 gets	(const char*, const char*);
	static char* getschar	(const char*, const char*);
	static int		 geti	(const char*, const char*);
	static bool		 getb	(const char*, const char*);
	
	static std::string	 get	(const char*, const char*, const char*);	
	static char* getchar	(const char*, const char*, const char*);	
	static int		 get	(const char*, const char*, int);
	static bool		 get	(const char*, const char*, bool);
	
	static int		 set	(const char*, const char*, int);
	static bool		 set	(const char*, const char*, bool);
	static std::string	 set	(const char*, const char*, const char*);
	static char* setchar	(const char*, const char*, const char*);

	static void		 setTree (ConfigurationTree *);
	static ConfigurationTree*	tree (void);
};

#endif // __CONFIG_H__
