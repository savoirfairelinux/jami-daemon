#ifndef __REQUESTS_H__
#define __REQUESTS_H__

#define RIDMAX	32

struct request {
	struct request	*next;
	char			*subject;
	char			 rid[RIDMAX];
	void			*payload;
};

typedef struct request reqobj;


void	 request_free	(reqobj *);
void	 request_push	(int, char *, void *);
reqobj	*request_pop	(char *);
reqobj	*request_find	(char *);

#endif	// __REQUESTS_H__
