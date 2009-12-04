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

#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <syslog.h>

namespace Logger
{
	void log(const int, const char*, ...);

	void setConsoleLog(bool);
	void setDebugMode(bool);
};

#define _error(...)	Logger::log(LOG_ERR, __VA_ARGS__)
#define _warn(...)	Logger::log(LOG_WARNING, __VA_ARGS__)
#define _info(...)	Logger::log(LOG_INFO, __VA_ARGS__)
#define _debug(...)	Logger::log(LOG_DEBUG, __VA_ARGS__)

#define _debugException(...)	Logger::log(LOG_DEBUG, __VA_ARGS__)
#define _debugInit(...)		Logger::log(LOG_DEBUG, __VA_ARGS__)
#define _debugAlsa(...)		Logger::log(LOG_DEBUG, __VA_ARGS__)

#define BLACK "\033[22;30m"
#define RED "\033[22;31m"
#define GREEN "\033[22;32m"
#define BROWN "\033[22;33m"
#define BLUE "\033[22;34m"
#define MAGENTA "\033[22;35m"
#define CYAN "\033[22;36m"
#define GREY "\033[22;37m"
#define DARK_GREY "\033[01;30m"
#define LIGHT_RED "\033[01;31m"
#define LIGHT_SCREEN "\033[01;32m"
#define YELLOW "\033[01;33m"
#define LIGHT_BLUE "\033[01;34m"
#define LIGHT_MAGENTA "\033[01;35m"
#define LIGHT_CYAN "\033[01;36m"
#define WHITE "\033[01;37m"
#define END_COLOR "\033[0m"

#endif

