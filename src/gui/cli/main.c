#include <stdio.h>
#include <unistd.h>

#include "commons.h"
#include "core.h"
#include "display.h"
#include "eventloop.h"
#include "globals.h"
#include "net.h"

#define MAX_RETRIES	5

int
main (int argc, char **argv) {
	int socket;
	int	launched = 0;
	int retry = 0;

	/* Init */

	/* Connect */
	do {
		//display_info ("Connecting to %s:%d...\n", CONNECT_HOST, CONNECT_PORT);
		socket = create_socket (CONNECT_HOST, CONNECT_PORT);
		if (socket < 0) {
			if (!launched) {
					// Attempt to launch sflphone.
					launched = 1;
					//display_info ("Attempting to run sflphoned.");
					core_pid = run_core ();
			} else {
				sleep (1);
			}
		} else {
			break;
		}
		retry++;
	} while (retry < MAX_RETRIES);

	if (retry < MAX_RETRIES) {
		return event_loop (socket);
	} else {
		display_info ("Cannot run sflphone daemon !");
		return 1;
	}
}
