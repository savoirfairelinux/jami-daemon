#ifndef __COMMONS_H__
#define __COMMONS_H__
#include <strings.h>

#define CONNECT_HOST	"127.0.0.1"
#define CONNECT_PORT	3999
#define	FOREVER			for(;;)

#define IF_INIT			if(0)
#define IF_GOT(STRING)	else if (0 == strncasecmp (ibuf, STRING, strlen(STRING))) 

#endif	// __COMMONS_H__
