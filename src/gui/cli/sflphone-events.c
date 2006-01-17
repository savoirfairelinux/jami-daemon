#include <stdio.h>
#include <string.h>

#include "commons.h"
#include "display.h"
#include "globals.h"
#include "sflphone.h"
#include "sflphone-events.h"
#include "strutils.h"

#define EVBUFSIZE	512

static char		 ibuf[EVBUFSIZE];

int
handle_event (void) {
	bzero (ibuf, EVBUFSIZE);
	fgets (ibuf, EVBUFSIZE, fdsocket);
	clean_string (ibuf);

	if (strlen (ibuf) < 2) {
		return 0;
	}
//	debug ("GOT %s\n", ibuf);

	IF_INIT{}
	IF_GOT("100 ") {
		// Info message ?
		sflphone_handle_100 (token (ibuf));
	}

	IF_GOT("101 ") {
		// Incoming event ?
		sflphone_handle_101 (token (ibuf));
	}

	IF_GOT("200 ") {
		// Info message ?
		sflphone_handle_200 (token (ibuf));
	}
	
	return 0;
}

/* EOF */
