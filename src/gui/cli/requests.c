#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "requests.h"

// malloc a new request
static struct request *new_request (char *, char *, void *);

static reqobj	*requests_list = NULL;


// Malloc new request
static struct request *
new_request (char *rid, char *subject, void *payload) {
	int				len;
	struct request *req;

	req = malloc (sizeof (struct request));
	if (req == NULL) {
		perror ("malloc");
		exit (1);
	}

	// Erase memory
	bzero (req, sizeof (struct request));

	// Put rid
	len = (strlen(rid) > (RIDMAX-1)) ? (RIDMAX-1) : strlen(rid);
	strncpy (req->rid, rid, len);

	// Put subject
	req->subject = strdup (subject);

	// Attach payload
	if (payload != NULL) {
		req->payload = payload;
	}

	return req;
}

void
request_push (int id, char *subject, void *payload) {
	reqobj	*newreq;
	char	 tmp[RIDMAX];

	snprintf (tmp, RIDMAX - 1, "seq%d", id);

	newreq = (reqobj *) new_request (tmp, subject, payload);
	if (requests_list == NULL) {
		// First item
		requests_list = newreq;
	} else {
		// Add to the beginning of the list.
		newreq->next = requests_list->next;
		requests_list = newreq;
	}
}

reqobj *
request_find (char *sid) {
	reqobj *candidate;

	if (requests_list == NULL) {
		return NULL;
	}

	candidate = requests_list;

	while (candidate != NULL) {
		if (0 == strcmp (sid, candidate->rid)) {
			return candidate;
		}
		candidate = candidate->next;
	}

	// Not found
	return NULL;
}

reqobj *
request_pop (char *sid) { 
	reqobj *candidate;
	reqobj *prev;

	if (requests_list == NULL) {
		return NULL;
	}

	candidate = prev = requests_list;

	while (candidate != NULL) {
		if (0 == strcmp (sid, candidate->rid)) {
			// dequeue
			if (candidate == requests_list) {
				requests_list = candidate->next;
			} else {
				prev->next = candidate->next;
			}
			return candidate;
		}

		prev = candidate;
		candidate = candidate->next;
	}

	// Not found
	return NULL;
}

void
request_free (reqobj *item) {
	if (item->subject != NULL) {
		free (item->subject);
	}

	free (item);
}

/* EOF */
