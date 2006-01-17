#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "display.h"
#include "strutils.h"

static void remove_cr (char *);


/* Get a pointer to the next token */
char *
token (const char *string) {
	char	*ptr;

	/* find next token */
	ptr = strchr (string, TOKEN_DELIM);
	if (ptr == NULL) {
		return NULL;
	}
	else if (ptr[1] == '\0') {
		return NULL;
	}
	else return (ptr + 1);
}

/* Get a pointer to the Nth next token */
char *
nthtoken (const char *original, int n) {
	char	*ptr;
	int	 	 i = 0;
	 
	ptr = (char *) original;
	do {
		/* find next token */
		ptr = token (ptr);
		if (ptr == NULL) {
			/* not found */
			return NULL;
		}
		else if (i < (n-1) && ptr[1] == '\0') {
			/* end of string */
			return NULL;
		}

		i++;
	} while (i < n);

	return ptr;
}


void
clean_string (char *str) {
		remove_cr (str);
}

void
print_urldecoded (char *str) {
	char	*decoded;
	int		 i;

	decoded = (char *) malloc (strlen (str) + 10);
	if (decoded == NULL) {
		perror ("malloc");
		exit (1);
	}

	for (i = 0; i < strlen (str); i++) {
		if (str[i] == '+') {
			decoded[i] = ' ';
		}

#if 0
		else if (str[i] == '%' && isdigit (str[i+1]) && isdigit (str[i+2])) {
		}
#endif
	
	}
}


/* remove cr/lf at end of str */
static void
remove_cr (char *str) {
	int i;

	for (i = 0; i < strlen (str); i++) {
		if (str[i] == '\n' || str[i] == '\r') {
			str[i] = '\0';
			break;
		}
	}
}
