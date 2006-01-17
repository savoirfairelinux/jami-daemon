#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "calls.h"
#include "display.h"

// malloc a new call
static struct call	*new_call	(char *, char *, char *);
static char			*cstate2str	(int);

static callobj	*calls_list = NULL;


callobj *
call_push_new (char *seqid, char *callid, char *destination) {
	callobj	*new;

	new = new_call (seqid, callid, destination);

	// Add call to the list.
	if (calls_list == NULL) {
		calls_list = new;
	} else {
		new->next = calls_list;
		calls_list = new;
	}

	return new;
}

void
call_print_list (void) {
	callobj *cptr = calls_list;

	if (cptr == NULL) {
		display_info ("No call currently in progress !");
		return;
	}

	display_info ("Calls in progress:");
	while (cptr != NULL) {
		display_info ("* (%s) [%s] %s", cstate2str (cptr->state),
						cptr->cid, cptr->dest);
		cptr = cptr->next;
	}
}


/********************************
 * PRIVATE FUNCTIONS            *
 ********************************/
static char *
cstate2str (int state) {
	switch (state) {
	case TALKING:
		return "Talking";
		break;

	case ON_HOLD:
		return "On Hold";
		break;

	case CALLING:
	default:
		return "Calling";
		break;
	}
}

// Malloc new call descriptor
static struct call *
new_call (char *seqid, char *cid, char *destination) {
	int				len;
	struct call *new;

	new = malloc (sizeof (struct call));
	if (new == NULL) {
		perror ("malloc");
		exit (1);
	}

	// Erase memory
	bzero (new, sizeof (struct call));

	// Put cid
	len = (strlen(cid) > (CIDMAX - 1)) ? (CIDMAX - 1) : strlen(cid);
	strncpy (new->cid, cid, len);

	// Put seq id
	len = (strlen(seqid) > (CIDMAX - 1)) ? (CIDMAX - 1) : strlen(seqid);
	strncpy (new->sid, seqid, len);

	// Put dest
	if (destination != NULL) {
		new->dest = strdup (destination);
	}

	return new;
}

/* EOF */
