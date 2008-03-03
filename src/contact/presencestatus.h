/*
 *  Copyright (C) 2008 Savoir-Faire Linux inc.
 *  Author: Guillaume Carmel-Archambault <guillaume.carmel-archambault@savoirfairelinux.com>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef PRESENCE_STATUS_H
#define PRESENCE_STATUS_H

/* Definition of all presence status used by the deamon and the GUI
 * The deamon knows how to identify tags coming from presence servers
 * and cast them in a defined presence status presented here. 
 * The presence information is transmitted along DBus by those strings.
 * The GUI can format and translate these strings for presentation. 
 * 
 * If a presence status identified by a string cannot be identified
 * when sent from a presence server, we directly use the raw string
 * without any formating or translation process possible
 */
// Same presence status as defined in Asterisk
#define PRESENCE_UNKNOWN			"UNKNOWN"
#define PRESENCE_NOT_IN_USE			"NOT_IN_USE"
#define PRESENCE_INUSE				"INUSE"
#define PRESENCE_BUSY				"BUSY"
#define PRESENCE_INVALID			"INVALID"
#define PRESENCE_UNAVAILABLE		"UNAVAILABLE"
#define PRESENCE_RINGING			"RINGING"
#define PRESENCE_RING_IN_USE		"RING_IN_USE"
#define PRESENCE_HOLD_IN_USE		"HOLD_IN_USE"
#define PRESENCE_ON_HOLD			"ON_HOLD"
// Presence status defined on some hardware phones
#define PRESENCE_ONLINE				"ONLINE"
#define PRESENCE_BUSY				"BUSY"
#define PRESENCE_BE_RIGHT_BACK		"BE_RIGHT_BACK"
#define PRESENCE_AWAY				"AWAY"
#define PRESENCE_OUT_TO_LUNCH		"OUT_TO_LUNCH"
#define PRESENCE_OFFLINE			"OFFLINE"
#define PRESENCE_DO_NOT_DISTURB		"DO_NOT_DISTURB"
// Other presence status defined supported
#define PRESENCE_IN_REUNION			"IN_REUNION"
#define PRESENCE_IN_CONFERENCE_CALL	"IN_CONFERENCE_CALL"

#endif
