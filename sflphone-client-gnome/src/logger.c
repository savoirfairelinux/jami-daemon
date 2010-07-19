/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 *  Author: Julien Bonjean <julien.bonjean@savoirfairelinux.com> 
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

#include <logger.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

int log_level = LOG_INFO; 

void internal_log(const int level, const char* format, ...)
{
	if(level > log_level)
		return;

        va_list ap;
        char *prefix = "<> ";
        char buffer[4000];
	char message[4096];

	switch(level)
        {
                case LOG_ERR:
                {
                        prefix = "<error> ";
                        break;
                }
                case LOG_WARN:
                {
                        prefix = "<warning> ";
                        break;
                }
                case LOG_INFO:
                {
                        prefix = "<info> ";
                        break;
                }
                case LOG_DEBUG:
                {
                        prefix = "<debug> ";
                        break;
                }
        }

        va_start(ap, format);
        vsprintf(buffer, format, ap);
        va_end(ap);

	message[0] = '\0';
	strncat(message, prefix, strlen(prefix));
	strncat(message, buffer, strlen(buffer));
	strncat(message, "\n", 1);

        fprintf(stderr, "%s", message);
}

void set_log_level(const int level)
{
	log_level = level;
}
