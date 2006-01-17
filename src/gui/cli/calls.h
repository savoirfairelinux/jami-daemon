#ifndef __CALLS_H__
#define __CALLS_H__

#define CIDMAX	32

enum callstate {
	CALLING = 0,
	TALKING,
	ON_HOLD
};

struct call {
	struct call		*next;
	char			*dest;
	char			 cid[CIDMAX];
	char			 sid[CIDMAX];
	enum callstate	state;
};

typedef struct call callobj;

void	 call_print_list	(void);
callobj	*call_push_new	(char *, char *, char *);

#endif	// __CALLS_H__
