/*
Copyright (c) 2009-2014 Juliusz Chroboczek
Copyright (c) 2014 Savoir-Faire Linux Inc.

Authors : Adrien Béraud <adrien.beraud@savoirfairelinux.com>,
          Juliusz Chroboczek <jch@pps.univ–paris–diderot.fr>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "dht.h"

#include "logger.h"

extern "C" {
#include <gnutls/gnutls.h>
}

#include <sys/time.h>

#ifndef _WIN32
#include <arpa/inet.h>
#include <sys/types.h>

#else
#include <w32api.h>
#define WINVER WindowsXP
#include <ws2tcpip.h>
#endif

#include <algorithm>
#include <sstream>

#include <unistd.h>
#include <fcntl.h>
#include <cstdarg>
#include <cstring>

#ifndef MSG_CONFIRM
#define MSG_CONFIRM 0
#endif

#ifdef _WIN32

#define EAFNOSUPPORT WSAEAFNOSUPPORT
static bool
set_nonblocking(int fd, int nonblocking)
{
    unsigned long mode = !!nonblocking;
    int rc = ioctlsocket(fd, FIONBIO, &mode);
    if (rc != 0)
        errno = WSAGetLastError();
    return rc == 0;
}

extern const char *inet_ntop(int, const void *, char *, socklen_t);

#else

static bool
set_nonblocking(int fd, int nonblocking)
{
    int rc = fcntl(fd, F_GETFL, 0);
    if (rc < 0)
        return false;
    rc = fcntl(fd, F_SETFL, nonblocking?(rc | O_NONBLOCK):(rc & ~O_NONBLOCK));
    if (rc < 0)
        return false;
    return true;
}

#endif

#define WANT4 1
#define WANT6 2

static std::mt19937 rd {std::random_device{}()};
static std::uniform_int_distribution<uint8_t> rand_byte;

static const uint8_t v4prefix[16] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF, 0, 0, 0, 0
};

static std::string
to_hex(const uint8_t *buf, unsigned buflen)
{
    std::stringstream s;
    s << std::hex;
    for (unsigned i = 0; i < buflen; i++)
        s << std::setfill('0') << std::setw(2) << (unsigned)buf[i];
    s << std::dec;
    return s.str();
}

static void
debug_printable(const uint8_t *buf, unsigned buflen)
{
    std::string buf_clean(buflen, '\0');
    for (unsigned i=0; i<buflen; i++)
        buf_clean[i] = buf[i] >= 32 && buf[i] <= 126 ? buf[i] : '.';
    DEBUG("%s", buf_clean.c_str());
}

namespace dht {

const Dht::TransPrefix Dht::TransPrefix::PING = {"pn"};
const Dht::TransPrefix Dht::TransPrefix::FIND_NODE  = {"fn"};
const Dht::TransPrefix Dht::TransPrefix::GET_VALUES  = {"gp"};
const Dht::TransPrefix Dht::TransPrefix::ANNOUNCE_VALUES  = {"ap"};

static constexpr InfoHash zeroes {};
static constexpr InfoHash ones = {std::array<uint8_t, HASH_LEN>{
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF
}};

const long unsigned Dht::MAX_REQUESTS_PER_SEC = 400;

Dht::Status
Dht::getStatus(sa_family_t af) const
{
    unsigned good = 0, dubious = 0, cached = 0, incoming = 0;
    int tot = getNodesStats(af, &good, &dubious, &cached, &incoming);
    if (tot < 1)
        return Status::Disconnected;
    else if (good < 4)
        return Status::Connecting;
    return Status::Connected;
}

bool
Dht::isRunning(sa_family_t af) const
{
    return (af == AF_INET  && dht_socket  >= 0)
        || (af == AF_INET6 && dht_socket6 >= 0);
}

bool
Dht::isMartian(const sockaddr *sa, socklen_t len)
{
    // Check that sa_family can be accessed safely
    if (!sa || len < sizeof(sockaddr_in))
        return true;

    switch(sa->sa_family) {
    case AF_INET: {
        sockaddr_in *sin = (sockaddr_in*)sa;
        const uint8_t *address = (const uint8_t*)&sin->sin_addr;
        return sin->sin_port == 0 ||
            (address[0] == 0) ||
            (address[0] == 127) ||
            ((address[0] & 0xE0) == 0xE0);
    }
    case AF_INET6: {
        if (len < sizeof(sockaddr_in6))
            return true;
        sockaddr_in6 *sin6 = (sockaddr_in6*)sa;
        const uint8_t *address = (const uint8_t*)&sin6->sin6_addr;
        return sin6->sin6_port == 0 ||
            (address[0] == 0xFF) ||
            (address[0] == 0xFE && (address[1] & 0xC0) == 0x80) ||
            (memcmp(address, zeroes.data(), 15) == 0 &&
             (address[15] == 0 || address[15] == 1)) ||
            (memcmp(address, v4prefix, 12) == 0);
    }

    default:
        return true;
    }
}

Dht::Node*
Dht::Bucket::randomNode()
{
    if (nodes.empty())
        return nullptr;
    std::uniform_int_distribution<unsigned> rand_node(0, nodes.size()-1);
    unsigned nn = rand_node(rd);
    for (auto& n : nodes)
        if (not nn--) return &n;
    return &nodes.back();
}

InfoHash
Dht::RoutingTable::randomId(const Dht::RoutingTable::const_iterator& it) const
{
    int bit1 = it->first.lowbit();
    int bit2 = std::next(it) != end() ? std::next(it)->first.lowbit() : -1;
    int bit = std::max(bit1, bit2) + 1;

    if (bit >= 8*HASH_LEN)
        return it->first;

    int b = bit/8;
    InfoHash id_return;
    std::copy_n(it->first.begin(), b, id_return.begin());
    id_return[b] = it->first[b] & (0xFF00 >> (bit % 8));
    id_return[b] |= rand_byte(rd) >> (bit % 8);
    for (unsigned i = b + 1; i < HASH_LEN; i++)
        id_return[i] = rand_byte(rd);
    return id_return;
}

InfoHash
Dht::RoutingTable::middle(const RoutingTable::const_iterator& it) const
{
    int bit1 = it->first.lowbit();
    int bit2 = std::next(it) != end() ? std::next(it)->first.lowbit() : -1;
    int bit = std::max(bit1, bit2) + 1;

    if (bit >= 8*HASH_LEN)
        throw std::out_of_range("End of table");

    InfoHash id = it->first;
    id[bit / 8] |= (0x80 >> (bit % 8));
    return id;
}

Dht::RoutingTable::iterator
Dht::RoutingTable::findBucket(const InfoHash& id)
{
    if (empty())
        return end();
    auto b = begin();
    while (true) {
        auto next = std::next(b);
        if (next == end())
            return b;
        if (InfoHash::cmp(id, next->first) < 0)
            return b;
        b = next;
    }
}

Dht::RoutingTable::const_iterator
Dht::RoutingTable::findBucket(const InfoHash& id) const
{
    /* Avoid code duplication for the const version */
    const_iterator it = const_cast<RoutingTable*>(this)->findBucket(id);
    return it;
}

/* Every bucket contains an unordered list of nodes. */
Dht::Node *
Dht::findNode(const InfoHash& id, sa_family_t af)
{
    Bucket* b = findBucket(id, af);
    if (!b)
        return nullptr;
    for (auto& n : b->nodes)
        if (n.id == id) return &n;
    return nullptr;
}

const Dht::Node*
Dht::findNode(const InfoHash& id, sa_family_t af) const
{
    const Bucket* b = findBucket(id, af);
    if (!b)
        return nullptr;
    for (const auto& n : b->nodes)
        if (n.id == id) return &n;
    return nullptr;
}

/* This is our definition of a known-good node. */
bool
Dht::Node::isGood(time_t now) const
{
    return
        pinged <= 2 &&
        reply_time >= now - 7200 &&
        time >= now - 15 * 60;
}

/* Every bucket caches the address of a likely node.  Ping it. */
int
Dht::sendCachedPing(Bucket& b)
{
    /* We set family to 0 when there's no cached node. */
    if (b.cached.ss_family == 0)
        return 0;

    DEBUG("Sending ping to cached node.");
    int rc = sendPing((sockaddr*)&b.cached, b.cachedlen, TransId{TransPrefix::PING});
    b.cached.ss_family = 0;
    b.cachedlen = 0;
    return rc;
}

/* Called whenever we send a request to a node, increases the ping count
   and, if that reaches 3, sends a ping to a new candidate. */
void
Dht::pinged(Node& n, Bucket *b)
{
    n.pinged++;
    n.pinged_time = now.tv_sec;
    if (n.pinged >= 3)
        sendCachedPing(b ? *b : *findBucket(n.id, n.ss.ss_family));
}

/* The internal blacklist is an LRU cache of nodes that have sent
   incorrect messages. */
void
Dht::blacklistNode(const InfoHash* id, const sockaddr *sa, socklen_t salen)
{
    DEBUG("Blacklisting broken node.");

    if (id) {
        /* Make the node easy to discard. */
        Node *n = findNode(*id, sa->sa_family);
        if (n) {
            n->pinged = 3;
            pinged(*n);
        }
        /* Discard it from any searches in progress. */
        for (auto& sr : searches) {
            sr.nodes.erase(std::remove_if (sr.nodes.begin(), sr.nodes.end(), [id](const SearchNode& sn){
                return sn.id == *id;
            }));
        }
    }
    /* And make sure we don't hear from it again. */
    memcpy(&blacklist[next_blacklisted], sa, salen);
    next_blacklisted = (next_blacklisted + 1) % BLACKLISTED_MAX;
}

bool
Dht::isNodeBlacklisted(const sockaddr *sa, socklen_t salen) const
{
    if (salen > sizeof(sockaddr_storage))
        abort();

    if (isBlacklisted(sa, salen))
        return true;

    for (unsigned i = 0; i < BLACKLISTED_MAX; i++) {
        if (memcmp(&blacklist[i], sa, salen) == 0)
            return true;
    }

    return false;
}

/* Split a bucket into two equal parts. */
bool
Dht::RoutingTable::split(const RoutingTable::iterator& b)
{
    InfoHash new_id;
    try {
        new_id = middle(b);
    } catch (const std::out_of_range& e) {
        return false;
    }

    // Insert new bucket
    insert(std::next(b), Bucket {b->af, new_id, b->time});

    // Re-assign nodes
    std::list<Node> nodes {};
    nodes.splice(nodes.begin(), b->nodes);
    while (!nodes.empty()) {
        auto n = nodes.begin();
        auto b = findBucket(n->id);
        if (b == end())
            nodes.erase(n);
        else
            b->nodes.splice(b->nodes.begin(), nodes, n);
    }
    return true;
}

/* We just learnt about a node, not necessarily a new one.  Confirm is 1 if
   the node sent a message, 2 if it sent us a reply. */
Dht::Node*
Dht::newNode(const InfoHash& id, const sockaddr *sa, socklen_t salen, int confirm)
{
    if (isMartian(sa, salen) || isNodeBlacklisted(sa, salen))
        return nullptr;

    auto& list = sa->sa_family == AF_INET ? buckets : buckets6;
    auto b = list.findBucket(id);
    if (b == list.end() || id == myid)
        return nullptr;

    bool mybucket = list.contains(b, myid);

    if (confirm == 2)
        b->time = now.tv_sec;

    DEBUG("Dht::newNode %s", id.toString().c_str());

    for (auto& n : b->nodes) {
        if (n.id != id) continue;
        if (confirm || n.time < now.tv_sec - 15 * 60) {
            /* Known node.  Update stuff. */
            memcpy((sockaddr*)&n.ss, sa, salen);
            if (confirm)
                n.time = now.tv_sec;
            if (confirm >= 2) {
                n.reply_time = now.tv_sec;
                n.pinged = 0;
                n.pinged_time = 0;
            }
            if (confirm) {
                /* If this node existed in searches but was expired, give it another chance. */
                for (auto& s : searches) {
                    if (s.af != sa->sa_family) continue;
                    if (s.insertNode(id, sa, salen, now.tv_sec, true)) {
                        time_t tm = s.getNextStepTime(types, now.tv_sec);
                        WARN("Resurrect node  ! (%lu, in %lu sec)", tm, tm - now.tv_sec);
                        if (tm != 0 && (search_time == 0 || search_time > tm))
                            search_time = tm;
                    }
                }
            }
        }
        return &n;
    }

    /* New node. */
    DEBUG("New node!");

    /* Try adding the node to searches */
    for (auto& s : searches) {
        if (s.af != sa->sa_family) continue;
        if (s.insertNode(id, sa, salen, now.tv_sec)) {
            time_t tm = s.getNextStepTime(types, now.tv_sec);
            DEBUG("Inserted node the new way ! (%lu, in %lu sec)", tm, tm - now.tv_sec);
            if (tm != 0 && (search_time == 0 || search_time > tm))
                search_time = tm;
        }
    }

    if (mybucket) {
        if (sa->sa_family == AF_INET)
            mybucket_grow_time = now.tv_sec;
        else
            mybucket6_grow_time = now.tv_sec;
    }

    /* First, try to get rid of a known-bad node. */
    for (auto& n : b->nodes) {
        if (n.pinged < 3 || n.pinged_time >= now.tv_sec - 15)
            continue;
        n.id = id;
        memcpy((sockaddr*)&n.ss, sa, salen);
        n.time = confirm ? now.tv_sec : 0;
        n.reply_time = confirm >= 2 ? now.tv_sec : 0;
        n.pinged_time = 0;
        n.pinged = 0;
        return &n;
    }

    if (b->nodes.size() >= 8) {
        /* Bucket full.  Ping a dubious node */
        bool dubious = false;
        for (auto& n : b->nodes) {
            /* Pick the first dubious node that we haven't pinged in the
               last 15 seconds.  This gives nodes the time to reply, but
               tends to concentrate on the same nodes, so that we get rid
               of bad nodes fast. */
            if (!n.isGood(now.tv_sec)) {
                dubious = true;
                if (n.pinged_time < now.tv_sec - 15) {
                    DEBUG("Sending ping to dubious node.");
                    sendPing((sockaddr*)&n.ss, n.sslen, TransId {TransPrefix::PING});
                    n.pinged++;
                    n.pinged_time = now.tv_sec;
                    break;
                }
            }
        }

        if (mybucket && (!dubious || list.size() == 1)) {
            DEBUG("Splitting.");
            sendCachedPing(*b);
            list.split(b);
            dumpTables();
            return newNode(id, sa, salen, confirm);
        }

        /* No space for this node.  Cache it away for later. */
        if (confirm || b->cached.ss_family == 0) {
            memcpy(&b->cached, sa, salen);
            b->cachedlen = salen;
        }

        return nullptr;
    }

    /* Create a new node. */
    //DEBUG("New node! - allocation");
    b->nodes.emplace_front(id, sa, salen, confirm ? now.tv_sec : 0, confirm >= 2 ? now.tv_sec : 0);
    return &b->nodes.front();
}

/* Called periodically to purge known-bad nodes.  Note that we're very
   conservative here: broken nodes in the table don't do much harm, we'll
   recover as soon as we find better ones. */
void
Dht::expireBuckets(RoutingTable& list)
{
    for (auto& b : list) {
        bool changed = false;
        b.nodes.remove_if([&changed](const Node& n) {
            if (n.pinged >= 4) {
                changed = true;
                return true;
            }
            return false;
        });
        if (changed)
            sendCachedPing(b);
    }
    std::uniform_int_distribution<time_t> time_dis(120, 360-1);
    expire_stuff_time = now.tv_sec + time_dis(rd);
}

/* While a search is in progress, we don't necessarily keep the nodes being
   walked in the main bucket table.  A search in progress is identified by
   a unique transaction id, a short (and hence small enough to fit in the
   transaction id of the protocol packets). */

Dht::Search *
Dht::findSearch(unsigned short tid, sa_family_t af)
{
    auto sr = std::find_if (searches.begin(), searches.end(), [tid,af](const Search& s){
        return s.tid == tid && s.af == af;
    });
    return sr == searches.end() ? nullptr : &(*sr);
}

/* A search contains a list of nodes, sorted by decreasing distance to the
   target.  We just got a new candidate, insert it at the right spot or
   discard it. */
bool
Dht::Search::insertNode(const InfoHash& nid,
    const sockaddr *sa, socklen_t salen,
    time_t now, bool confirmed, const Blob& token)
{
    if (sa->sa_family != af) {
        DEBUG("Attempted to insert node in the wrong family.");
        return false;
    }

    // Fast track for the case where the node is not relevant for this search
    if (nodes.size() == SEARCH_NODES && id.xorCmp(nid, nodes.back().id) > 0)
        return false;

    bool found = false;
    auto n = std::find_if(nodes.begin(), nodes.end(), [=,&found](const SearchNode& sn) {
        if (sn.id == nid) {
            found = true;
            return true;
        }
        return id.xorCmp(nid, sn.id) < 0;
    });
    if (!found) {
        if (n == nodes.end() && nodes.size() == SEARCH_NODES)
            return false;
        n = nodes.insert(n, SearchNode{ .id = nid });
        if (nodes.size() > SEARCH_NODES)
            nodes.pop_back();
    }

    DEBUG("Search::insertNode %s", nid.toString().c_str());

    memcpy(&n->ss, sa, salen);
    n->sslen = salen;

    if (confirmed) {
        n->pinged = 0;
 //       n->request_time = 0;
//        n->token.clear();
    }
    if (not token.empty()) {
        n->reply_time = now;
        n->request_time = 0;
      /*  n->pinged = 0;*/
        if (token.size() > 64)
            DEBUG("Eek!  Overlong token.");
        else
            n->token = token;
    }

    return true;
}

void
Dht::expireSearches()
{
    auto t = now.tv_sec - SEARCH_EXPIRE_TIME;
    searches.remove_if([t](const Search& sr) {
        return sr.announce.empty() && sr.step_time < t;
    });
}

bool
Dht::searchSendGetValues(Search& sr, SearchNode *n)
{
    time_t t = now.tv_sec;
    if (!n) {
        auto ni = std::find_if(sr.nodes.begin(), sr.nodes.end(), [t](const SearchNode& sn) {
            return sn.pinged < 3 && !sn.isSynced(t) && sn.request_time < t - 15;
        });
        if (ni != sr.nodes.end())
            n = &*ni;
    }

    if (!n || n->pinged >= 3 || n->isSynced(t) || n->request_time >= t - 15)
        return false;

    {
        char hbuf[NI_MAXHOST];
        char sbuf[NI_MAXSERV];
        getnameinfo((sockaddr*)&n->ss, n->sslen, hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV);
        WARN("Sending get_values to %s:%s for %s.", hbuf, sbuf, n->id.toString().c_str());
    }
    sendGetValues((sockaddr*)&n->ss, n->sslen, TransId {TransPrefix::GET_VALUES, sr.tid}, sr.id, -1, n->reply_time >= t - 15);
    n->pinged++;
    n->request_time = t;

    /* If the node happens to be in our main routing table, mark it
       as pinged. */
    Node *node = findNode(n->id, n->ss.ss_family);
    if (node) pinged(*node);
    return true;
}

/* When a search is in progress, we periodically call search_step to send
   further requests. */
void
Dht::searchStep(Search& sr)
{
    if (sr.nodes.empty()) {
        // No nodes... yet ?
        // Nothing to do, wait for the timeout.
        /*
        if (sr.step_time == 0)
            sr.step_time = now.tv_sec;
        if (now.tv_sec - sr.step_time > SEARCH_TIMEOUT) {
            WARN("Search timed out.");
            if (sr.done_callback)
                sr.done_callback(false);
            if (sr.announce.empty())
                sr.done = true;
        }
        */
        return;
    }

    /* Check if the first 8 live nodes have replied. */
    if (sr.isSynced(now.tv_sec)) {
        DEBUG("searchStep (synced).");
        for (auto& a : sr.announce) {
            if (!a.value) {
                continue;
                ERROR("Trying to announce a null value !");
            }
            unsigned i = 0;
            bool all_acked = true;
            auto vid = a.value->id;
            const auto& type = getType(a.value->type);
            for (auto& n : sr.nodes) {
                if (n.pinged >= 3)
                    continue;
                // A proposed extension to the protocol consists in
                // omitting the token when storage tables are full.  While
                // I don't think this makes a lot of sense -- just sending
                // a positive reply is just as good --, let's deal with it.
                // if (n.token.empty())
                //    n.acked[vid] = now.tv_sec;
                // auto at = n.getAnnounceTime(vid, type);
                auto a_status = n.acked.find(vid);
                auto at = n.getAnnounceTime(a_status, type);
                if ( at <= now.tv_sec ) {
                    all_acked = false;
                    storageStore(sr.id, a.value);

                    {
                        char hbuf[NI_MAXHOST];
                        char sbuf[NI_MAXSERV];
                        getnameinfo((sockaddr*)&n.ss, n.sslen, hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV);
                        WARN("Sending announce_value to %s:%s (%s).", hbuf, sbuf, n.id.toString().c_str());
                    }
                    sendAnnounceValue((sockaddr*)&n.ss, sizeof(sockaddr_storage),
                                       TransId {TransPrefix::ANNOUNCE_VALUES, sr.tid}, sr.id, *a.value,
                                       n.token, n.reply_time >= now.tv_sec - 15);
                    if (a_status == n.acked.end()) {
                        n.acked[vid] = { .request_time = now.tv_sec };
                    } else {
                        a_status->second.request_time = now.tv_sec;
                    }
                    n.pending = true;
                }
                if (++i == 8)
                    break;
            }
            if (all_acked && a.callback) {
                a.callback(true);
                a.callback = nullptr;
            }
        }
        for (auto& n : sr.nodes) {
            if (n.pending) {
                n.pending = false;
                n.pinged++;
                n.request_time = now.tv_sec;
                if (auto node = findNode(n.id, n.ss.ss_family))
                    pinged(*node);
            }
        }
        DEBUG("Search done.");
        if (sr.done_callback) {
            sr.done_callback(true);
            sr.done_callback = nullptr;
        }
        if (sr.announce.empty())
            sr.done = true;
    } else {
        DEBUG("searchStep.");
        if (sr.step_time + SEARCH_GET_STEP >= now.tv_sec)
            return;
        if (sr.nodes.empty() && sr.announce.empty()) {
            sr.done = true;
            return;
        }

        unsigned i = 0;
        for (auto& sn : sr.nodes) {
            i += searchSendGetValues(sr, &sn) ? 1 : 0;
            if (i >= 3)
                break;
        }
    }
    sr.step_time = now.tv_sec;
    {
        std::stringstream out;
        dumpSearch(sr, out);
        DEBUG("%s", out.str().c_str());
    }
}


std::list<Dht::Search>::iterator
Dht::newSearch()
{
    auto oldest = searches.begin();
    for (auto i = searches.begin(); i != searches.end(); ++i) {
        if (i->done && (oldest->step_time > i->step_time))
            oldest = i;
    }

    /* The oldest slot is expired. */
    if (oldest != searches.end() && oldest->announce.empty() && oldest->step_time < now.tv_sec - SEARCH_EXPIRE_TIME)
        return oldest;

    /* Allocate a new slot. */
    if (searches.size() < MAX_SEARCHES) {
        searches.push_front(Search {});
        return searches.begin();
    }

    /* Oh, well, never mind.  Reuse the oldest slot. */
    /*if (oldest == searches.end())
        throw DhtException("Can't create search: search limit reached.");*/
    return oldest;
}

/* Insert the contents of a bucket into a search structure. */
void
Dht::Search::insertBucket(const Bucket& b, time_t now)
{
    for (auto& n : b.nodes)
        insertNode(n.id, (sockaddr*)&n.ss, n.sslen, now);
}

bool
Dht::Search::isSynced(time_t now) const
{
    unsigned i = 0;
    for (const auto& n : nodes) {
        if (n.pinged >= 3)
            continue;
        if (!n.isSynced(now))
            return false;
        if (++i == 8)
            break;
    }
    return i > 0;
}

time_t
Dht::Search::getAnnounceTime(const std::map<ValueType::Id, ValueType>& types) const
{
    if (nodes.empty())
        return 0;
    time_t ret = 0;
    for (const auto& a : announce) {
        if (!a.value) continue;
        auto type_it = types.find(a.value->type);
        const ValueType& type = (type_it == types.end()) ? ValueType::USER_DATA : type_it->second;
        unsigned i = 0;
        for (const auto& n : nodes) {
            if (n.pinged >= 3)
                continue;
            auto at = n.getAnnounceTime(a.value->id, type);
            if (at != 0 && (ret == 0 || ret > at))
                ret = at;
            if (++i == 8)
                break;
        }
    }
    return ret;
}

time_t
Dht::Search::getNextStepTime(const std::map<ValueType::Id, ValueType>& types, time_t now) const
{
    if (done || nodes.empty())
        return 0;
    if (!isSynced(now))
        return step_time + SEARCH_GET_STEP + 1;
    return getAnnounceTime(types);
}

void
Dht::bootstrapSearch(Dht::Search& sr)
{
    auto& list = (sr.af == AF_INET) ? buckets : buckets6;
    if (list.empty() || (list.size() == 1 && list.front().nodes.empty()))
        return;
    DEBUG("bootstrapSearch.");
    auto b = list.findBucket(sr.id);
    if (b == list.end())
        return;

    time_t t = now.tv_sec;
    sr.insertBucket(*b, t);

    if (sr.nodes.size() < SEARCH_NODES) {
        if (std::next(b) != list.end())
            sr.insertBucket(*std::next(b), t);
        if (b != list.begin())
            sr.insertBucket(*std::prev(b), t);
    }
    if (sr.nodes.size() < SEARCH_NODES)
        sr.insertBucket(*list.findBucket(myid), t);
}

/* Start a search.  If announce is set, perform an announce when the
   search is complete. */
Dht::Search*
Dht::search(const InfoHash& id, sa_family_t af, GetCallback callback, DoneCallback done_callback, Value::Filter filter)
{
    if (!isRunning(af)) {
        ERROR("Unsupported protocol IPv%s bucket for %s", (af == AF_INET) ? "4" : "6", id.toString().c_str());
        if (done_callback)
            done_callback(false);
        return nullptr;
    }

    auto sr = std::find_if (searches.begin(), searches.end(), [id,af](const Search& s) {
        return s.id == id && s.af == af;
    });

    time_t t = now.tv_sec;
    if (sr != searches.end()) {
        /* We're reusing data from an old search.  Reusing the same tid
           means that we can merge replies for both searches. */
        sr->done = false;
        // Discard any doubtful nodes.
        sr->nodes.erase(std::remove_if (sr->nodes.begin(), sr->nodes.end(), [t](const SearchNode& n) {
            return n.pinged >= 3 || n.reply_time < t - 7200;
        }), sr->nodes.end());
/*
        for (auto& n : sr->nodes) {
            // preserve recent nodes for which the token is still valid.
            if (n.reply_time > t - TOKEN_EXPIRE_TIME)
                continue;
            n.pinged = 0;
            n.replied = false;
            n.token = {};
            n.acked.clear();
            //n.acked = false;
        }*/
    } else {
        sr = newSearch();
        if (sr == searches.end()) {
            errno = ENOSPC;
            return nullptr;
        }
        sr->af = af;
        sr->tid = search_id++;
        sr->step_time = 0;
        sr->id = id;
        sr->done = false;
        sr->nodes = {};
        DEBUG("New IPv%s search for %s", (af == AF_INET) ? "4" : "6", id.toString().c_str());
    }

    if (callback)
        sr->callbacks.emplace_back(filter, callback);
    sr->done_callback = done_callback;

    bootstrapSearch(*sr);
    searchStep(*sr);
    search_time = t;
    return &(*sr);
}

void
Dht::announce(const InfoHash& id, sa_family_t af, const std::shared_ptr<Value>& value, DoneCallback callback)
{
    if (!value) {
        if (callback)
            callback(false);
        return;
    }
    auto sri = std::find_if (searches.begin(), searches.end(), [id,af](const Search& s) {
        return s.id == id && s.af == af;
    });
    Search* sr = (sri == searches.end()) ? search(id, af, nullptr, nullptr) : &(*sri);
    if (!sr) {
        if (callback)
            callback(false);
        return;
    }
    sr->done = false;
    auto a_sr = std::find_if(sr->announce.begin(), sr->announce.end(), [&](const Announce& a){
        return a.value == value;
    });
    if (a_sr == sr->announce.end())
        sr->announce.emplace_back(Announce {value, callback});
    else
        a_sr->callback = callback;
}

void
Dht::put(const InfoHash& id, Value&& value, DoneCallback callback)
{
    if (value.id == Value::INVALID_ID) {
        std::random_device rdev;
        std::uniform_int_distribution<Value::Id> rand_id {};
        value.id = rand_id(rdev);
    }
/*
    // If the value is encrypted, the owner is unknown.
    if (not value.isEncrypted() && value.owner == zeroes)
        value.owner = getId();
*/
    auto val = std::make_shared<Value>(std::move(value));
    DEBUG("put: adding %s -> %s", id.toString().c_str(), val->toString().c_str());

    auto ok = std::make_shared<bool>(false);
    auto done = std::make_shared<bool>(false);
    auto done4 = std::make_shared<bool>(false);
    auto done6 = std::make_shared<bool>(false);
    auto donecb = [=]() {
        // Callback as soon as the value is announced on one of the available networks
        if (callback && !*done && (*ok || (*done4 && *done6))) {
            callback(*ok);
            *done = true;
        }
    };
    announce(id, AF_INET, val, [=](bool ok4) {
        DEBUG("search done IPv4 %d", ok4);
        *done4 = true;
        *ok |= ok4;
        donecb();
    });
    announce(id, AF_INET6, val, [=](bool ok6) {
        DEBUG("search done IPv6 %d", ok6);
        *done6 = true;
        *ok |= ok6;
        donecb();
    });
}

void
Dht::get(const InfoHash& id, GetCallback getcb, DoneCallback donecb, Value::Filter filter)
{
    /* Try to answer this search locally. */
    if (getcb) {
        auto locVals = getLocal(id, filter);
        if (not locVals.empty()) {
            DEBUG("Found local data (%d values).", locVals.size());
            getcb(locVals);
        }
    }

    auto done = std::make_shared<bool>(false);
    auto done4 = std::make_shared<bool>(false);
    auto done6 = std::make_shared<bool>(false);
    auto vals = std::make_shared<std::vector<std::shared_ptr<Value>>>();
    auto done_l = [=]() {
        if ((*done4 && *done6) || *done) {
            *done = true;
            donecb(true);
        }
    };
    auto cb = [=](const std::vector<std::shared_ptr<Value>>& values) {
        if (*done)
            return false;
        std::vector<std::shared_ptr<Value>> newvals {};
        for (const auto& v : values) {
            auto it = std::find_if(vals->begin(), vals->end(), [&](const std::shared_ptr<Value>& sv) {
                return sv == v || *sv == *v;
            });
            if (it == vals->end()) {
                if (filter(*v))
                    newvals.push_back(v);
            }
        }
        if (!newvals.empty()) {
            *done = !getcb(newvals);
            vals->insert(vals->end(), newvals.begin(), newvals.end());
        }
        done_l();
        return !*done;
    };
    Dht::search(id, AF_INET, cb, [=](bool) {
        *done4 = true;
        done_l();
    });
    Dht::search(id, AF_INET6, cb, [=](bool) {
        *done6 = true;
        done_l();
    });

}

/* A struct storage stores all the stored peer addresses for a given info
   hash. */

Dht::Storage*
Dht::findStorage(const InfoHash& id)
{
    for (auto& st : store)
        if (st.id == id)
            return &st;
    return nullptr;
}

Dht::ValueStorage*
Dht::storageStore(const InfoHash& id, const std::shared_ptr<Value>& value)
{
    Storage *st = findStorage(id);
    if (!st) {
        if (store.size() >= MAX_HASHES)
            return nullptr;
        store.push_back(Storage {id, {}});
        st = &store.back();
    }

    auto it = std::find_if (st->values.begin(), st->values.end(), [&](const ValueStorage& vr) {
        return vr.data == value || *vr.data == *value;
    });
    if (it != st->values.end()) {
        /* Already there, only need to refresh */
        it->time = now.tv_sec;
        return &*it;
    } else {
        DEBUG("Storing %s -> %s", id.toString().c_str(), value->toString().c_str());
        if (st->values.size() >= MAX_VALUES)
            return nullptr;
        st->values.emplace_back(value, now.tv_sec);
        return &st->values.back();
    }
}

void
Dht::expireStorage()
{
    auto i = store.begin();
    while (i != store.end())
    {
        i->values.erase(
            std::partition(i->values.begin(), i->values.end(),
                [&](const ValueStorage& v)
                {
                    if (!v.data) return true; // should not happen
                    const auto& type = getType(v.data->type);
                    bool expired = v.time + type.expiration < now.tv_sec;
                    if (expired)
                        DEBUG("Discarding expired value %s", v.data->toString().c_str());
                    return !expired;
                }),
            i->values.end());

        if (i->values.size() == 0) {
            DEBUG("Discarding expired value %s", i->id.toString().c_str());
            i = store.erase(i);
        }
        else
            ++i;
    }
}

void
Dht::rotateSecrets()
{
    std::uniform_int_distribution<time_t> time_dist(15*60, 45*60);
    rotate_secrets_time = now.tv_sec + time_dist(rd);

    oldsecret = secret;
    {
        std::random_device rdev;
        std::generate_n(secret.begin(), secret.size(), std::bind(rand_byte, std::ref(rdev)));
    }
}

Blob
Dht::makeToken(const sockaddr *sa, bool old) const
{
    void *ip;
    size_t iplen;
    in_port_t port;

    if (sa->sa_family == AF_INET) {
        sockaddr_in *sin = (sockaddr_in*)sa;
        ip = &sin->sin_addr;
        iplen = 4;
        port = htons(sin->sin_port);
    } else if (sa->sa_family == AF_INET6) {
        sockaddr_in6 *sin6 = (sockaddr_in6*)sa;
        ip = &sin6->sin6_addr;
        iplen = 16;
        port = htons(sin6->sin6_port);
    } else {
        return {};
    }

    const auto& c1 = old ? oldsecret : secret;
    Blob data;
    data.reserve(sizeof(secret)+2+iplen);
    data.insert(data.end(), c1.begin(), c1.end());
    data.insert(data.end(), (uint8_t*)ip, (uint8_t*)ip+iplen);
    data.insert(data.end(), (uint8_t*)&port, ((uint8_t*)&port)+2);

    size_t sz = TOKEN_SIZE;
    Blob ret {};
    ret.resize(sz);
    gnutls_datum_t gnudata = {data.data(), (unsigned int)data.size()};
    if (gnutls_fingerprint(GNUTLS_DIG_SHA512, &gnudata, ret.data(), &sz) != GNUTLS_E_SUCCESS)
        throw DhtException("Can't compute SHA512");
    ret.resize(sz);
    return ret;
}

bool
Dht::tokenMatch(const Blob& token, const sockaddr *sa) const
{
    if (!sa || token.size() != TOKEN_SIZE)
        return false;
    if (token == makeToken(sa, false))
        return true;
    if (token == makeToken(sa, true))
        return true;
    return false;
}

int
Dht::getNodesStats(sa_family_t af, unsigned *good_return, unsigned *dubious_return, unsigned *cached_return, unsigned *incoming_return) const
{
    unsigned good = 0, dubious = 0, cached = 0, incoming = 0;
    auto& list = (af == AF_INET) ? buckets : buckets6;

    for (const auto& b : list) {
        for (auto& n : b.nodes) {
            if (n.isGood(now.tv_sec)) {
                good++;
                if (n.time > n.reply_time)
                    incoming++;
            } else {
                dubious++;
            }
        }
        if (b.cached.ss_family > 0)
            cached++;
    }
    if (good_return)
        *good_return = good;
    if (dubious_return)
        *dubious_return = dubious;
    if (cached_return)
        *cached_return = cached;
    if (incoming_return)
        *incoming_return = incoming;
    return good + dubious;
}

void
Dht::dumpBucket(const Bucket& b, std::ostream& out) const
{
    out << b.first.toString() << " count " << b.nodes.size() << " age " << (int)(now.tv_sec - b.time);
    if (b.cached.ss_family)
        out << " (cached)";
    out  << std::endl;
    for (auto& n : b.nodes) {
        std::string buf(INET6_ADDRSTRLEN, '\0');
        unsigned short port;
        out << "    Node " << n.id.toString() << " ";
        if (n.ss.ss_family == AF_INET) {
            sockaddr_in *sin = (sockaddr_in*)&n.ss;
            inet_ntop(AF_INET, &sin->sin_addr, (char*)buf.data(), buf.size());
            port = ntohs(sin->sin_port);
        } else if (n.ss.ss_family == AF_INET6) {
            sockaddr_in6 *sin6 = (sockaddr_in6*)&n.ss;
            inet_ntop(AF_INET6, &sin6->sin6_addr, (char*)buf.data(), buf.size());
            port = ntohs(sin6->sin6_port);
        } else {
            out << "unknown("+std::to_string(n.ss.ss_family)+")";
            port = 0;
        }
        buf.resize(std::char_traits<char>::length(buf.c_str()));

        if (n.ss.ss_family == AF_INET6)
            out << "[" << buf << "]:" << port;
        else
            out << buf << ":" << port;
        if (n.time != n.reply_time)
            out << " age " << (now.tv_sec - n.time) << ", " << (now.tv_sec - n.reply_time);
        else
            out << " age " << (now.tv_sec - n.time);
        if (n.pinged)
            out << " (" << n.pinged << ")";
        if (n.isGood(now.tv_sec))
            out << " (good)";
        out << std::endl;
    }
}

void
Dht::dumpSearch(const Search& sr, std::ostream& out) const
{
    out << std::endl << "Search (IPv" << (sr.af == AF_INET6 ? "6" : "4") << ") " << sr.id.toString().c_str();
    out << " age " << (now.tv_sec - sr.step_time) << " s";
    if (sr.done)
        out << " [done]";
    bool synced = sr.isSynced(now.tv_sec);
    out << (synced ? " [synced]" : " [not synced]");
    if (synced && not sr.announce.empty()) {
        auto at = sr.getAnnounceTime(types);
        if (at && at > now.tv_sec)
            out << " [all announced]";
        else
            out << " announce at " << at << ", in " << (at-now.tv_sec) << " s.";
    }
    out << std::endl;
    unsigned i = 0;
    for (const auto& n : sr.nodes) {
        out << "Node " << i++ << " id " << n.id.toString().c_str() << " bits " << InfoHash::commonBits(sr.id, n.id);
        if (n.request_time)
            out << " req: " << (now.tv_sec - n.request_time) << " s,";
        out << " age:" << (now.tv_sec - n.reply_time) << " s";
        if (n.pinged)
            out << " pinged: " << n.pinged;
        if (findNode(n.id, AF_INET))
            out << " [known]";
        if (n.reply_time)
            out << " [replied]";
        out << (n.isSynced(now.tv_sec) ? " [synced]" : " [not synced]");
        out << std::endl;
    }
}

void
Dht::dumpTables() const
{
    std::stringstream out;
    out << "My id " << myid.toString().c_str() << std::endl;

    out << "Buckets IPv4 :" << std::endl;
    for (const auto& b : buckets)
        dumpBucket(b, out);
    out << "Buckets IPv6 :" << std::endl;
    for (const auto& b : buckets6)
        dumpBucket(b, out);

    for (const auto& sr : searches)
        dumpSearch(sr, out);
    out << std::endl;

    for (const auto& st : store) {
        out << "Storage " << st.id.toString().c_str() << " " << st.values.size() << " values:" << std::endl;
        for (const auto& v : st.values)
            out << "   " << *v.data << " (" << (now.tv_sec - v.time) << "s)" << std::endl;
    }

    //out << std::endl << std::endl;
    DEBUG("%s", out.str().c_str());
}


Dht::Dht(int s, int s6, const InfoHash& id, const unsigned char *v)
 : dht_socket(s), dht_socket6(s6), myid(id)
{
    if (s < 0 && s6 < 0)
        return;

    if (s >= 0) {
        buckets = {Bucket {AF_INET}};
        if (!set_nonblocking(s, 1))
            throw DhtException("Can't set socket to non-blocking mode");
    }

    if (s6 >= 0) {
        buckets6 = {Bucket {AF_INET6}};
        if (!set_nonblocking(s6, 1))
            throw DhtException("Can't set socket to non-blocking mode");
    }

    registerType(ValueType::USER_DATA);
    registerType(ServiceAnnouncement::TYPE);

    std::uniform_int_distribution<decltype(search_id)> searchid_dis {};
    search_id = searchid_dis(rd);

    if (v) {
        memcpy(my_v, "1:v4:", 5);
        memcpy(my_v + 5, v, 4);
        have_v = true;
    } else {
        have_v = false;
    }

    gettimeofday(&now, nullptr);

    std::uniform_int_distribution<time_t> time_dis {0,3};
    mybucket_grow_time = now.tv_sec;
    mybucket6_grow_time = now.tv_sec;
    confirm_nodes_time = now.tv_sec + time_dis(rd);
    rate_limit_time = now.tv_sec;

    rotateSecrets();

    expireBuckets(buckets);
    expireBuckets(buckets6);

    DEBUG("DHT initialised with node ID %s", myid.toString().c_str());
}


Dht::~Dht()
{}

/* Rate control for requests we receive. */

bool
Dht::rateLimit()
{
    if (rate_limit_tokens == 0) {
        rate_limit_tokens = std::min(MAX_REQUESTS_PER_SEC, 100 * static_cast<long unsigned>(now.tv_sec - rate_limit_time));
        rate_limit_time = now.tv_sec;
    }

    if (rate_limit_tokens == 0)
        return false;

    rate_limit_tokens--;
    return true;
}

bool
Dht::neighbourhoodMaintenance(RoutingTable& list)
{
    DEBUG("neighbourhoodMaintenance");

    auto b = list.findBucket(myid);
    if (b == list.end())
        return false;

    InfoHash id = myid;
    id[HASH_LEN-1] = rand_byte(rd);

    std::binomial_distribution<bool> rand_trial(1, 1./8.);
    auto q = b;
    if (std::next(q) != list.end() && (q->nodes.empty() || rand_trial(rd)))
        q = std::next(q);
    if (b != list.begin() && (q->nodes.empty() || rand_trial(rd))) {
        auto r = std::prev(b);
        if (!r->nodes.empty())
            q = r;
    }

    /* Since our node-id is the same in both DHTs, it's probably
       profitable to query both families. */
    int want = dht_socket >= 0 && dht_socket6 >= 0 ? (WANT4 | WANT6) : -1;
    Node *n = q->randomNode();
    if (n) {
        DEBUG("Sending find_node for%s neighborhood maintenance.", q->af == AF_INET6 ? " IPv6" : "");
        sendFindNode((sockaddr*)&n->ss, n->sslen,
                       TransId {TransPrefix::FIND_NODE}, id, want,
                       n->reply_time >= now.tv_sec - 15);
        pinged(*n, &(*q));
    }

    return true;
}

bool
Dht::bucketMaintenance(RoutingTable& list)
{
    std::binomial_distribution<bool> rand_trial(1, 1./8.);
    std::binomial_distribution<bool> rand_trial_38(1, 1./38.);

    for (auto b = list.begin(); b != list.end(); ++b) {
        if (b->time < now.tv_sec - 600 || b->nodes.empty()) {
            /* This bucket hasn't seen any positive confirmation for a long
               time.  Pick a random id in this bucket's range, and send
               a request to a random node. */
            InfoHash id = list.randomId(b);
            auto q = b;
            /* If the bucket is empty, we try to fill it from a neighbour.
               We also sometimes do it gratuitiously to recover from
               buckets full of broken nodes. */
            if (std::next(b) != list.end() && (q->nodes.empty() || rand_trial(rd)))
                q = std::next(b);
            if (b != list.begin() && (q->nodes.empty() || rand_trial(rd))) {
                auto r = std::prev(b);
                if (!r->nodes.empty())
                    q = r;
            }

            Node *n = q->randomNode();
            if (n) {
                int want = -1;

                if (dht_socket >= 0 && dht_socket6 >= 0) {
                    auto otherbucket = findBucket(id, q->af == AF_INET ? AF_INET6 : AF_INET);
                    if (otherbucket && otherbucket->nodes.size() < 8)
                        /* The corresponding bucket in the other family
                           is emptyish -- querying both is useful. */
                        want = WANT4 | WANT6;
                    else if (rand_trial_38(rd))
                        /* Most of the time, this just adds overhead.
                           However, it might help stitch back one of
                           the DHTs after a network collapse, so query
                           both, but only very occasionally. */
                        want = WANT4 | WANT6;
                }

                DEBUG("Sending find_node for%s bucket maintenance.", q->af == AF_INET6 ? " IPv6" : "");
                sendFindNode((sockaddr*)&n->ss, n->sslen,
                               TransId {TransPrefix::FIND_NODE}, id, want,
                               n->reply_time >= now.tv_sec - 15);
                pinged(*n, &(*q));
                /* In order to avoid sending queries back-to-back,
                   give up for now and reschedule us soon. */
                return true;
            } else {
                //DEBUG("Bucket maintenance %s: no suitable node", q->af == AF_INET ? "IPv4" : "IPv6");
            }
        }
    }
    return false;
}

void
Dht::processMessage(const uint8_t *buf, size_t buflen, const sockaddr *from, socklen_t fromlen)
{
    if (buflen == 0)
        return;

    //DEBUG("processMessage %p %lu %p %lu", buf, buflen, from, fromlen);

    MessageType message;
    InfoHash id, info_hash, target;
    TransId tid;
    Blob token {};
    uint8_t nodes[256], nodes6[1024];
    unsigned nodes_len = 256, nodes6_len = 1024;
    in_port_t port;
    Value::Id value_id;
    uint16_t error_code;

    std::vector<std::shared_ptr<Value>> values;

    int want;
    uint16_t ttid;

    if (isMartian(from, fromlen))
        return;

    if (isNodeBlacklisted(from, fromlen)) {
        DEBUG("Received packet from blacklisted node.");
        return;
    }

    if (buf[buflen] != '\0')
        throw DhtException("Unterminated message.");

    try {
        message = parseMessage(buf, buflen, tid, id, info_hash, target,
                                port, token, value_id,
                                nodes, &nodes_len, nodes6, &nodes6_len,
                                values, &want, error_code);
        if (message != MessageType::Error && id == zeroes)
            throw DhtException("no or invalid InfoHash");
    } catch (const std::exception& e) {
        DEBUG("Can't process message of size %lu: %s.", buflen, e.what());
        debug_printable(buf, buflen);
        //DEBUG("");
        return;
    }

    if (id == myid) {
        DEBUG("Received message from self.");
        return;
    }

    if (message > MessageType::Reply) {
        /* Rate limit requests. */
        if (!rateLimit()) {
            DEBUG("Dropping request due to rate limiting.");
            return;
        }
    }

    switch (message) {
    case MessageType::Error:
        if (tid.length != 4) return;
        DEBUG("Received error message:");
        debug_printable(buf, buflen);
        if (error_code == 401 && id != zeroes && tid.matches(TransPrefix::ANNOUNCE_VALUES, &ttid)) {
            auto sr = findSearch(ttid, from->sa_family);
            if (!sr) return;
            for (auto& n : sr->nodes) {
                if (n.id != id) continue;
                newNode(id, from, fromlen, 2);
                n.request_time = 0;
                n.reply_time = 0;
                n.pinged = 0;
            }
            searchSendGetValues(*sr);
        }
        break;
    case MessageType::Reply:
        if (tid.length != 4) {
            DEBUG("Broken node truncates transaction ids: ");
            debug_printable(buf, buflen);
            DEBUG("\n");
            /* This is really annoying, as it means that we will
               time-out all our searches that go through this node.
               Kill it. */
            blacklistNode(&id, from, fromlen);
            return;
        }
        if (tid.matches(TransPrefix::PING)) {
            DEBUG("Pong!");
            newNode(id, from, fromlen, 2);
        } else if (tid.matches(TransPrefix::FIND_NODE) or tid.matches(TransPrefix::GET_VALUES)) {
            bool gp = false;
            Search *sr = nullptr;
            if (tid.matches(TransPrefix::GET_VALUES, &ttid)) {
                gp = true;
                sr = findSearch(ttid, from->sa_family);
            }
            DEBUG("Nodes found (%u+%u)%s!", nodes_len/26, nodes6_len/38, gp ? " for get_values" : "");
            if (nodes_len % 26 != 0 || nodes6_len % 38 != 0) {
                DEBUG("Unexpected length for node info!");
                blacklistNode(&id, from, fromlen);
            } else if (gp && sr == NULL) {
                DEBUG("Unknown search!");
                newNode(id, from, fromlen, 1);
            } else {
                newNode(id, from, fromlen, 2);
                for (unsigned i = 0; i < nodes_len / 26; i++) {
                    uint8_t *ni = nodes + i * 26;
                    const InfoHash& ni_id = *reinterpret_cast<InfoHash*>(ni);
                    if (ni_id == myid)
                        continue;
                    sockaddr_in sin { .sin_family = AF_INET };
                    memcpy(&sin.sin_addr, ni + ni_id.size(), 4);
                    memcpy(&sin.sin_port, ni + ni_id.size() + 4, 2);
                    newNode(ni_id, (sockaddr*)&sin, sizeof(sin), 0);
                    if (sr && sr->af == AF_INET) {
                        sr->insertNode(ni_id, (sockaddr*)&sin, sizeof(sin), now.tv_sec);
                    }
                }
                for (unsigned i = 0; i < nodes6_len / 38; i++) {
                    uint8_t *ni = nodes6 + i * 38;
                    InfoHash* ni_id = reinterpret_cast<InfoHash*>(ni);
                    if (*ni_id == myid)
                        continue;
                    sockaddr_in6 sin6 {.sin6_family = AF_INET6};
                    memcpy(&sin6.sin6_addr, ni + HASH_LEN, 16);
                    memcpy(&sin6.sin6_port, ni + HASH_LEN + 16, 2);
                    newNode(*ni_id, (sockaddr*)&sin6, sizeof(sin6), 0);
                    if (sr && sr->af == AF_INET6) {
                        sr->insertNode(*ni_id, (sockaddr*)&sin6, sizeof(sin6), now.tv_sec);
                    }
                }
                if (sr) {
                    /* Since we received a reply, the number of
                       requests in flight has decreased.  Let's push
                       another request. */
                    /*if (sr->isSynced(now.tv_sec)) {
                        DEBUG("Trying to accelerate search!");
                        search_time = now.tv_sec;
                        //sr->step_time = 0;
                    } else {*/
                        searchSendGetValues(*sr);
                    //}
                }
            }
            if (sr) {
                sr->insertNode(id, from, fromlen, now.tv_sec, true, token);
                if (!values.empty()) {
                    DEBUG("Got %d values !", values.size());
                    for (auto& cb : sr->callbacks) {
                        if (!cb.second) continue;
                        std::vector<std::shared_ptr<Value>> tmp;
                        std::copy_if(values.begin(), values.end(), std::back_inserter(tmp), [&](const std::shared_ptr<Value>& v){
                            return cb.first(*v);
                        });
                        if (cb.second and not tmp.empty())
                            cb.second(tmp);
                    }
                }
                //dumpTables();
                if (sr->isSynced(now.tv_sec)) {
                    search_time = now.tv_sec;
                }
            }
        } else if (tid.matches(TransPrefix::ANNOUNCE_VALUES, &ttid)) {
            DEBUG("Got reply to announce_values.");
            Search *sr = findSearch(ttid, from->sa_family);
            if (!sr || value_id == Value::INVALID_ID) {
                DEBUG("Unknown search or announce!");
                newNode(id, from, fromlen, 1);
            } else {
                newNode(id, from, fromlen, 2);
                for (auto& sn : sr->nodes)
                    if (sn.id == id) {
                        auto it = sn.acked.insert({value_id, {}});
                        it.first->second.request_time = 0;
                        it.first->second.reply_time = now.tv_sec;
                        sn.request_time = 0;
                        //sn.reply_time = now.tv_sec;
                        //sn.acked[value_id] = now.tv_sec;
                        sn.pinged = 0;

                        break;
                    }
                /* See comment for gp above. */
                searchSendGetValues(*sr);
            }
        } else {
            DEBUG("Unexpected reply: ");
            debug_printable(buf, buflen);
            DEBUG("\n");
        }
        break;
    case MessageType::Ping:
        DEBUG("Got ping (%d)!", tid.length);
        newNode(id, from, fromlen, 1);
        DEBUG("Sending pong.");
        sendPong(from, fromlen, tid);
        break;
    case MessageType::FindNode:
        DEBUG("Got \"find node\" request");
        newNode(id, from, fromlen, 1);
        DEBUG("Sending closest nodes (%d).", want);
        sendClosestNodes(from, fromlen, tid, target, want);
        break;
    case MessageType::GetValues:
        DEBUG("Got \"get values\" request");
        newNode(id, from, fromlen, 1);
        if (info_hash == zeroes) {
            DEBUG("Eek!  Got get_values with no info_hash.");
            sendError(from, fromlen, tid, 203, "Get_values with no info_hash");
            break;
        } else {
            Storage* st = findStorage(info_hash);
            Blob ntoken = makeToken(from, false);
            if (st && st->values.size() > 0) {
                 DEBUG("Sending found%s values.", from->sa_family == AF_INET6 ? " IPv6" : "");
                 sendClosestNodes(from, fromlen, tid, info_hash, want, ntoken, st);
            } else {
                DEBUG("Sending nodes for get_values.");
                sendClosestNodes(from, fromlen, tid, info_hash, want, ntoken);
            }
        }
        break;
    case MessageType::AnnounceValue:
        DEBUG("Got \"announce value\" request!");
        newNode(id, from, fromlen, 1);
        if (info_hash == zeroes) {
            DEBUG("Announce_value with no info_hash.");
            sendError(from, fromlen, tid, 203, "Announce_value with no info_hash");
            break;
        }
        if (!tokenMatch(token, from)) {
            DEBUG("Incorrect token %s for announce_values.", to_hex(token.data(), token.size()).c_str());
            sendError(from, fromlen, tid, 401, "Announce_value with wrong token");
            break;
        }
        for (const auto& v : values) {
            if (v->id == Value::INVALID_ID) {
                DEBUG("Incorrect value id ");
                sendError(from, fromlen, tid, 203, "Announce_value with invalid id");
                continue;
            }
            auto lv = getLocal(info_hash, v->id);
            if (lv && lv->owner != id) {
                DEBUG("Attempting to store value belonging to another user.");
                sendError(from, fromlen, tid, 203, "Announce_value with wrong permission");
                continue;
            }
            {
                // Allow the value to be edited by the storage policy
                std::shared_ptr<Value> vc = v;
                const auto& type = getType(vc->type);
                DEBUG("Found value of type %s", type.name.c_str());
                if (type.storePolicy(vc, id, from, fromlen)) {
                    storageStore(info_hash, vc);
                }
            }

            /* Note that if storage_store failed, we lie to the requestor.
               This is to prevent them from backtracking, and hence
               polluting the DHT. */
            DEBUG("Sending announceValue confirmation.");
            sendValueAnnounced(from, fromlen, tid, v->id);
        }
    }
}

void
Dht::periodic(const uint8_t *buf, size_t buflen,
             const sockaddr *from, socklen_t fromlen,
             time_t *tosleep)
{
    gettimeofday(&now, nullptr);

    processMessage(buf, buflen, from, fromlen);

    if (now.tv_sec >= rotate_secrets_time)
        rotateSecrets();

    if (now.tv_sec >= expire_stuff_time) {
        expireBuckets(buckets);
        expireBuckets(buckets6);
        expireStorage();
        expireSearches();
    }

    if (search_time > 0 && now.tv_sec >= search_time) {
        DEBUG("search_time");
        search_time = 0;
        for (auto& sr : searches) {
            time_t tm = sr.getNextStepTime(types, now.tv_sec);
            if (tm == 0) continue;
            if (tm <= now.tv_sec) {
                searchStep(sr);
                tm = sr.getNextStepTime(types, now.tv_sec);
            }
            if (tm != 0 && (search_time == 0 || search_time > tm))
                search_time = tm;
        }
        if (search_time == 0)
            DEBUG("next search_time : (none)");
        else if (search_time < now.tv_sec)
            DEBUG("next search_time : %lu (ASAP)");
        else
            DEBUG("next search_time : %lu (in %lu s)", search_time, search_time-now.tv_sec);
    }

    if (now.tv_sec >= confirm_nodes_time) {
        //DEBUG("confirm_nodes_time");
        bool soon = false;

        soon |= bucketMaintenance(buckets);
        soon |= bucketMaintenance(buckets6);

        if (!soon) {
//            DEBUG("!soon");
            if (mybucket_grow_time >= now.tv_sec - 150)
                soon |= neighbourhoodMaintenance(buckets);
            if (mybucket6_grow_time >= now.tv_sec - 150)
                soon |= neighbourhoodMaintenance(buckets6);
        }

        /* In order to maintain all buckets' age within 600 seconds, worst
           case is roughly 27 seconds, assuming the table is 22 bits deep.
           We want to keep a margin for neighborhood maintenance, so keep
           this within 25 seconds. */
        auto time_dis = soon ?
               std::uniform_int_distribution<time_t> {5 , 25}
             : std::uniform_int_distribution<time_t> {60, 180};
        confirm_nodes_time = now.tv_sec + time_dis(rd);


        dumpTables();
    }

    if (confirm_nodes_time > now.tv_sec)
        *tosleep = confirm_nodes_time - now.tv_sec;
    else
        *tosleep = 0;

    if (search_time > 0) {
        if (search_time <= now.tv_sec)
            *tosleep = 0;
        else if (*tosleep > search_time - now.tv_sec)
            *tosleep = search_time - now.tv_sec;
    }
}

std::vector<Dht::ValuesExport>
Dht::exportValues() const
{
    std::vector<ValuesExport> e {};
    e.reserve(store.size());
    for (const auto& h : store) {
        ValuesExport ve;
        ve.first = h.id;
        serialize<uint16_t>(h.values.size(), ve.second);
        for (const auto& v : h.values) {
            Blob vde;
            serialize<time_t>(v.time, ve.second);
            v.data->pack(ve.second);
        }
        e.push_back(std::move(ve));
    }
    return e;
}

void
Dht::importValues(const std::vector<ValuesExport>& import)
{
    for (const auto& h : import) {
        if (h.second.empty())
            continue;
        auto b = h.second.begin(),
             e = h.second.end();
        try {
            const size_t n_vals = deserialize<uint16_t>(b, e);
            for (unsigned i = 0; i < n_vals; i++) {
                time_t val_time;
                Value tmp_val;
                try {
                    val_time = deserialize<time_t>(b, e);
                    tmp_val.unpack(b, e);
                } catch (const std::exception&) {
                    ERROR("Error reading value at %s", h.first.toString().c_str());
                    continue;
                }
                auto st = storageStore(h.first, std::make_shared<Value>(std::move(tmp_val)));
                st->time = val_time;
            }
        } catch (const std::exception&) {
            ERROR("Error reading values at %s", h.first.toString().c_str());
            continue;
        }
    }
}


std::vector<Dht::NodeExport>
Dht::exportNodes()
{
    std::vector<NodeExport> nodes;
    const auto b4 = buckets.findBucket(myid);
    if (b4 != buckets.end()) {
        for (auto& n : b4->nodes)
            if (n.isGood(now.tv_sec))
                nodes.push_back(n.exportNode());
    }
    const auto b6 = buckets6.findBucket(myid);
    if (b6 != buckets6.end()) {
        for (auto& n : b6->nodes)
            if (n.isGood(now.tv_sec))
                nodes.push_back(n.exportNode());
    }
    for (auto b = buckets.begin(); b != buckets.end(); ++b) {
        if (b == b4) continue;
        for (auto& n : b->nodes)
            if (n.isGood(now.tv_sec))
                nodes.push_back(n.exportNode());
    }
    for (auto b = buckets6.begin(); b != buckets6.end(); ++b) {
        if (b == b6) continue;
        for (auto& n : b->nodes)
            if (n.isGood(now.tv_sec))
                nodes.push_back(n.exportNode());
    }
    return nodes;
}

bool
Dht::insertNode(const InfoHash& id, const sockaddr *sa, socklen_t salen)
{
    if (sa->sa_family != AF_INET && sa->sa_family != AF_INET6) {
        errno = EAFNOSUPPORT;
        return false;
    }
    Node *n = newNode(id, sa, salen, 0);
    return !!n;
}

int
Dht::pingNode(const sockaddr *sa, socklen_t salen)
{
    DEBUG("Sending ping.");
    return sendPing(sa, salen, TransId {TransPrefix::PING});
}

/* We could use a proper bencoding printer and parser, but the format of
   DHT messages is fairly stylised, so this seemed simpler. */

#define CHECK(offset, delta, size)                      \
    if (offset + delta > size) throw std::overflow_error("Provided buffer is not large enough.");

#define INC(offset, delta, size)                        \
    if (delta < 0) throw std::overflow_error("Provided buffer is not large enough."); \
    CHECK(offset, (size_t)delta, size);                         \
    offset += delta

#define COPY(buf, offset, src, delta, size)             \
    CHECK(offset, delta, size);                         \
    memcpy(buf + offset, src, delta);                   \
    offset += delta;

#define ADD_V(buf, offset, size)                        \
    if (have_v) {                                       \
        COPY(buf, offset, my_v, sizeof(my_v), size);    \
    }

int
Dht::send(const void *buf, size_t len, int flags, const sockaddr *sa, socklen_t salen)
{
    if (salen == 0)
        abort();

    if (isNodeBlacklisted(sa, salen)) {
        DEBUG("Attempting to send to blacklisted node.");
        errno = EPERM;
        return -1;
    }

    int s;
    if (sa->sa_family == AF_INET)
        s = dht_socket;
    else if (sa->sa_family == AF_INET6)
        s = dht_socket6;
    else
        s = -1;

    if (s < 0) {
        errno = EAFNOSUPPORT;
        return -1;
    }
    return sendto(s, buf, len, flags, sa, salen);
}

int
Dht::sendPing(const sockaddr *sa, socklen_t salen, TransId tid)
{
    char buf[512];
    int i = 0, rc;
    rc = snprintf(buf + i, 512 - i, "d1:ad2:id20:"); INC(i, rc, 512);
    COPY(buf, i, myid.data(), myid.size(), 512);
    rc = snprintf(buf + i, 512 - i, "e1:q4:ping1:t%d:", tid.length);
    INC(i, rc, 512);
    COPY(buf, i, tid.data(), tid.length, 512);
    ADD_V(buf, i, 512);
    rc = snprintf(buf + i, 512 - i, "1:y1:qe"); INC(i, rc, 512);
    return send(buf, i, 0, sa, salen);
}

int
Dht::sendPong(const sockaddr *sa, socklen_t salen, TransId tid)
{
    char buf[512];
    int i = 0, rc;
    rc = snprintf(buf + i, 512 - i, "d1:rd2:id20:"); INC(i, rc, 512);
    COPY(buf, i, myid.data(), myid.size(), 512);
    rc = snprintf(buf + i, 512 - i, "e1:t%d:", tid.length); INC(i, rc, 512);
    COPY(buf, i, tid.data(), tid.length, 512);
    ADD_V(buf, i, 512);
    rc = snprintf(buf + i, 512 - i, "1:y1:re"); INC(i, rc, 512);
    return send(buf, i, 0, sa, salen);
}

int
Dht::sendFindNode(const sockaddr *sa, socklen_t salen, TransId tid,
               const InfoHash& target, int want, int confirm)
{
    constexpr const size_t BUF_SZ = 512;
    char buf[BUF_SZ];
    int i = 0, rc;
    rc = snprintf(buf + i, BUF_SZ - i, "d1:ad2:id20:"); INC(i, rc, BUF_SZ);
    COPY(buf, i, myid.data(), myid.size(), BUF_SZ);
    rc = snprintf(buf + i, BUF_SZ - i, "6:target20:"); INC(i, rc, BUF_SZ);
    COPY(buf, i, target.data(), target.size(), BUF_SZ);
    if (want > 0) {
        rc = snprintf(buf + i, BUF_SZ - i, "4:wantl%s%se",
                      (want & WANT4) ? "2:n4" : "",
                      (want & WANT6) ? "2:n6" : "");
        INC(i, rc, BUF_SZ);
    }
    rc = snprintf(buf + i, BUF_SZ - i, "e1:q9:find_node1:t%d:", tid.length);
    INC(i, rc, BUF_SZ);
    COPY(buf, i, tid.data(), tid.length, BUF_SZ);
    ADD_V(buf, i, BUF_SZ);
    rc = snprintf(buf + i, BUF_SZ - i, "1:y1:qe"); INC(i, rc, BUF_SZ);
    return send(buf, i, confirm ? MSG_CONFIRM : 0, sa, salen);
}

int
Dht::sendNodesValues(const sockaddr *sa, socklen_t salen, TransId tid,
                 const uint8_t *nodes, unsigned nodes_len,
                 const uint8_t *nodes6, unsigned nodes6_len,
                 Storage *st, const Blob& token)
{
    constexpr const size_t BUF_SZ = 2048 * 64;
    char buf[BUF_SZ];
    int i = 0, rc;

    rc = snprintf(buf + i, BUF_SZ - i, "d1:rd2:id20:"); INC(i, rc, BUF_SZ);
    COPY(buf, i, myid.data(), myid.size(), BUF_SZ);
    if (nodes_len > 0) {
        rc = snprintf(buf + i, BUF_SZ - i, "5:nodes%u:", nodes_len);
        INC(i, rc, BUF_SZ);
        COPY(buf, i, nodes, nodes_len, BUF_SZ);
    }
    if (nodes6_len > 0) {
         rc = snprintf(buf + i, BUF_SZ - i, "6:nodes6%u:", nodes6_len);
         INC(i, rc, BUF_SZ);
         COPY(buf, i, nodes6, nodes6_len, BUF_SZ);
    }
    if (not token.empty()) {
        rc = snprintf(buf + i, BUF_SZ - i, "5:token%lu:", token.size());
        INC(i, rc, BUF_SZ);
        COPY(buf, i, token.data(), token.size(), BUF_SZ);
    }

    if (st && st->values.size() > 0) {
        /* We treat the storage as a circular list, and serve a randomly
           chosen slice.  In order to make sure we fit,
           we limit ourselves to 50 values. */
        std::uniform_int_distribution<> pos_dis(0, st->values.size()-1);
        unsigned j0 = pos_dis(rd);
        unsigned j = j0;
        unsigned k = 0;

        rc = snprintf(buf + i, BUF_SZ - i, "6:valuesl"); INC(i, rc, BUF_SZ);
        do {
            Blob packed_value;
            st->values[j].data->pack(packed_value);
            rc = snprintf(buf + i, BUF_SZ - i, "%lu:", packed_value.size()); INC(i, rc, BUF_SZ);
            COPY(buf, i, packed_value.data(), packed_value.size(), BUF_SZ);
            k++;
            j = (j + 1) % st->values.size();
        } while (j != j0 && k < 50);
        rc = snprintf(buf + i, BUF_SZ - i, "e"); INC(i, rc, BUF_SZ);
    }

    rc = snprintf(buf + i, BUF_SZ - i, "e1:t%d:", tid.length); INC(i, rc, BUF_SZ);
    COPY(buf, i, tid.data(), tid.length, BUF_SZ);
    ADD_V(buf, i, BUF_SZ);
    rc = snprintf(buf + i, BUF_SZ - i, "1:y1:re"); INC(i, rc, BUF_SZ);

    return send(buf, i, 0, sa, salen);
}

unsigned
Dht::insertClosestNode(uint8_t *nodes, unsigned numnodes, const InfoHash& id, const Node& n)
{
    unsigned i, size;

    if (n.ss.ss_family == AF_INET)
        size = HASH_LEN + sizeof(in_addr) + sizeof(in_port_t); // 26
    else if (n.ss.ss_family == AF_INET6)
        size = HASH_LEN + sizeof(in6_addr) + sizeof(in_port_t); // 38
    else
        return numnodes;

    for (i = 0; i < numnodes; i++) {
        const InfoHash* nid = reinterpret_cast<const InfoHash*>(nodes + size * i);
        if (InfoHash::cmp(n.id, *nid) == 0)
            return numnodes;
        if (id.xorCmp(n.id, *nid) < 0)
            break;
    }

    if (i >= 8)
        return numnodes;

    if (numnodes < 8)
        numnodes++;

    if (i < numnodes - 1)
        memmove(nodes + size * (i + 1), nodes + size * i, size * (numnodes - i - 1));

    if (n.ss.ss_family == AF_INET) {
        sockaddr_in *sin = (sockaddr_in*)&n.ss;
        memcpy(nodes + size * i, n.id.data(), HASH_LEN);
        memcpy(nodes + size * i + HASH_LEN, &sin->sin_addr, sizeof(in_addr));
        memcpy(nodes + size * i + HASH_LEN + sizeof(in_addr), &sin->sin_port, 2);
    }
    else if (n.ss.ss_family == AF_INET6) {
        sockaddr_in6 *sin6 = (sockaddr_in6*)&n.ss;
        memcpy(nodes + size * i, n.id.data(), HASH_LEN);
        memcpy(nodes + size * i + HASH_LEN, &sin6->sin6_addr, sizeof(in6_addr));
        memcpy(nodes + size * i + HASH_LEN + sizeof(in6_addr), &sin6->sin6_port, 2);
    }

    return numnodes;
}

unsigned
Dht::bufferClosestNodes(uint8_t *nodes, unsigned numnodes, const InfoHash& id, const Bucket& b) const
{
    for (auto& n : b.nodes) {
        if (n.isGood(now.tv_sec))
            numnodes = insertClosestNode(nodes, numnodes, id, n);
    }
    return numnodes;
}

int
Dht::sendClosestNodes(const sockaddr *sa, socklen_t salen, TransId tid,
                    const InfoHash& id, int want, const Blob& token, Storage *st)
{
    uint8_t nodes[8 * 26];
    uint8_t nodes6[8 * 38];
    unsigned numnodes = 0, numnodes6 = 0;

    if (want < 0)
        want = sa->sa_family == AF_INET ? WANT4 : WANT6;

    if ((want & WANT4)) {
        auto b = buckets.findBucket(id);
        if (b != buckets.end()) {
            numnodes = bufferClosestNodes(nodes, numnodes, id, *b);
            if (std::next(b) != buckets.end())
                numnodes = bufferClosestNodes(nodes, numnodes, id, *std::next(b));
            if (b != buckets.begin())
                numnodes = bufferClosestNodes(nodes, numnodes, id, *std::prev(b));
        }
    }

    if ((want & WANT6)) {
        auto b = buckets6.findBucket(id);
        if (b != buckets6.end()) {
            numnodes6 = bufferClosestNodes(nodes6, numnodes6, id, *b);
            if (std::next(b) != buckets6.end())
                numnodes6 = bufferClosestNodes(nodes6, numnodes6, id, *std::next(b));
            if (b != buckets6.begin())
                numnodes6 = bufferClosestNodes(nodes6, numnodes6, id, *std::prev(b));
        }
    }
    DEBUG("sending closest nodes (%d+%d nodes.)", numnodes, numnodes6);

    try {
        return sendNodesValues(sa, salen, tid,
                                nodes, numnodes * 26,
                                nodes6, numnodes6 * 38,
                                st, token);
    } catch (const std::overflow_error& e) {
        ERROR("Can't send value: buffer not large enough !");
        return -1;
    }
}

int
Dht::sendGetValues(const sockaddr *sa, socklen_t salen,
               TransId tid, const InfoHash& infohash,
               int want, int confirm)
{
    const size_t BUF_SZ = 2048 * 4;
    char buf[BUF_SZ];
    size_t i = 0;
    int rc;

    rc = snprintf(buf + i, BUF_SZ - i, "d1:ad2:id20:"); INC(i, rc, BUF_SZ);
    COPY(buf, i, myid.data(), myid.size(), BUF_SZ);
    rc = snprintf(buf + i, BUF_SZ - i, "9:info_hash20:"); INC(i, rc, BUF_SZ);
    COPY(buf, i, infohash.data(), infohash.size(), BUF_SZ);
    if (want > 0) {
        rc = snprintf(buf + i, BUF_SZ - i, "4:wantl%s%se",
                      (want & WANT4) ? "2:n4" : "",
                      (want & WANT6) ? "2:n6" : "");
        INC(i, rc, BUF_SZ);
    }
    rc = snprintf(buf + i, BUF_SZ - i, "e1:q9:get_peers1:t%d:", tid.length);
    INC(i, rc, BUF_SZ);
    COPY(buf, i, tid.data(), tid.length, BUF_SZ);
    ADD_V(buf, i, BUF_SZ);
    rc = snprintf(buf + i, BUF_SZ - i, "1:y1:qe"); INC(i, rc, BUF_SZ);
    return send(buf, i, confirm ? MSG_CONFIRM : 0, sa, salen);
}

int
Dht::sendAnnounceValue(const sockaddr *sa, socklen_t salen, TransId tid,
                   const InfoHash& infohash, const Value& value,
                   const Blob& token, int confirm)
{
    const size_t BUF_SZ = 2048 * 4;
    char buf[BUF_SZ];
    size_t i = 0;
    int rc;

    rc = snprintf(buf + i, BUF_SZ - i, "d1:ad2:id%lu:", myid.size()); INC(i, rc, BUF_SZ);
    COPY(buf, i, myid.data(), myid.size(), BUF_SZ);
    rc = snprintf(buf + i, BUF_SZ - i, "9:info_hash%lu:", infohash.size()); INC(i, rc, BUF_SZ);
    COPY(buf, i, infohash.data(), infohash.size(), BUF_SZ);

    Blob packed_value;
    value.pack(packed_value);
    rc = snprintf(buf + i, BUF_SZ - i, "6:valuesl%lu:", packed_value.size()); INC(i, rc, BUF_SZ);
    COPY(buf, i, packed_value.data(), packed_value.size(), BUF_SZ);
    rc = snprintf(buf + i, BUF_SZ - i, "e5:token%lu:", token.size()); INC(i, rc, BUF_SZ);
    COPY(buf, i, token.data(), token.size(), BUF_SZ);
    rc = snprintf(buf + i, BUF_SZ - i, "e1:q13:announce_peer1:t%u:", tid.length); INC(i, rc, BUF_SZ);
    COPY(buf, i, tid.data(), tid.length, BUF_SZ);
    ADD_V(buf, i, BUF_SZ);
    rc = snprintf(buf + i, BUF_SZ - i, "1:y1:qe"); INC(i, rc, BUF_SZ);

    return send(buf, i, confirm ? 0 : MSG_CONFIRM, sa, salen);
}

int
Dht::sendValueAnnounced(const sockaddr *sa, socklen_t salen, TransId tid, Value::Id vid)
{
    char buf[512];
    int i = 0, rc;

    rc = snprintf(buf + i, 512 - i, "d1:rd2:id20:"); INC(i, rc, 512);
    COPY(buf, i, myid.data(), myid.size(), 512);
    rc = snprintf(buf + i, 512 - i, "3:vid%lu:", sizeof(Value::Id)); INC(i, rc, 512);
    COPY(buf, i, &vid, sizeof(Value::Id), 512);
    rc = snprintf(buf + i, 512 - i, "e1:t%u:", tid.length); INC(i, rc, 512);
    COPY(buf, i, tid.data(), tid.length, 512);
    ADD_V(buf, i, 512);
    rc = snprintf(buf + i, 512 - i, "1:y1:re"); INC(i, rc, 512);
    return send(buf, i, 0, sa, salen);
}

int
Dht::sendError(const sockaddr *sa, socklen_t salen, TransId tid, int code, const char *message)
{
    char buf[512];
    int i = 0, rc;

    rc = snprintf(buf + i, 512 - i, "d1:eli%de%d:", code, (int)strlen(message));
    INC(i, rc, 512);
    COPY(buf, i, message, (int)strlen(message), 512);
    rc = snprintf(buf + i, 512 - i, "e1:t%d:", tid.length); INC(i, rc, 512);
    COPY(buf, i, tid.data(), tid.length, 512);
    ADD_V(buf, i, 512);
    rc = snprintf(buf + i, 512 - i, "1:y1:ee"); INC(i, rc, 512);
    return send(buf, i, 0, sa, salen);
}

#undef CHECK
#undef INC
#undef COPY
#undef ADD_V

Dht::MessageType
Dht::parseMessage(const uint8_t *buf, size_t buflen,
              TransId& tid_return,
              InfoHash& id_return, InfoHash& info_hash_return,
              InfoHash& target_return, in_port_t& port_return,
              Blob& token, Value::Id& value_id,
              uint8_t *nodes_return, unsigned *nodes_len,
              uint8_t *nodes6_return, unsigned *nodes6_len,
              std::vector<std::shared_ptr<Value>>& values_return,
              int *want_return, uint16_t& error_code)
{
    const uint8_t *p;

    /* This code will happily crash if the buffer is not NUL-terminated. */
    if (buf[buflen] != '\0')
        throw DhtException("Eek!  parse_message with unterminated buffer.");

#define CHECK(ptr, len) if (((uint8_t*)ptr) + (len) > (buf) + (buflen)) throw std::out_of_range("Truncated message.");

    p = (uint8_t*)dht_memmem(buf, buflen, "1:t", 3);
    if (p) {
        char *q;
        size_t l = strtoul((char*)p + 3, &q, 10);
        if (q && *q == ':') {
            CHECK(q + 1, l);
            tid_return = {q+1, l};
        } else
            tid_return.length = 0;
    }

    p = (uint8_t*)dht_memmem(buf, buflen, "2:id20:", 7);
    if (p) {
        CHECK(p + 7, HASH_LEN);
        memcpy(id_return.data(), p + 7, HASH_LEN);
    } else {
        id_return = {};
    }

    p = (uint8_t*)dht_memmem(buf, buflen, "9:info_hash20:", 14);
    if (p) {
        CHECK(p + 14, HASH_LEN);
        memcpy(info_hash_return.data(), p + 14, HASH_LEN);
    } else {
        info_hash_return = {};
    }

    p = (uint8_t*)dht_memmem(buf, buflen, "porti", 5);
    if (p) {
        char *q;
        unsigned long l = strtoul((char*)p + 5, &q, 10);
        if (q && *q == 'e' && l < 0x10000)
            port_return = l;
        else
            port_return = 0;
    } else
        port_return = 0;

    p = (uint8_t*)dht_memmem(buf, buflen, "6:target20:", 11);
    if (p) {
        CHECK(p + 11, HASH_LEN);
        memcpy(target_return.data(), p + 11, HASH_LEN);
    } else {
        target_return = {};
    }

    p = (uint8_t*)dht_memmem(buf, buflen, "5:token", 7);
    if (p) {
        char *q;
        size_t l = strtoul((char*)p + 7, &q, 10);
        if (q && *q == ':' && l > 0 && l <= 128) {
            CHECK(q + 1, l);
            token.clear();
            token.insert(token.begin(), q + 1, q + 1 + l);
        }
    }

    if (nodes_len) {
        p = (uint8_t*)dht_memmem(buf, buflen, "5:nodes", 7);
        if (p) {
            char *q;
            size_t l = strtoul((char*)p + 7, &q, 10);
            if (q && *q == ':' && l > 0 && l < *nodes_len) {
                CHECK(q + 1, l);
                memcpy(nodes_return, q + 1, l);
                *nodes_len = l;
            } else
                *nodes_len = 0;
        } else
            *nodes_len = 0;
    }

    if (nodes6_len) {
        p = (uint8_t*)dht_memmem(buf, buflen, "6:nodes6", 8);
        if (p) {
            char *q;
            size_t l = strtoul((char*)p + 8, &q, 10);
            if (q && *q == ':' && l > 0 && l < *nodes6_len) {
                CHECK(q + 1, l);
                memcpy(nodes6_return, q + 1, l);
                *nodes6_len = l;
            } else
                *nodes6_len = 0;
        } else
            *nodes6_len = 0;
    }

    p = (uint8_t*)dht_memmem(buf, buflen, "6:valuesl", 9);
    if (p) {
        unsigned i = p - buf + 9;
        while (true) {
            char *q;
            size_t l = strtoul((char*)buf + i, &q, 10);
            if (q && *q == ':' && l > 0) {
                CHECK(q + 1, l);
                i = q + 1 + l - (char*)buf;
                Value v;
                v.unpackBlob(Blob {q + 1, q + 1 + l});
                values_return.push_back(std::make_shared<Value>(std::move(v)));
            } else
                break;
        }
        if (i >= buflen || buf[i] != 'e')
            DEBUG("eek... unexpected end for values.");
    }

    p = (uint8_t*)dht_memmem(buf, buflen, "3:vid8:", 7);
    if (p) {
        CHECK(p + 7, sizeof(value_id));
        memcpy(&value_id, p + 7, sizeof(value_id));
    } else {
        value_id = Value::INVALID_ID;
    }

    if (want_return) {
        p = (uint8_t*)dht_memmem(buf, buflen, "4:wantl", 7);
        if (p) {
            unsigned i = p - buf + 7;
            *want_return = 0;
            while (buf[i] > '0' && buf[i] <= '9' && buf[i + 1] == ':' &&
                  i + 2 + buf[i] - '0' < buflen) {
                CHECK(buf + i + 2, buf[i] - '0');
                if (buf[i] == '2' && memcmp(buf + i + 2, "n4", 2) == 0)
                    *want_return |= WANT4;
                else if (buf[i] == '2' && memcmp(buf + i + 2, "n6", 2) == 0)
                    *want_return |= WANT6;
                else
                    DEBUG("eek... unexpected want flag (%c)", buf[i]);
                i += 2 + buf[i] - '0';
            }
            if (i >= buflen || buf[i] != 'e')
                DEBUG("eek... unexpected end for want.");
        } else {
            *want_return = -1;
        }
    }

    p = (uint8_t*)dht_memmem(buf, buflen, "1:eli", 5);
    if (p) {
        CHECK(p + 5, sizeof(error_code));
        memcpy(&error_code, p + 5, sizeof(error_code));
    } else {
        error_code = 0;
    }

#undef CHECK

    if (dht_memmem(buf, buflen, "1:y1:r", 6))
        return MessageType::Reply;
    if (dht_memmem(buf, buflen, "1:y1:e", 6))
        return MessageType::Error;
    if (!dht_memmem(buf, buflen, "1:y1:q", 6))
        throw DhtException("Parse error");
    if (dht_memmem(buf, buflen, "1:q4:ping", 9))
        return MessageType::Ping;
    if (dht_memmem(buf, buflen, "1:q9:find_node", 14))
       return MessageType::FindNode;
    if (dht_memmem(buf, buflen, "1:q9:get_peers", 14))
        return MessageType::GetValues;
    if (dht_memmem(buf, buflen, "1:q13:announce_peer", 19))
       return MessageType::AnnounceValue;
    throw DhtException("Can't read message type.");
}

#ifdef HAVE_MEMMEM

void *
Dht::dht_memmem(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen)
{
    return memmem(haystack, haystacklen, needle, needlelen);
}

#else

void *
Dht::dht_memmem(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen)
{
    const char *h = (const char *)haystack;
    const char *n = (const char *)needle;
    size_t i;

    /* size_t is unsigned */
    if (needlelen > haystacklen)
        return NULL;

    for (i = 0; i <= haystacklen - needlelen; i++) {
        if (memcmp(h + i, n, needlelen) == 0)
            return (void*)(h + i);
    }
    return NULL;
}

#endif

}
