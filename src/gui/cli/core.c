#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "core.h"
#include "display.h"

#define SFLPHONED_NAME	"sflphoned"

static char *core_path;
static int	find_core (void);

/* Runs the sflphone core daemon */
int
run_core (void) {
	int ret;

	if (!find_core ()) {
		display_info ("Unable to find %s.", SFLPHONED_NAME);
		display_info ("Point $SFLPHONE_CORE_DIR to its directory.");
		exit (1);
	}

	if (core_path == NULL) {
		return -1;
	}

	ret = fork ();
	switch (ret) {
	case 0:
		/* core process */
		execl (core_path, NULL, NULL);
		exit (0);
		break;
	case -1:
		/* error */
		display_info ("Unable to fork(), call 911.");
		exit (1);
	default:
		/* original process */
		sleep (1);	/* Wait for core to init. */
		break;
	}
	return ret;
}

/* Find the core sflphoned and set the full path */
static int
find_core (void) {
	char *envar;

	envar = getenv ("SFLPHONE_CORE_DIR");
	if (envar == NULL) {
		return 0;
	}

	core_path = (char *) malloc (strlen (envar) + 3 + strlen (SFLPHONED_NAME));
	if (core_path == NULL) {
		return 0;
	}

	snprintf (core_path, strlen (envar) + 3 + strlen (SFLPHONED_NAME),
					"%s/%s", envar, SFLPHONED_NAME);
	return 1;
}

/* EOF */
