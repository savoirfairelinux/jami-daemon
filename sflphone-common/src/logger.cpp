#include "logger.h"
#include <syslog.h>
#include <stdarg.h>

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
	char buffer[1024];
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
			color_prefix = GREEN;
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

