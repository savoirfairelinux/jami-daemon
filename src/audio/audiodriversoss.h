/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author:  Laurielle Lea <laurielle.lea@savoirfairelinux.com> 
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

#ifndef _AUDIO_DRIVERS_OSS_H
#define _AUDIO_DRIVERS_OSS_H


#include "audiodrivers.h"
#include "../error.h"

// TODO : a mettre dans config
#define AUDIO_DEVICE	"/dev/dsp"

class AudioDriversOSS : public AudioDrivers {
public:
	AudioDriversOSS (DeviceMode, Error*);
	~AudioDriversOSS (void);

	int		initDevice		(DeviceMode);
	int		resetDevice		(void);
	bool	openDevice 		(int);
	int	 	writeBuffer		(void *, int);
	int	 	writeBuffer		(void);
	int 	readBuffer		(void *, int);
	int 	readBuffer		(int);
	unsigned int readableBytes (void);

	int audio_fd;
private:
	int		closeDevice		(void);
	Error *	error;
};

#endif // _AUDIO_DRIVERS_OSS_H
