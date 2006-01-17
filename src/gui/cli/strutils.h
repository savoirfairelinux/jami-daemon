#ifndef __STRUTILS_H__
#define __STRUTILS_H__

#define	TOKEN_DELIM	' '

char	*token	(const char *);
char	*nthtoken	(const char *, int);
void	 clean_string (char *);
void	 print_urldecoded (char *);

#endif // __STRUTILS_H__
