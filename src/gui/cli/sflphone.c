#include <stdio.h>
#include <string.h>

#include "calls.h"
#include "commons.h"
#include "display.h"
#include "globals.h"
#include "requests.h"
#include "setup.h"
#include "sflphone.h"
#include "strutils.h"
#include "seqs.h"

// get events signal = starting voip
void 
sflphone_get_events(void) {
  int id = get_next_seq();
  request_push(id, "getevents", NULL);
  fprintf(fdsocket, "getevents seq%d\n", id);
  fflush(fdsocket);
}
// Ask the core's version
void
sflphone_ask_version (void) {
	int id = get_next_seq ();

	request_push (id, "version", NULL);
	fprintf (fdsocket, "version seq%d\n", id);
	fflush (fdsocket);
}

/* Ask a list of available audio devices */
void
sflphone_list_audiodevs (void) {
	int id = get_next_seq ();

	request_push (id, "audiodevice", NULL);
	fprintf (fdsocket, "list seq%d audiodevice\n", id);
	fflush (fdsocket);
}

// set the config full name
void
sflphone_set_fullname (char *s) {
	clean_string (s);
	if (strlen (s) < 1) {
		return;
	}
	fprintf (fdsocket, "configset seq%d VoIPLink SIP.fullName %s\n",
					get_next_seq (), s);
	fflush (fdsocket);
	display_info ("Set SIP User name: %s\n", s);
}

// set the config  sip user
void
sflphone_set_sipuser (char *s) {
	clean_string (s);
	if (strlen (s) < 1) {
		return;
	}
	fprintf (fdsocket, "configset seq%d VoIPLink SIP.userPart %s\n",
					get_next_seq (), s);
	fprintf (fdsocket, "configset seq%d VoIPLink SIP.username %s\n",
					get_next_seq (), s);
	fflush (fdsocket);
	display_info ("Set SIP User: %s\n", s);
}

// set the config sip host
void
sflphone_set_siphost (char *s) {
	clean_string (s);
	if (strlen (s) < 3) {
		return;
	}
	fprintf (fdsocket, "configset seq%d VoIPLink SIP.hostPart %s\n",
					get_next_seq (), s);
	fflush (fdsocket);
	display_info ("Set SIP host: %s\n", s);
}

// set the config sip password
void
sflphone_set_password (char *s) {
	clean_string (s);
	if (strlen (s) < 2) {
		return;
	}
	fprintf (fdsocket, "configset seq%d VoIPLink SIP.password %s\n",
					get_next_seq (), s);
	fflush (fdsocket);
	display_info ("Set SIP password: ****\n");
}

// set the config sip proxy
void
sflphone_set_proxy (char *s) {
	clean_string (s);
	if (strlen (s) < 3) {
		return;
	}
	fprintf (fdsocket, "configset seq%d VoIPLink SIP.proxy %s\n",
					get_next_seq (), s);
	fflush (fdsocket);
	display_info ("Set SIP proxy: %s\n", s);
}

// set the config STUN usage
void
sflphone_set_stun (char *s) {
	int use = 0;

	if (s[0] == '1') {
		use = 1;
	}

	fprintf (fdsocket, "configset seq%d VoIPLink SIP.useStun %d\n",
					get_next_seq (), use);
	fflush (fdsocket);
	display_info ("Set SIP STUN usage: %d\n", use);
}

// The user asks for a call
// dest is the destination to call
void
sflphone_call (char *dest) {
	int accno = 1; // must change when multiple accounts.
	int seq;
	char call_id[CIDMAX];
	char seq_id[CIDMAX];
	callobj	*calldesc;

	// dest is the destination like "5143456789"
	// output pattern is like "call seq0 acc1 c1 5143456789"
	if (dest == NULL) {
		return;
	}
	clean_string (dest);
	if (strlen (dest) < 1) {
		return;
	}

	// Alloc call parameters
	bzero (call_id, CIDMAX);
	bzero (seq_id, CIDMAX);
	seq = get_next_seq ();
	snprintf (call_id, CIDMAX - 1, "c%d", get_next_callid ());
	snprintf (seq_id, CIDMAX - 1, "seq%d", seq);

	// Push request, we are waiting for the next ack/cancel for this call.
	request_push (seq, "call", (void *) calldesc);
	
	// Add call descriptor
	call_push_new (seq_id, call_id, dest);
	
	// Send the command
	fprintf (fdsocket, "call %s acc%d %s %s\n", seq_id, accno, call_id, dest);
	fflush (fdsocket);
}

// 100 type messages are for lists
void
sflphone_handle_100 (char *string) {
	char	*ptr;
	char	 cseq[32];
	reqobj	*request;
	char	*ibuf;


	// Original Message:
	// 100 seq234 Payload here
	//     ^--string points here

	// Isolate seq field (1st field of string)
	ptr = token (string);
	if (ptr == NULL) {
		// Bogus string, should have 3 fields
		debug ("Got bogus 100: %s\n", string);
		return;
	}

	
	// Copy sequence field to cseq
	// Message:
	// 200 seq244 Payload here
	//     ^      ^--ptr points to here
	//     ^--string points to here.
	bzero (cseq, 32);
	strncpy (cseq, string, ((ptr - 1) - string));
	//debug ("Got a 100, seq=%s\n", cseq);


	// find the associated request and get its subject
	request = request_find (cseq);
	if (request == NULL) {
		// Irrelevant request
		return;
	}
	ibuf = request->subject;

	// Look at the subject and react.
	IF_INIT{}

	// This is an item from a list of audiodevs
	IF_GOT("audiodev") {
		print_urldecoded (ptr);
	}

}

// 200 type messages are validations of some commands. we have
// to notify the user about some of them.
void
sflphone_handle_200 (char *string) {
	char	*ptr;
	char	 cseq[32];
	reqobj	*request;
	char	*ibuf;


	// Original Message:
	// 200 seq234 Payload here
	//     ^--string points here

	// Isolate seq field (1st field of string)
	ptr = token (string);
	if (ptr == NULL) {
		// Bogus string, should have 3 fields
		debug ("Got bogus 200: %s\n", string);
		return;
	}

	
	// Copy sequence field to cseq
	// Message:
	// 200 seq244 Payload here
	//     ^      ^--ptr points to here
	//     ^--string points to here.
	bzero (cseq, 32);
	strncpy (cseq, string, ((ptr - 1) - string));
	//debug ("Got a 200, seq=%s\n", cseq);


	// Pop the associated request and get its subject
	request = request_find (cseq);
	if (request == NULL) {
		// Irrelevant request
		return;
	}
	ibuf = request->subject;

	// Look at the subject and react.
	IF_INIT{}

	// This message is related to an ongoing call
	IF_GOT("call") {
		// A call has been established
		if (!strcmp (ptr, "Established")) {
		}
	}

	// Answer to a version request
	IF_GOT("version") {
		// Find version string.
		display_info ("Core version: %s", ptr);
		request_pop (cseq);
		request_free (request);
	}

	// End of audio devices list.
	IF_GOT("audiodev") {
		request_pop (cseq);
		request_free (request);
	}
}

void
sflphone_handle_101 (char *string) {
}

void
sflphone_quit(SFLPHONE_QUIT_STATE state) {
  switch(state) {
  case SFLPHONE_QUIT:
    fprintf(fdsocket, "stop seq%d\n", get_next_seq());
    fflush(fdsocket);
    display_info ("Quitting SFLphone");
    
    break;

  case SFLPHONE_QUIT_SOFTLY:
    fprintf(fdsocket, "quit seq%d\n", get_next_seq());
    fflush(fdsocket);
    display_info ("Softly quitting SFLphone. SFLphone deamon still running");
    
    break;
  }
}

/* EOF */
