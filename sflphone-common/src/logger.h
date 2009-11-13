#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <string>
#include <syslog.h>

using namespace std;

namespace Logger
{
	void log(const int, const char*, ...);
};

#define _error(...)	Logger::log(LOG_ERROR, __VA_ARGS__)
#define _warn(...)	Logger::log(LOG_WARNING, __VA_ARGS__)
#define _info(...)	Logger::log(LOG_INFO, __VA_ARGS__)
#define _debug(...)	Logger::log(LOG_DEBUG, __VA_ARGS__)

#define _debugException(...)
#define _debugInit(...)
#define _debugAlsa(...)

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

