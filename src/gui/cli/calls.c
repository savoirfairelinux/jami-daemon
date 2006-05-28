#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "calls.h"
#include "display.h"

// malloc a new call
static struct call 	*new_call	(char *, char *, char *);
void 			delete_call	(struct call*);
static char		*cstate2str	(int);

static callobj	*calls_list = NULL;
char     *current_cid = NULL;


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
	current_cid = new->cid;

	return new;
}

int
call_pop( char *call_id )
{
	callobj *cptr = calls_list;
	callobj *previous = NULL;
	
	if (cptr == NULL) {
		return 0;
	}

	while (cptr != NULL) {
		if ( 0 == strcmp(call_id, cptr->cid) ) {
			if ( previous == NULL ) { // the root is the next one
				calls_list = cptr->next;
			} else { // the previous next is the current one next
				previous->next = cptr->next;
			}
			current_cid = NULL;
			delete_call(cptr); // free memory
			return 1;
		}
		previous = cptr;
		cptr = cptr->next;
	}
	return 0;
}

void
call_change_state (char *call_id, int state)
{
	callobj *cptr = calls_list;
	
	while (cptr != NULL) {
		if ( 0 == strcmp(call_id, cptr->cid) ) {
			cptr->state = state;
			current_cid = cptr->cid;
			return;
		}
		cptr = cptr->next;
	}
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

	// Erase memory and put state to 0
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

void 
delete_call (struct call* call) 
{
	if (call != NULL) {
		call->next = NULL;
		free(call->dest); call->dest = NULL;
		free(call);
	}
}

/* EOF */
