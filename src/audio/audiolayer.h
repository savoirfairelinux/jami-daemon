/*
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

#ifndef _AUDIO_LAYER_H
#define _AUDIO_LAYER_H

#include <cc++/thread.h> // for ost::Mutex

#include "portaudiocpp/PortAudioCpp.hxx"

#include "../global.h"
#include "ringbuffer.h"

#define FRAME_PER_BUFFER	160

class RingBuffer;

class AudioLayer {
public:
  AudioLayer();
  ~AudioLayer(void);
 
  /*
   * @param indexIn
   * @param indexOut
   * @param sampleRate
   */
  void openDevice(int, int, int);
  void startStream(void);
  void stopStream(void);
  void sleep(int);
  bool hasStream(void);
  bool isStreamActive(void);
  bool isStreamStopped(void);

  void flushMain();
  int putMain(void* buffer, int toCopy);
  int putUrgent(void* buffer, int toCopy);
  int canGetMic();
  int getMic(void *, int);
  void flushMic();

/**
 * Try to convert a int16 fromBuffer into a int16 to Buffer
 * If the channel are the same, it only pass the address and the size of the from Buffer
 * Else, it double or reduce by 2 the fromBuffer with a 0.5 conversion
 */
  unsigned int convert(int16* fromBuffer, int fromChannel, unsigned int fromSize, int16** toBuffer, int toChannel);

  int audioCallback (const void *, void *, unsigned long,
        const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags);

  void setErrorMessage(const std::string& error) { _errorMessage = error; }
  std::string getErrorMessage() { return _errorMessage; }

  /**
   * Get the sample rate of audiolayer
   * accessor only
   */
  unsigned int getSampleRate() { return _sampleRate; }
  unsigned int getInChannel()  { return _inChannel; }
  unsigned int getOutChannel() { return _outChannel; }

  enum IODEVICE {InputDevice=0x01, OutputDevice=0x02 };

private:
  void closeStream (void);
  RingBuffer _urgentRingBuffer;
  RingBuffer _mainSndRingBuffer;
  RingBuffer _micRingBuffer;

  portaudio::MemFunCallbackStream<AudioLayer> *_stream;
  /**
   * Sample Rate of SFLphone : should be 8000 for 8khz
   * Added because we could change it in the futur
   */
  unsigned int _sampleRate;

  /**
   * Input channel (mic) should be 2 stereo or 1 mono
   */
  unsigned int _inChannel; // mic

  /**
   * Output channel (stereo) should be 2 stereo or 1 mono
   */
  unsigned int _outChannel; // speaker

  std::string _errorMessage;
//	portaudio::AutoSystem autoSys;
  ost::Mutex _mutex;
};

#endif // _AUDIO_LAYER_H_

