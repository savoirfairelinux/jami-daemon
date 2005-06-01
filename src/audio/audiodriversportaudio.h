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

#if defined(AUDIO_PORTAUDIO)

#ifndef _AUDIO_DRIVERS_PORTAUDIO_H
#define _AUDIO_DRIVERS_PORTAUDIO_H

#include "portaudio/pa_common/portaudio.h"
#include "../global.h"

#define TABLE_SIZE			360
#define FRAME_PER_BUFFER	160

class Manager;
class AudioDriversPortAudio {
public:
	struct paData {
		float32 *dataIn;  	// From mic
		float32 *dataOut;		// To spk
		float32 *dataToAdd;		// To spk
		int   dataToAddRem;
		int   dataFilled;
	};
	paData mydata;
	
	AudioDriversPortAudio (Manager*);
	~AudioDriversPortAudio (void);

	int		resetDevice		(void);
	int		initDevice		(void);
	bool	openDevice 		(void);
	int	 	writeBuffer		(void *, int);
	int 	readBuffer		(void *, int);
	int 	startStream		(void);
	int 	stopStream		(void);
	void    sleep			(int);
	int		isStreamActive	(void);
	int		isStreamStopped	(void);
	int 	getDeviceCount	(void);

	static int audioCallback (const void *, void *, unsigned long,
			const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags, void *);

private:
	int	closeDevice	(void);
	Manager*		_manager;
	PaStream*		_stream;
};

#endif // _AUDIO_DRIVERS_PORTAUDIO_H_

#endif // defined(AUDIO_PORTAUDIO)
