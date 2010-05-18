/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include "urlhook.h"
#include <iostream>
#include <vector>

UrlHook::UrlHook () { }

UrlHook::~UrlHook () { }

int UrlHook::addAction (std::string field_value, std::string command)
{

    std::string command_bg;
	std::string temp;
	std::vector <std::string> args;
	size_t pos;
	unsigned int i;

	/* Escape the '&' char to not discard $_GET parameters in the URL - #2659 */ 
	while ( (pos = field_value.find ("&", 0)) != std::string::npos) {
        temp = field_value.substr (0, pos);
        field_value.erase (0, pos + 1);
		args.push_back (temp);
		std::cout << temp << " " << std::endl;	
	}

    command_bg = command + " ";

	pos = args.size ();
	for (i=0; i<pos; i++) {
		// Escape the "&"
		command_bg += args[i] + "\\&";
	}

	// Retrieve the last argument
	command_bg +=  field_value;

    /* Execute the command in the background to not block the application */
	command_bg += "&";

    /* Execute a system call */
    return RUN_COMMAND (command_bg.c_str());

}


