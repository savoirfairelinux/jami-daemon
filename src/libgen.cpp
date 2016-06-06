#include <libgen.h>

char *basename(char * path)
{
	int i;

	if (path == NULL || path[0] == '\0')
		return "";
	for (i = strlen(path) - 1; i >= 0 && path[i] == '/'; i--);
	if (i == -1)
		return "/";
	for (path[i + 1] = '\0'; i >= 0 && path[i] != '/'; i--);
	return &path[i + 1];
}

char *dirname(char * path)
{
	int i;

	if (path == NULL || path[0] == '\0')
		return ".";
	for (i = strlen(path) - 1; i >= 0 && path[i] == '/'; i--);
	if (i == -1)
		return "/";
	for (i--; i >= 0 && path[i] != '/'; i--);
	if (i == -1)
		return ".";
	path[i] = '\0';
	for (i--; i >= 0 && path[i] == '/'; i--);
	if (i == -1)
		return "/";
	path[i + 1] = '\0';
	return path;
}