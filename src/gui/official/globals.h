#ifndef SFLPHONE_GLOBAL_H
#define SFLPHONE_GLOBAL_H

#define DEBUG

#ifdef DEBUG
	#define _debug(...)	fprintf(stderr, "[debug] " __VA_ARGS__)
#else
	#define _debug(...)
#endif

#define NB_PHONELINES 6
#define PROGNAME "SFLPhone"
#define VERSION "0.4.2"


#endif
