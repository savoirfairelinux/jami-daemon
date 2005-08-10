/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
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

#ifndef __TONE_GENRATOR_H__
#define __TONE_GENRATOR_H__

#include <string>

#include "../global.h"
#include "../manager.h"

using namespace std;
using namespace ost;


#define ZT_TONE_DIALTONE	0
#define ZT_TONE_BUSY		1
#define ZT_TONE_RINGTONE	2
#define ZT_TONE_CONGESTION	3

#define NB_TONES_MAX		4
#define NB_ZONES_MAX		7

#define	ID_NORTH_AMERICA	0	
#define ID_FRANCE			1
#define	ID_AUSTRALIA		2
#define	ID_UNITED_KINGDOM	3
#define	ID_SPAIN			4
#define	ID_ITALY			5
#define	ID_JAPAN			6


///////////////////////////////////////////////////////////////////////////////
// ToneThread 
///////////////////////////////////////////////////////////////////////////////
class ToneThread : public Thread {
public:
	ToneThread (int16 *, int);
	virtual ~ToneThread (void);

	virtual void run ();
private:
	int16*	buffer;
	int16*	buf_ctrl_vol;
	int			size;
};

///////////////////////////////////////////////////////////////////////////////
// ToneGenerator
///////////////////////////////////////////////////////////////////////////////
class ToneGenerator {
public:
	ToneGenerator ();
	~ToneGenerator (void);
	
	/**
 	 * Returns id selected zone for tone choice
 	*/
	int idZoneName 		(const string &);
			
	/**
	 * Calculate sinus with superposition of 2 frequencies
	 */
	void generateSin	(int, int, int16 *);

	/**
 	 * Build tone according to the id-zone, with initialisation of ring tone.
 	 * Generate sinus with frequencies alternatively by time
 	 */
	void buildTone		(int, int, int16*);

	/**
	 * Handle the required tone
	 */
	void toneHandle 	(int);

	/**
	 * Play the ringtone when incoming call occured
	 */
	int  playRingtone		(const char*);
	
	///////////////////////////
	// Public members variable
	//////////////////////////
	int16 *sample;
	int freq1, 
		freq2;
	int time;
	int totalbytes;
	int16   	buf[SIZEBUF];
	
private:
	/*
	 * Initialisation of the supported tones according to the countries.
	 */
	void		initTone (void);

	/*
	 * Count all the occurences of a character in a string
	 *
	 * @param	c character to locate
	 * @param	str	string to work on
	 * @return	return the number of time 'c' is found in 'str'
	 */
	int		 	contains(const string& str, char c);
	
	//////////////////////////
	// Private member variable
	//////////////////////////
	string toneZone[NB_ZONES_MAX][NB_TONES_MAX];
	ToneThread*	tonethread;
	
};

#endif // __TONE_GENRATOR_H__
