/*
 *  Copyright (C) 2014 Savoir-Faire Linux Inc.
 *  Author: Philippe Proulx <philippe.proulx@savoirfairelinux.com>
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */
#ifndef DRING_H
#define DRING_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vector>
#include <functional>
#include <string>
#include <map>

enum class EventHandlerKey { CALL,
                            CONFIG,
                            PRESENCE,
                            VIDEO};

const char *
ring_version();

/* error codes returned by functions of this API */
enum ring_error {
    RING_ERR_MANAGER_INIT,
    RING_ERR_UNKNOWN,
};

/* flags for initialization */
enum ring_init_flag {
    RING_FLAG_DEBUG = 1,
    RING_FLAG_CONSOLE_LOG = 2,
};

/**
 * Initializes libring.
 *
 * @param ev_handlers Event handlers
 * @param flags       Flags to customize this initialization
 * @returns           0 if successful or a negative error code
 */
int ring_init(const std::map<EventHandlerKey, void*>& ev_handlers, enum ring_init_flag flags);

/**
 * Finalizes libring, freeing any resource allocated by the library.
 */
void ring_fini(void);

/**
 * Poll for SIP/IAX events
 */
void ring_poll_events(void);

#endif /* RING_H */
