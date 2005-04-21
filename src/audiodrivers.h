/**
 *  Copyright (C) 2004 Savoir-Faire Linux inc.
 *  Author : Laurielle Lea <laurielle.lea@savoirfairelinux.com> 
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

#ifndef __AUDIO_DRIVERS_H__
#define __AUDIO_DRIVERS_H__

#include "audiobuffer.h"

class AudioDrivers {
public:
	AudioDrivers (void);
	virtual	~AudioDrivers (void);
	AudioBuffer audio_buf; // Buffer that the application fills	

	enum DeviceState {
		DeviceOpened,
		DeviceClosed };

	enum DeviceMode {
		ReadOnly,
		WriteOnly,
		ReadWrite };

	virtual int	 initDevice			(DeviceMode) = 0;
	virtual int	 resetDevice		(void) = 0;
	virtual int  writeBuffer		(void) = 0;
	virtual int	 readBuffer			(void *, int) = 0;
	virtual int	 readBuffer			(int) = 0;
	virtual unsigned int readableBytes (void) = 0;


protected:
	DeviceState devstate;  // Current state
	DeviceMode devmode;    // Current mode
};


#endif// __AUDIO_DRIVERS_H__
