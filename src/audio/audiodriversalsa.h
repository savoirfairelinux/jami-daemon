/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author:  Jerome Oufella <jerome.oufella@savoirfairelinux.com> 
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

//#ifdef AUDIO_ALSA

#ifndef _AUDIO_DRIVERS_ALSA_H_
#define _AUDIO_DRIVERS_ALSA_H_

#include <alsa/asoundlib.h>

#include "audiodrivers.h"
#include "../error.h"


/**
 * This is the ALSA implementation of DspOut.
 * Note that you cannot change how many fragments
 * this class requests, yet.
 */
class AudioDriversALSA : public AudioDrivers {
public:
	/**
	 * Constructs a AudioDriversALSA object representing the given
	 * filename.  Default is /dev/dsp.
	 */
	AudioDriversALSA(DeviceMode, Error*);

	/**
	 * Destructor.  Will close the device if it is open.
	 */
	virtual ~AudioDriversALSA( void );

	int 	initDevice	(DeviceMode);
	int		resetDevice	(void);	
	int 	writeBuffer	(void);
	int		readBuffer 	(void *, int);
	unsigned int readableBytes (void) { return 0; }
	
private:
	Error *	error;
	
	snd_pcm_t *audio_hdl;
	void closeDevice (void);
};

#endif  // _AUDIO_DRIVERS_ALSA_H_

//#endif // defined(AUDIO_ALSA)
