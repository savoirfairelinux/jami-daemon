#include "seqs.h"


unsigned int
get_next_seq (void) {
	static unsigned int i = 0;

	return i++;
}

unsigned int
get_next_callid (void) {
	static unsigned int i = 0;

	return i++;
}

/* EOF */
