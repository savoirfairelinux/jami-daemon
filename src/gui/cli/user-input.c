#include <stdio.h>
#include <string.h>

#include "calls.h"
#include "commons.h"
#include "display.h"
#include "user-input.h"
#include "setup.h"
#include "sflphone.h"
#include "strutils.h"

#define IBUFSIZE 512

static char ibuf[IBUFSIZE];
extern char *current_cid;

int
handle_input (void) {
	bzero (ibuf, IBUFSIZE);
	fgets (ibuf, IBUFSIZE, stdin);
	clean_string (ibuf);

	if (strlen (ibuf) < 1) {
		return 0;
	}

	IF_INIT{}

	/* Ask the core version */
	IF_GOT("version") {
		sflphone_ask_version ();
		return 0;
	}

	/* Run setup */
	IF_GOT("setup") {
		setup_first ();
		return 0;
	}

	/* Display Help */
	IF_GOT("help") {
		help_display (); 
		return 0;
	}

	/* Quit application */
	IF_GOT("quit softly") {
		sflphone_quit(SFLPHONE_QUIT_SOFTLY);
		return -1;
	}

	/* Quit application */
	IF_GOT("quit") {
		sflphone_quit(SFLPHONE_QUIT);
		return -1;
	}

	/* Account setup commands */
	IF_GOT("register fullname ") {
		// example: register fullname Bobbie Ewing
		sflphone_set_fullname (nthtoken (ibuf, 2));
		return 0;
	}
	IF_GOT ("register sipuser ") {
		// example: register sipuser 244
		sflphone_set_sipuser (nthtoken (ibuf, 2));
		return 0;
	}
	IF_GOT("register siphost ") {
		// example: register siphost savoirfairelinux.net
		sflphone_set_siphost (nthtoken (ibuf, 2));
		return 0;
	}
	IF_GOT("register password ") {
		// example: register password sdfg0si1g
		sflphone_set_password (nthtoken (ibuf, 2));
		return 0;
	}
	IF_GOT("register sipproxy ") {
		// example: register sipproxy proxy.savoirfairelinux.net
		sflphone_set_proxy (nthtoken (ibuf, 2));
		return 0;
	}
	IF_GOT("register stun ") {
		// example: register stun stun.savoirfairelinux.net
		sflphone_set_stun (nthtoken (ibuf, 2));
		return 0;
	}

	/* Place a call */
	IF_GOT("call ") {
		sflphone_call (token (ibuf));
		return 0;
	}

	/* List calls */
	IF_GOT("calls") {
		call_print_list ();
		return 0;
	}

	/* Answer incoming call */
	IF_GOT("answer") {
		debug ("Answer call.\n");
		return 0;
	}

	/* Hangup specific call */
	IF_GOT("hangup ") {
		printf ("Hangup specific call.\n");
		sflphone_hangup (token (ibuf));
		return 0;
	}

	/* Hangup current call */
	IF_GOT("hangup") {
		if (current_cid) {
			printf ("Hangup current call.\n");
			sflphone_hangup (current_cid);
		}
		return 0;
	}

	/* Hold current call */
	IF_GOT("hold") {
		if (current_cid) {
			printf ("Hold current call.\n");
			sflphone_hold (current_cid);
		}
		return 0;
	}

	/* Unhold current call */
	IF_GOT("unhold") {
		if (current_cid) {
			printf ("Unhold current call.\n");
			sflphone_unhold (current_cid);
		}
		return 0;
	}

	return 0;
}

void
help_display (void) {
  printf ("General call: call <number>, hangup <call-id>, answer\n");
  printf ("Current call: calls, hangup, hold\n");
  printf ("Configuration: setup\n");
  printf ("Register: register {fullname|sipuser|siphost|password|sipproxy|stun} <information>\n");
  printf ("Other: version, help, quit, quit softly\n");
}
