/**
 *  Copyright (C) 2004 Savoir-Faire Linux inc.
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *                                                                              
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
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

#ifndef __PHONE_LINE_H__
#define __PHONE_LINE_H__

#include <qpushbutton.h>
#include <qdatetime.h>

#include "jpushbutton.h"

#define NUMBER_OF_LINES		6
#define NUMBER_OF_STATES	2 // for the init of phoneline button pixmap

/** Defines the state of a line. */
enum line_state {
	FREE = 0,	// Line is available
	BUSY,		// Line is busy
	ONHOLD,		// Line is on hold
	OFFHOLD		// Line is on hold	
};

class PhoneLine {
public:
	PhoneLine					(void);
	~PhoneLine					(void);
	enum line_state	 getState	(void);
	void			 setState	(enum line_state);
	enum line_state	 getStateLine	(void);
	void			 setStateLine	(enum line_state);
	bool			 isFree		(void);
	bool			 isBusy		(void);
	bool			 isOnHold	(void);
	void			 toggleState(void);
	void			 setButton	(JPushButton *);
	JPushButton		*button		(void);
	void			 startTimer	(void);
	void			 stopTimer	(void);
	inline
	void			 setbDial	(bool dial) {b_dial = dial;}
	inline
	bool			 getbDial	(void) {return b_dial;}
	QTime			*timer;
	QString			 status;
	QString			 text;
	bool			 first;
	bool			 b_dial;	

private:
	JPushButton			*jpb;
	enum line_state		 state;
	enum line_state		 stateLine;
};

#endif	// __PHONE_LINE_H__
