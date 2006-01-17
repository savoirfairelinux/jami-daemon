#include <sys/select.h>
#include <stdio.h>

#include "commons.h"
#include "display.h"
#include "globals.h"
#include "user-input.h"
#include "sflphone.h"
#include "sflphone-events.h"

int
event_loop (int socket) {
	fd_set			 rfds,
					 wfds;

	/* get socket as FILE */
	fdsocket = fdopen (socket, "a+");
	if (fdsocket == NULL) {
		perror ("fdopen");
		return -1;
	}

	/* Ask SFLphone's core version */
	sflphone_ask_version ();
  sflphone_get_events();

	/* Main event loop */
	FOREVER {
		FD_ZERO (&rfds);
		FD_ZERO (&wfds);
		FD_SET (socket, &rfds); // Watch socket
		FD_SET (0, &rfds);		// Watch stdin

		/* Wait for input, forever */
		display_prompt ();
		select (socket+1, &rfds, NULL, NULL, NULL);

		/* React to socket input */
		if (FD_ISSET (socket, &rfds)) {
			if (feof (fdsocket)) {	
				display_info ("SFLphone core closed the link. "
					"Shutting down.");
				break;
			} else {
				handle_event ();
			}
		}

		/* React to user input */
		if (FD_ISSET (0, &rfds)) {
			if (handle_input () < 0) {
				/* Exit if handle_input tells to. */
				break;
			}

			// Break on EOF
			if (feof (stdin)) { 
				display_info ("Got EOF. Bye bye !\n");
				break;
			}
		}
	}

	return 0;
}

/* EOF */
