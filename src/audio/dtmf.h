/*
 *  Copyright (C) 2004-2005-2006 Savoir-Faire Linux inc.
 *  Author : Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com> 
 *
 * 	Portions Copyright (c) 2000 Billy Biggs <bbiggs@div8.net>
 *  Portions Copyright (c) 2004 Wirlab <kphone@wirlab.net>
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

#ifndef __DTMF_H_
#define __DTMF_H_

#include "../global.h"
#include "dtmfgenerator.h"

/**
 * DMTF library to generate a dtmf sample
 */
class DTMF {
public:
       /**
        * Create a new DTMF.
        * @param samplingRate frequency of the sample (ex: 8000 hz)
        */
	DTMF (unsigned int sampleRate, unsigned int nbChannel);
	~DTMF (void);
	
	void startTone		(char);
	/**
	 * Copy the sound inside the int16* buffer 
	 * @param buffer : a int16* buffer
	 * @param n      : 
	 */
	bool generateDTMF	(int16* buffer, size_t n);

	char currentTone;
	char newTone;

	DTMFGenerator dtmfgenerator;
};

#endif // __KEY_DTMF_H_
