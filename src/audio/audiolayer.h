/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
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

#ifndef _AUDIO_LAYER_H
#define _AUDIO_LAYER_H

#include "portaudiocpp/PortAudioCpp.hxx"


#include "../global.h"
#include "ringbuffer.h"
#include <cc++/thread.h>
#include "../manager.h"

#define FRAME_PER_BUFFER	160
#define MIC_CHANNELS 		2 // 1=mono 2=stereo
#define SAMPLE_BYTES 		sizeof(int16)
#define SAMPLES_SIZE(i) 	(i * MIC_CHANNELS * SAMPLE_BYTES)


class RingBuffer;

class AudioLayer {
public:
	AudioLayer();
	~AudioLayer (void);

	void listDevices();
	void	openDevice 		(int);
	void 	startStream		(void);
	void 	stopStream		(void);
	void    sleep			(int);
	bool    isStreamActive	        (void);
	bool    isStreamStopped	        (void);

  void flushMain();
  void putMain(void* buffer, int toCopy);
  void putUrgent(void* buffer, int toCopy);
  void flushMic();

	int audioCallback (const void *, void *, unsigned long,
			   const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags);

	inline RingBuffer &urgentRingBuffer(void) { return _urgentRingBuffer; }
	inline RingBuffer &micRingBuffer(void) { return _micRingBuffer; }

private:
	void	closeStream	(void);
	RingBuffer _urgentRingBuffer;
	RingBuffer _mainSndRingBuffer;
	RingBuffer _micRingBuffer;

	portaudio::MemFunCallbackStream<AudioLayer> *_stream;
//	portaudio::AutoSystem autoSys;
  ost::Mutex _mutex;
  int NBCHARFORTWOINT16;
};

#endif // _AUDIO_LAYER_H_

#endif // defined(AUDIO_PORTAUDIO)
