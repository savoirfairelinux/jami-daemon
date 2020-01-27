/*
 *  Copyright (C) 2014-2020 Savoir-faire Linux Inc.
 *  Author(s) : Paymon <paymon@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef CONNSTAT_H
#define CONNSTAT_H

#include "def.h"

#include <functional>

namespace connstat {

enum supported_topics {
    NONE_TOPIC,
    NEWLINK,
    NEWROUTE4,
    NEWROUTE6,
    NEWROUTE,
    NEWADDR4,
    NEWADDR6,
    NEWADDR,
    NEWNEIGH,
    NEIGHTBL,
    IPV4_MROUTE,
    IPV6_MROUTE,
    IP_MROUTE,
    CONNSTAT_DEL_TOPICS,
    DELLINK,
    DELROUTE4,
    DELROUTE6,
    DELROUTE,
    DELADDR4,
    DELADDR6,
    DELADDR,
    DELNEIGH,
    CONNSTAT_TOPICS_MAX
};


// typedef void cb(void);
using cb = std::function<void (unsigned int)>;


class DRING_PUBLIC connstat
{
public:

    connstat();
    ~connstat();

    unsigned int addtopic(unsigned int topic);
    int registerCB(cb ucb, unsigned int topic);
    int uRegisterCB(cb ucb, unsigned int topic);
    void executer(unsigned int);


private:
    static   void*                nlibsk;
    static   int                  nclients;
    static   struct cb_by_topic   cbs_by_topics[CONNSTAT_TOPICS_MAX];

    int      install_cb(cb user_cb, unsigned int topic);
    int      nlsk_init(void);
    int      nlsk_setup(void);
    void     nlsk_unplug();
    void     nlsk_unsubscribe();
};

} /* namespace connstat */

#endif /* DRING_CONNSTAT_H */
