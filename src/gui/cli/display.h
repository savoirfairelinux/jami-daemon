#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#ifdef DEBUG
#include <stdio.h>
# ifdef USE_ANSI
#  define debug(...)     fprintf (stderr, "\r[33m[debug][0m " __VA_ARGS__)
# else
#  define debug(...)     fprintf (stderr, "\r[debug] " __VA_ARGS__)
# endif
#else
# define debug(...)
#endif

void	display_prompt		(void);
void	display_inc_call	(char *);


#ifdef USE_ANSI
# define display_info(...)	printf("[1;33;40m[Info] " __VA_ARGS__);printf("[0m\n");
#else
#define display_info(...)	printf ("[Info] " __VA_ARGS__);printf("\n");
#endif

#endif	// __DISPLAY_H__
