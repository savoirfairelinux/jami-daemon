/*
 *  Copyright (C) 2004-2009 Savoir-Faire Linux inc.
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
 */

#include "logger.h"
#include <stdarg.h>
#include <string>
#include <stdio.h>

using namespace std;

namespace Logger
{

bool consoleLog = false;
bool debugMode = false;

void log(const int level, const char* format, ...)
{
	if(!debugMode && level == LOG_DEBUG)
		return;

	va_list ap;
	string prefix = "<> ";
	char buffer[2048];
	string message = "";
	string color_prefix = "";

	switch(level)
	{
		case LOG_ERR:
		{
			prefix = "<error> ";
			color_prefix = RED;
			break;
		}
		case LOG_WARNING:
		{
			prefix = "<warning> ";
			color_prefix = LIGHT_RED;
			break;
		}
		case LOG_INFO:
		{
			prefix = "<info> ";
			color_prefix = "";
			break;
		}
		case LOG_DEBUG:
		{
			prefix = "<debug> ";
			color_prefix = "";
			break;
		}
	}
	
	va_start(ap, format);
	vsprintf(buffer, format, ap);
	va_end(ap);

	message = buffer;
	message = prefix + message;

	syslog(level, message.c_str());

	if(consoleLog)
	{
		message = color_prefix + message + END_COLOR + "\n";
		fprintf(stderr, message.c_str());
	}
}

void setConsoleLog(bool c)
{
	Logger::consoleLog = c;
}

void setDebugMode(bool d)
{
	Logger::debugMode = d;
}

}

