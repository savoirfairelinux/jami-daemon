#include <stdio.h>

#include "display.h"

#ifdef USE_ANSI
# define PROMPT "[1;36;40mSFL[0;36;40mphone[0m>"
#else
# define PROMPT "SFLphone>"
#endif

// Display the SFLphone> prompt
void
display_prompt (void) {
	printf ("%s ", PROMPT);
	fflush (stdout);
}

// Display incoming call info
void
display_inc_call (char *from) {
	printf ("\r");
#ifdef USE_ANSI
	printf ("[1;37;45m***[1;33;44m Incoming Call from %s "
					"[1;37;45m***[0m\n", from);
#else
	printf ("*** Incoming Call from %s ***\n", from);
#endif
}

/* EOF */
