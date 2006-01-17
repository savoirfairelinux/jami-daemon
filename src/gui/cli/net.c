#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>


int	global_socket;

int
create_socket (char *hostname, unsigned short portnum) {
	struct hostent		*hp;
	struct sockaddr_in	 sa;
	int					 sock;

	/* Check destination */
    if ((hp = gethostbyname (hostname)) == NULL) {
		perror ("gethostbyname");
		return (-1);
	}

	/* Alloc socket */
	sock = socket (PF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror ("socket");
		return (-1);
	}

	/* Setup socket */
	bzero (&sa, sizeof(sa));
    bcopy (hp->h_addr, (char *)&sa.sin_addr, hp->h_length);
	sa.sin_family = hp->h_addrtype;
    sa.sin_port = htons ((unsigned short)portnum);

	/* Connect to sflphone */
	if (connect (sock, (const struct sockaddr *) &sa, sizeof(sa)) < 0) {
		close(sock);
		perror ("connect");
		return(-1);
	}

	return (sock);
}

/* EOF */
