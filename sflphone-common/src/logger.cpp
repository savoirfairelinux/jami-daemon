#include "logger.h"
#include <syslog.h>
#include <stdarg.h>

void Logger::log(const int level, const char* format, ...)
{
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

	message = color_prefix + message + END_COLOR + "\n";
	fprintf(stderr, message.c_str());
}

