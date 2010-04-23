/*
 *  Copyright (C) 2008 2009 Savoir-Faire Linux inc.
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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

#include "echocancel.h"

#include <iostream>


#define FRAME_SIZE 160
#define FILTER_LENGTH 500

EchoCancel::EchoCancel() 
{
  _debug("EchoCancel: Instantiate echo canceller");

  _echoState = speex_echo_state_init(FRAME_SIZE, FILTER_LENGTH);

  int sampleRate = 8000;

  _preState = speex_preprocess_state_init(FRAME_SIZE, sampleRate);

  speex_echo_ctl(_echoState, SPEEX_ECHO_SET_SAMPLING_RATE, &sampleRate);


  _micData = new RingBuffer(5000);
  _spkrData = new RingBuffer(5000);

  _micData->createReadPointer();
  _spkrData->createReadPointer();

  _spkrStoped = true;
}

EchoCancel::~EchoCancel() 
{
  _debug("EchoCancel: Delete echo canceller");

  speex_echo_state_destroy(_echoState);
  _echoState = NULL;

  delete _micData;
  _micData = NULL;

  delete _spkrData;
  _spkrData = NULL;
}

void EchoCancel::putData(SFLDataFormat *inputData, int nbBytes) 
{
  std::cout << "putData nbBytes: " << nbBytes << std::endl;

  if(_spkrStoped) {
      _micData->flushAll();
      _spkrData->flushAll();
      _spkrStoped = false;
  }

  // Put data in speaker ring buffer
  _spkrData->Put(inputData, nbBytes);
  // speex_echo_playback(_echoState, inputData);
}

int EchoCancel::process(SFLDataFormat *inputData, SFLDataFormat *outputData, int nbBytes)
{

  SFLDataFormat tmpSpkr[5000];
  SFLDataFormat tmpMic[5000];

  std::cout << "process nbBytes: " << nbBytes << std::endl;

  // Put data in microphone ring buffer
  _micData->Put(inputData, nbBytes);

  int spkrAvail = _spkrData->AvailForGet();
  int micAvail = _micData->AvailForGet();

  std::cout << "process spkrData AvailForGet: " << spkrAvail << std::endl;  
  std::cout << "process micData AvailForGet: " << micAvail << std::endl; 

  // Number of frame processed
  int nbFrame = 0;

  // Get data from mic and speaker
  while((spkrAvail > (FRAME_SIZE*2)) && (micAvail > (FRAME_SIZE*2))) {

    _spkrData->Get(&tmpSpkr, (FRAME_SIZE*2));
    _micData->Get(&tmpMic, (FRAME_SIZE*2));
    
    speex_echo_cancellation(_echoState, (const spx_int16_t*)(&tmpMic), (const spx_int16_t*)&tmpSpkr, outputData);
     speex_preprocess_run(_preState, (spx_int16_t*)(outputData));

    spkrAvail = _spkrData->AvailForGet();
    micAvail = _micData->AvailForGet();

    ++nbFrame;
  }

  return nbFrame*(FRAME_SIZE);
}

void EchoCancel::process(SFLDataFormat *micData, SFLDataFormat *spkrData, SFLDataFormat *outputData, int nbBytes){

  // speex_echo_cancellation(_echoState, micData, spkrData, outputData);

}
