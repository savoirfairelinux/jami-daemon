#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include "display.h"
#include "setup.h"
#include "sflphone.h"

#define IBUFSIZE 512

static char ibuf[IBUFSIZE + 1];

char *
read_input (const char *string) {
  printf("%s", string);
  return fgets(ibuf, IBUFSIZE, stdin);
}

#ifdef HAVE_LIBREADLINE
#include <readline/readline.h>
#include <readline/history.h>
#else
#define readline read_input
#endif

int
setup_first (void) {
	char *text;

	display_info ("Welcome to SFLphone.");
	display_info ("This wizard will guide you to setup your first account.");
	printf ("\n");

	display_info ("Please enter your full name");
	text = readline ("ie. John doe> ");
	if (text != NULL) {
		sflphone_set_fullname (text);
		free (text);
	}

	display_info ("Please enter your SIP username.");
	text = readline ("ie. 104> ");
	if (text != NULL) {
		sflphone_set_sipuser (text);
		free (text);
	}
	
	display_info ("Please enter your SIP domain.");
	text = readline ("ie. mycompany.net> ");
	if (text != NULL) {
		sflphone_set_siphost (text);
		free (text);
	}

	display_info ("Please enter your SIP proxy address.");
	text = readline ("ie. sip.mycompany.net> ");
	if (text != NULL) {
		sflphone_set_proxy (text);
		free (text);
	}

	display_info ("If you wish to use STUN, enter your server address.");
	text = readline ("ie. stun.mycompany.net> ");
	if (text != NULL) {
		if (strlen (text) > 3) {
			sflphone_set_stun (text);
		} else {
			display_info ("Answer was too short, ignored.");
		}
		free (text);
	}

	printf ("\n");
	sflphone_list_audiodevs ();
	display_info ("Now choose your audio device.");
	text = readline ("Number to use? ");
	if (text != NULL) {
		free (text);
	}

	display_info ("Setup done.");
	return 1;
}

