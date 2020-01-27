#include "connstat.h"

#include "logger.h"

#include <netlink/netlink.h>
#include <netlink/socket.h>
#include <netlink/msg.h>
#include <netlink/route/link.h>
#include <net/if.h>
#include <pthread.h>


/*
 * TODO: implement mutex lock for this (u)register mute this from differenct
 * threads
 */

/* types and enums */
typedef void connstat_cb(void);
enum connstat_status { SUCCESS, FAILURE };

/* forward declarations */
static void* nl_event_loop_thr(void*);
void __msgtype2str(int msg_type, char *buf, size_t len);
static inline uint32_t nl_mgrp(uint32_t group);

/* global vars */
static pthread_t tid;
static struct nl_sock* nlsk = NULL;

static unsigned int col_size = 0;

namespace connstat {
static int nlcb(struct nl_msg*, void*);
static void nlcb2(struct nl_object*, void*);
static int get_neigh_state(struct nl_msg*, void*);


int connstat::nclients = 0;
void* connstat::nlibsk = NULL;


#define CS_MAX_CB_PERTOPIC 2
static struct cb_by_topic {
	int topic;
	cb cbs[CS_MAX_CB_PERTOPIC];
} cb_by_topic;
struct cb_by_topic connstat::cbs_by_topics[CONNSTAT_TOPICS_MAX];


connstat::connstat()
{
	if (nlsk)
		return;
	if (nlsk_init())
		throw std::runtime_error("nlsk_init() failed");
	return;
}

connstat::~connstat()
{
	nlsk_unplug();
	nlsk = NULL;
}

int connstat::install_cb(cb ucb, unsigned int topic)
{
	int status = FAILURE;
	for (unsigned int i=0; i<CS_MAX_CB_PERTOPIC; i++)
		if (!cbs_by_topics[topic-1].cbs[i]) {
			/* found it */
			cbs_by_topics[topic-1].cbs[i] = ucb;
			nclients++;
			status = SUCCESS;
			break;
		}
	return status;
}

int connstat::registerCB(cb ucb, unsigned int topics)
{
	int status = FAILURE;

	unsigned int t = 0;

	if (!nclients)
		while (t < CONNSTAT_TOPICS_MAX) {
			cbs_by_topics[t].topic = t;
			for (unsigned int i=0; i<CS_MAX_CB_PERTOPIC; i++)
				cbs_by_topics[t].cbs[i] = NULL;
			t++;
		}

	if (nclients == CS_MAX_CB_PERTOPIC * CONNSTAT_TOPICS_MAX) {
		JAMI_XINFO("FIXME: we're full! raise the limit, CS_MAX_CB_PERTOPIC\n");
		return status;
	}

	/* LINK */
	if ( (t = topics & addtopic(NEWLINK)) == addtopic(NEWLINK) ) {
		nl_socket_add_memberships(nlsk, RTNLGRP_LINK, RTNLGRP_NONE);
		status += install_cb(ucb, NEWLINK);
		if (!(topics ^= t))
			goto ret;
	}
	if ( (t = topics & addtopic(DELLINK)) == addtopic(DELLINK) ) {
		nl_socket_add_memberships(nlsk, RTNLGRP_LINK, RTNLGRP_NONE);
		status += install_cb(ucb, DELLINK);
		if (!(topics ^= t))
			goto ret;
	}

	/* ROUTE */
	if ( (t = topics & addtopic(NEWROUTE4)) == addtopic(NEWROUTE4) ) {
		nl_socket_add_memberships(nlsk, RTNLGRP_IPV4_ROUTE, RTNLGRP_NONE);
		status += install_cb(ucb, NEWROUTE4);
		if (!(topics ^= t))
			goto ret;
	}
	if ( (t = topics & addtopic(NEWROUTE6)) == addtopic(NEWROUTE6) ) {
		nl_socket_add_memberships(nlsk, RTNLGRP_IPV6_ROUTE, RTNLGRP_NONE);
		status += install_cb(ucb, NEWROUTE6);
		if (!(topics ^= t))
			goto ret;
	}
	if ( (t = topics & addtopic(NEWROUTE)) == addtopic(NEWROUTE) ) {
		nl_socket_add_memberships(nlsk, RTNLGRP_IPV6_ROUTE, RTNLGRP_NONE);
		nl_socket_add_memberships(nlsk, RTNLGRP_IPV4_ROUTE, RTNLGRP_NONE);
		status += install_cb(ucb, NEWROUTE);
		if (!(topics ^= t))
			goto ret;
	}
	if ( (t = topics & addtopic(DELROUTE4)) == addtopic(DELROUTE4) ) {
		nl_socket_add_memberships(nlsk, RTNLGRP_IPV4_ROUTE, RTNLGRP_NONE);
		status += install_cb(ucb, DELROUTE4);
		if (!(topics ^= t))
			goto ret;
	}
	if ( (t = topics & addtopic(DELROUTE6)) == addtopic(DELROUTE6) ) {
		nl_socket_add_memberships(nlsk, RTNLGRP_IPV6_ROUTE, RTNLGRP_NONE);
		status += install_cb(ucb, DELROUTE6);
		if (!(topics ^= t))
			goto ret;
	}
	if ( (t = topics & addtopic(DELROUTE)) == addtopic(DELROUTE) ) {
		nl_socket_add_memberships(nlsk, RTNLGRP_IPV6_ROUTE, RTNLGRP_NONE);
		nl_socket_add_memberships(nlsk, RTNLGRP_IPV4_ROUTE, RTNLGRP_NONE);
		status += install_cb(ucb, DELROUTE);
		if (!(topics ^= t))
			goto ret;
	}

	/* IF_ADDR */
	if ( (t = topics & addtopic(NEWADDR4)) == addtopic(NEWADDR4) ) {
		nl_socket_add_memberships(nlsk, RTNLGRP_IPV4_IFADDR, RTNLGRP_NONE);
		status += install_cb(ucb, NEWADDR);
		if (!(topics ^= t))
			goto ret;
	}
	if ( (t = topics & addtopic(NEWADDR6)) == addtopic(NEWADDR6) ) {
		nl_socket_add_memberships(nlsk, RTNLGRP_IPV6_IFADDR, RTNLGRP_NONE);
		status += install_cb(ucb, NEWADDR);
		if (!(topics ^= t))
			goto ret;
	}
	if ( (t = topics & addtopic(NEWADDR)) == addtopic(NEWADDR) ) {
		nl_socket_add_memberships(nlsk, RTNLGRP_IPV6_IFADDR, RTNLGRP_NONE);
		nl_socket_add_memberships(nlsk, RTNLGRP_IPV4_IFADDR, RTNLGRP_NONE);
		status += install_cb(ucb, NEWADDR);
		if (!(topics ^= t))
			goto ret;
	}
	if ( (t = topics & addtopic(DELADDR4)) == addtopic(DELADDR4) ) {
		nl_socket_add_memberships(nlsk, RTNLGRP_IPV4_IFADDR, RTNLGRP_NONE);
		status += install_cb(ucb, DELADDR);
		if (!(topics ^= t))
			goto ret;
	}
	if ( (t = topics & addtopic(DELADDR6)) == addtopic(DELADDR6) ) {
		nl_socket_add_memberships(nlsk, RTNLGRP_IPV6_IFADDR, RTNLGRP_NONE);
		status += install_cb(ucb, DELADDR);
		if (!(topics ^= t))
			goto ret;
	}
	if ( (t = topics & addtopic(DELADDR)) == addtopic(DELADDR) ) {
		nl_socket_add_memberships(nlsk, RTNLGRP_IPV6_IFADDR, RTNLGRP_NONE);
		nl_socket_add_memberships(nlsk, RTNLGRP_IPV4_IFADDR, RTNLGRP_NONE);
		status += install_cb(ucb, DELADDR);
		if (!(topics ^= t))
			goto ret;
	}

	/* ARP */
	if ( (t = topics & addtopic(NEWNEIGH)) == addtopic(NEWNEIGH) ) {
		JAMI_XINFO("got NEWNEIGH\n");
		nl_socket_add_memberships(nlsk, RTNLGRP_NEIGH, RTNLGRP_NONE);
		status += install_cb(ucb, NEWNEIGH);
		if (!(topics ^= t))
			goto ret;
	}
	if ( (t = topics & addtopic(DELNEIGH)) == addtopic(DELNEIGH) ) {
		nl_socket_add_memberships(nlsk, RTNLGRP_NEIGH, RTNLGRP_NONE);
		status += install_cb(ucb, DELNEIGH);
		if (!(topics ^= t))
			goto ret;
	}
	if ( (t = topics & addtopic(NEIGHTBL)) == addtopic(NEIGHTBL) ) {
		nl_socket_add_memberships(nlsk, RTNLGRP_NEIGH, RTNLGRP_NONE);
		status += install_cb(ucb, NEIGHTBL);
		if (!(topics ^= t))
			goto ret;
	}

	if (!topics)
            JAMI_XINFO("TODO: assert!\n");
	else
            JAMI_XDBG("unsupported topic(s) was requested");
ret:
	JAMI_XDBG("DONE processing netlink topic flags\n");
	return status ? FAILURE : SUCCESS;
}

/* FIXME: _NOT_ thread safe */
int connstat::uRegisterCB(cb ucb, unsigned int topic)
{
	int status = FAILURE;

	if (nclients <= 0) {
		fprintf(stderr, "assertion failed! refc is lower than zero!");
		return status;
	}

	cb* cbs = cbs_by_topics[topic-1].cbs;
	cb* cbrunner = cbs;

	//  order matters; "Unlike &, && guarantees left-to-right evaluation"
	while( (cbrunner - cbs < CS_MAX_CB_PERTOPIC) && (cbrunner != &ucb) )
		cbrunner++;

	if (cbrunner - cbs < CS_MAX_CB_PERTOPIC) { // found it!
		*cbrunner = cbs[nclients];
		cbs[nclients] = NULL;
		if (--nclients == 0)
			nlsk_unsubscribe();
		status = SUCCESS;
	}

	return status;
}

void connstat::executer(unsigned int topic)
{
	cb* cbs = cbs_by_topics[topic-1].cbs;
	cb* cbrunner = cbs;
	while( cbrunner - cbs < CS_MAX_CB_PERTOPIC && *cbrunner ) {
		if (*cbrunner)
			(*cbrunner)(topic);
		cbrunner++;
	}
}

/* TODO: pass topic */
int connstat::nlsk_setup(void)
{
	// struct nl_sock *setup_nlsk = (struct nl_sock *)nlsk;
	/* TODO: return proper status */
	int status = FAILURE;
	/*
	 * Notifications do not use sequence numbers, disable sequence number
	 * checking.
	 */
	nl_socket_disable_seq_check(nlsk);

	/*
	 * Define a callback function, which will be called for each notification
	 * received
	 */
	nl_socket_modify_cb(nlsk, NL_CB_VALID, NL_CB_CUSTOM, nlcb, (void *)this);

	/* Connect to routing netlink protocol */
	nl_connect(nlsk, NETLINK_ROUTE);

	status = SUCCESS;
	return status;
}

/* will allocate nlsok and start out even loop on a thread */
int connstat::nlsk_init(void)
{
	int status = FAILURE;

	// nlsk = (struct nl_sock *)nlibsk;

	if (nlsk && nl_connect(nlsk, NETLINK_ROUTE) == -NLE_BAD_SOCK) {
		JAMI_XINFO("netlink socket is already connected\n");
		return SUCCESS;
	}

	status = FAILURE;


	if (!(nlsk = nl_socket_alloc())) {
		fprintf(stderr, "can′t alloc nl socket\n");
		nlsk = NULL;
		return status;
	}

	if ((status = nlsk_setup()) == FAILURE)
		return status;

	if ((status = pthread_create(&tid, NULL, nl_event_loop_thr, (void *)nlsk)) != 0)
		fprintf(stderr, "can′t create thread err: %d\n", status);
	else
		status = SUCCESS;

	return status;
}

void connstat::nlsk_unplug()
{
	/*
	 * The socket is closed automatically if a struct nl_sock object is
	 * freed using nl_socket_free().
	 */
	if (!nlsk)
		nl_socket_free(nlsk);
}

void connstat::nlsk_unsubscribe()
{
	if (!nlsk)
		nl_socket_add_memberships(nlsk, RTNLGRP_NONE, RTNLGRP_NONE);
}

static void nlcb2(struct nl_object* obj, void* arg)
{
	// NL_DUMP_LINE(obj);
	struct rtnl_link *link_obj;
	int flags, up, running, lmcs, fam;
	char *ifname;


	link_obj = (struct rtnl_link *)obj;
	flags    = rtnl_link_get_flags(link_obj);
	ifname   = rtnl_link_get_name(link_obj);
	fam      = rtnl_link_get_family(link_obj);

	up       = (flags & IFF_UP)        ? 1 : 0;
	running  = (flags & IFF_RUNNING)   ? 1 : 0;
	lmcs     = (flags & IFF_MULTICAST) ? 1 : 0;

	int msg_type = nl_object_get_msgtype(obj);

#define MAX_MSGTYPE 64
	char msgtypestr[MAX_MSGTYPE] = {NULL};
	// nl_nlmsgtype2str(msg_type, msgtypestr, sizeof(msg_type));
	__msgtype2str(msg_type, msgtypestr, sizeof(msgtypestr));

	unsigned int topic = 0;
	switch (msg_type) {
	case RTM_NEWLINK:
		topic = NEWLINK;
		break;
	case RTM_DELLINK:
		topic = DELLINK;
		break;
	case RTM_NEWROUTE:
		topic = NEWROUTE;
		break;
	case RTM_DELROUTE:
		topic = DELROUTE;
		break;
	case RTM_NEWADDR:
		topic = NEWADDR;
		break;
	case RTM_DELADDR:
		topic = DELADDR;
		break;
	case RTM_NEWNEIGH:
		topic = NEWNEIGH;
		break;
	case RTM_DELNEIGH:
		topic = DELNEIGH;
		break;
	case RTM_NEWNEIGHTBL:
		topic = NEIGHTBL;
		break;
	default:
		return;
	}

	fprintf(stdout, "got %s, i/f=%s is %s and %s running\n",
			msgtypestr, ifname, up ? "UP" : "DOWN",
			running ? "is" : "is not");

	connstat* cs_objp = (connstat*)arg;
	cs_objp->executer(topic);
	return;
}

static int get_neigh_state(struct nl_msg* msg, void* arg)
{
	struct nlmsghdr *h = nlmsg_hdr(msg);
	struct ndmsg *r = (struct ndmsg*)nlmsg_data(h);

	unsigned int topic = 0;
	switch (r->ndm_state) {
	case NUD_REACHABLE:
		topic = NEWNEIGH;
		goto exec;
		break;
	default:
		break;
	}
	return SUCCESS;
exec:
	connstat* cs_objp = (connstat*)arg;
	cs_objp->executer(topic);
	return SUCCESS;
}

static int nlcb(struct nl_msg* msg, void* arg)
{
	struct nlmsghdr *h = nlmsg_hdr(msg);

	unsigned int ev = 0;

#define MAX_MSGTYPE 64
	char msgtypestr[MAX_MSGTYPE] = {NULL};
	__msgtype2str(h->nlmsg_type, msgtypestr, sizeof(msgtypestr));
	JAMI_XINFO("%s\n", msgtypestr);
	switch (h->nlmsg_type) {
	case RTM_NEWNEIGH:
	case RTM_DELNEIGH:
	case RTM_NEWNEIGHTBL:
		return get_neigh_state(msg, arg);
		break;
	case RTM_NEWLINK:
		ev = NEWLINK;
		break;
	case RTM_DELLINK:
		ev = DELLINK;
		break;
	case RTM_NEWROUTE:
		ev = NEWROUTE;
		break;
	case RTM_DELROUTE:
		ev = DELROUTE;
		break;
	case RTM_NEWADDR:
		ev = NEWADDR;
		break;
	case RTM_DELADDR:
		ev = DELADDR;
		break;
	default:
		return SUCCESS;
	}

	connstat* cs_objp = (connstat*)arg;
	cs_objp->executer(ev);
	return SUCCESS;
}

unsigned int connstat::addtopic(unsigned int topic)
{
	return nl_mgrp(topic);
}

} /* namespace connstat */

static void* nl_event_loop_thr(void* arg)
{
	nlsk = (struct nl_sock *)arg;
	/*
	 * Start receiving messages. The function nl_recvmsgs_default() will block
	 * until one or more netlink messages (notification) are received which
	 * will be passed on to nlcb().
	 * 0 on success or a negative error code from nl_recv():
	 * Number of bytes read,
	 * 0 on EOF,
	 * 0 on no data event (non-blocking mode), or
	 * a negative error code.
	 */
	while (nl_recvmsgs_default(nlsk) >= 0)
	   ;

	fprintf(stderr, "FIXME: printf errno etc\n");
	return arg;
}

void __msgtype2str(int msg_type, char *buf, size_t len)
{
	switch (msg_type) {
		case RTM_NEWLINK:
			snprintf(buf, len, "%s", "got NEWLINK");
			break;
		case RTM_DELLINK:
			snprintf(buf, len, "%s", "DELLINK");
			break;
		case RTM_NEWROUTE:
			snprintf(buf, len, "%s", "got NEWROUTE");
			break;
		case RTM_DELROUTE:
			snprintf(buf, len, "%s", "DELROUTE");
			break;
		case RTM_NEWADDR:
			snprintf(buf, len, "%s", "got NEWADDR");
			break;
		case RTM_DELADDR:
			snprintf(buf, len, "%s", "DELADDR");
			break;
		case RTM_NEWNEIGH:
			snprintf(buf, len, "%s", "got NEWNEIGH");
			break;
		case RTM_DELNEIGH:
			snprintf(buf, len, "%s", "DELNEIGH");
			break;
		case RTM_NEWPREFIX:
			snprintf(buf, len, "%s", "RTM_NEWPREFIX");
			break;
		case NLMSG_ERROR:
		case NLMSG_NOOP:
		case NLMSG_DONE:
		default:
			break;	/* ignore */
	}
}

static inline uint32_t nl_mgrp(uint32_t group)
{
	if (group > 31 ) {
		fprintf(stderr, "Use setsockopt for this group %d\n", group);
		exit(-1);
	}
	return group ? (1 << (group - 1)) : 0;
}
