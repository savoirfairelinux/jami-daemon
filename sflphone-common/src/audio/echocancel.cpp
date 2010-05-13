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

// #include <fstream>

EchoCancel::EchoCancel(int smplRate, int frameLength) : _samplingRate(smplRate),
							_frameLength(frameLength),
							_smplPerFrame(0),
							_smplPerSeg(0),
							_nbSegmentPerFrame(0),
							_historyLength(0),
							_spkrLevel(0),
							_micLevel(0),
							_spkrHistCnt(0),
							_micHistCnt(0),
							_amplFactor(0.0),
							_lastAmplFactor(0.0)
{
  _debug("EchoCancel: Instantiate echo canceller");

  /*
  micFile = new ofstream("micData", ofstream::binary);
  echoFile = new ofstream("echoData", ofstream::binary);
  spkrFile = new ofstream("spkrData", ofstream::binary);
  */

  _micData = new RingBuffer(50000);
  _spkrData = new RingBuffer(50000);

  _micData->createReadPointer();
  _spkrData->createReadPointer();

  _spkrStoped = true;

  _smplPerFrame = (_samplingRate * _frameLength) / MS_PER_SEC;
  _smplPerSeg = (_samplingRate * SEGMENT_LENGTH) / MS_PER_SEC;
  _historyLength = ECHO_LENGTH / SEGMENT_LENGTH;
  _nbSegmentPerFrame =  _frameLength / SEGMENT_LENGTH;

  _noiseState = speex_preprocess_state_init(_smplPerFrame, _samplingRate);
  int i=1;
  speex_preprocess_ctl(_noiseState, SPEEX_PREPROCESS_SET_DENOISE, &i);
  i=0;
  speex_preprocess_ctl(_noiseState, SPEEX_PREPROCESS_SET_AGC, &i);
  i=8000;
  speex_preprocess_ctl(_noiseState, SPEEX_PREPROCESS_SET_AGC_LEVEL, &i);
  i=0;
  speex_preprocess_ctl(_noiseState, SPEEX_PREPROCESS_SET_DEREVERB, &i);
  float f=.0;
  speex_preprocess_ctl(_noiseState, SPEEX_PREPROCESS_SET_DEREVERB_DECAY, &f);
  f=.0;
  speex_preprocess_ctl(_noiseState, SPEEX_PREPROCESS_SET_DEREVERB_LEVEL, &f);

  memset(_avgSpkrLevelHist, 0, BUFF_SIZE*sizeof(int));
  memset(_avgMicLevelHist, 0, BUFF_SIZE*sizeof(int));

  memset(_delayedAmplify, 0, MAX_DELAY*sizeof(float));

  _amplIndexIn = 0;
  _amplIndexOut = DELAY_AMPLIFY / SEGMENT_LENGTH;
  
}

EchoCancel::~EchoCancel() 
{
  _debug("EchoCancel: Delete echo canceller");

  delete _micData;
  _micData = NULL;

  delete _spkrData;
  _spkrData = NULL;

  speex_preprocess_state_destroy(_noiseState);

  /*
  micFile->close();
  spkrFile->close();
  echoFile->close();

  delete micFile;
  delete spkrFile;
  delete echoFile;
  */

}

void EchoCancel::reset()
{
  _debug("EchoCancel: Reset internal state, Sampling rate %d, Frame size %d", _samplingRate, _smplPerFrame);
  
  memset(_avgSpkrLevelHist, 0, BUFF_SIZE*sizeof(int));
  memset(_avgMicLevelHist, 0, BUFF_SIZE*sizeof(int));

  _spkrLevel = 0;
  _micLevel = 0;
  _spkrHistCnt = 0;
  _micHistCnt = 0;
  _amplFactor = 0.0;
  _lastAmplFactor = 0.0;

  _smplPerFrame = (_samplingRate * _frameLength) / MS_PER_SEC;
  _smplPerSeg = (_samplingRate * SEGMENT_LENGTH) / MS_PER_SEC;
  _historyLength = ECHO_LENGTH / SEGMENT_LENGTH;
  _nbSegmentPerFrame =  _frameLength / SEGMENT_LENGTH;

  memset(_delayedAmplify, 0, MAX_DELAY*sizeof(float));

  _amplIndexIn = 0;
  _amplIndexOut = DELAY_AMPLIFY / SEGMENT_LENGTH;

  _micData->flushAll();
  _spkrData->flushAll();

  speex_preprocess_state_destroy(_noiseState);

  _noiseState = speex_preprocess_state_init(_smplPerFrame, _samplingRate);
  int i=1;
  speex_preprocess_ctl(_noiseState, SPEEX_PREPROCESS_SET_DENOISE, &i);
  i=0;
  speex_preprocess_ctl(_noiseState, SPEEX_PREPROCESS_SET_AGC, &i);
  i=8000;
  speex_preprocess_ctl(_noiseState, SPEEX_PREPROCESS_SET_AGC_LEVEL, &i);
  i=0;
  speex_preprocess_ctl(_noiseState, SPEEX_PREPROCESS_SET_DEREVERB, &i);
  float f=.0;
  speex_preprocess_ctl(_noiseState, SPEEX_PREPROCESS_SET_DEREVERB_DECAY, &f);
  f=.0;
  speex_preprocess_ctl(_noiseState, SPEEX_PREPROCESS_SET_DEREVERB_LEVEL, &f);

  /*
  std::cout << "EchoCancel: _smplPerFrame " << _smplPerFrame 
            << ", _smplPerSeg " << _smplPerSeg
	    << ", _historyLength " << _historyLength
            << ", _nbSegmentPerFrame " << _nbSegmentPerFrame << std::endl;
  */

  _spkrStoped = true;
}

void EchoCancel::putData(SFLDataFormat *inputData, int nbBytes) 
{
  // std::cout << "putData nbBytes: " << nbBytes << std::endl;

  if(_spkrStoped) {
      _micData->flushAll();
      _spkrData->flushAll();
      _spkrStoped = false;
  }

  // Put data in speaker ring buffer
  _spkrData->Put(inputData, nbBytes);

  // std::cout << "EchoCancel: spkrDataAvail " << _spkrData->AvailForGet() << std::endl;

  // In case we use libspeex internal buffer 
  // (require capture and playback stream to be synchronized)
  // speex_echo_playback(_echoState, inputData);
}

void EchoCancel::process(SFLDataFormat *data, int nbBytes) {}

int EchoCancel::process(SFLDataFormat *inputData, SFLDataFormat *outputData, int nbBytes)
{

  if(_spkrStoped) {
    return 0;
  }

  int byteSize = _smplPerFrame*sizeof(SFLDataFormat);

  // init temporary buffers
  memset(_tmpSpkr, 0, BUFF_SIZE*sizeof(SFLDataFormat));
  memset(_tmpMic, 0, BUFF_SIZE*sizeof(SFLDataFormat));
  memset(_tmpOut, 0, BUFF_SIZE*sizeof(SFLDataFormat));

  // Put mic data in ringbuffer
  _micData->Put(inputData, nbBytes);

  // Store data for synchronization
  int spkrAvail = _spkrData->AvailForGet();
  int micAvail = _micData->AvailForGet();

  // std::cout << "Process echo: spkrAvail " << spkrAvail << ", micAvail " << micAvail << ", byteSize " << byteSize << std::endl;

  // Init number of frame processed
  int nbFrame = 0;

  // Get data from mic and speaker internal buffer
  while((spkrAvail >= byteSize) && (micAvail >= byteSize)) {

    // std::cout << "perform echocancel" << std::endl;

    // get synchronized data
    _spkrData->Get(_tmpSpkr, byteSize);
    _micData->Get(_tmpMic, byteSize);

    // micFile->write ((const char *)_tmpMic, byteSize);
    // spkrFile->write ((const char *)_tmpSpkr, byteSize);

    // Processed echo cancellation
    performEchoCancel(_tmpMic, _tmpSpkr, _tmpOut);

    // Remove noise
    speex_preprocess_run(_noiseState, _tmpOut);

    bcopy(_tmpOut, outputData+(nbFrame*_smplPerFrame), byteSize);

    // echoFile->write ((const char *)_tmpOut, byteSize);

    spkrAvail = _spkrData->AvailForGet();
    micAvail = _micData->AvailForGet();

    // std::cout << "Process echo remaining: spkrAvail " << spkrAvail << ", micAvail " << micAvail << std::endl;

    // increment nb of frame processed
    ++nbFrame;
  }

  return nbFrame * _smplPerFrame;
}

void EchoCancel::process(SFLDataFormat *micData, SFLDataFormat *spkrData, SFLDataFormat *outputData, int nbBytes){

}

void EchoCancel::setSamplingRate(int smplRate) {
  
  if (smplRate != _samplingRate) {
    _samplingRate = smplRate;

    /*
    if(smplRate == 16000)
      _frameLength = 10;
    else 
      _frameLength = 20;
    */

    reset();
  }
}


void EchoCancel::performEchoCancel(SFLDataFormat *micData, SFLDataFormat *spkrData, SFLDataFormat *outputData) {

  for(int k = 0; k < _nbSegmentPerFrame; k++) {

    updateEchoCancel(micData+(k*_smplPerSeg), spkrData+(k*_smplPerSeg));

    _spkrLevel = getMaxAmplitude(_avgSpkrLevelHist);
    _micLevel = getMaxAmplitude(_avgMicLevelHist);

    if(_micLevel >= MIN_SIG_LEVEL) {
      if(_spkrLevel < MIN_SIG_LEVEL) {
	increaseFactor(0.02);
      }
      else if(_micLevel > _spkrLevel) {
	increaseFactor(0.05);
      }
      else {
	decreaseFactor();
      }
    }
    else {
      if(_spkrLevel < MIN_SIG_LEVEL) {
	increaseFactor(0.02);
      }
      else {
	decreaseFactor();
      }
    }

    // lowpass filtering
    float amplify = (_lastAmplFactor + _amplFactor) / 2;

    _lastAmplFactor = _amplFactor;

    // std::cout << "Amplitude: " << amplify << ", spkrLevel: " << _spkrLevel << ", micLevel: " << _micLevel << std::endl;

    amplifySignal(micData+(k*_smplPerSeg), outputData+(k*_smplPerSeg), amplify);
    
  }
  
}


void EchoCancel::updateEchoCancel(SFLDataFormat *micData, SFLDataFormat *spkrData) {

  int micLvl = computeAmplitudeLevel(micData);
  int spkrLvl = computeAmplitudeLevel(spkrData);

  // Add 1 to make sure we are not dividing by 0
  _avgMicLevelHist[_micHistCnt++] = micLvl+1;
  _avgSpkrLevelHist[_spkrHistCnt++] = spkrLvl+1;

  if(_micHistCnt >= _historyLength)
    _micHistCnt = 0;

  if(_spkrHistCnt >= _historyLength)
    _spkrHistCnt = 0;
    
}


int EchoCancel::computeAmplitudeLevel(SFLDataFormat *data) {
  
  int level = 0;

  for(int i = 0; i < _smplPerSeg; i++) {
    if(data[i] >= 0.0)
      level += (int)data[i];
    else
      level -= (int)data[i];
  }

  level = level / _smplPerSeg;

  return level;
}


int EchoCancel::getMaxAmplitude(int *data) {

  SFLDataFormat level = 0.0;

  for(int i = 0; i < _historyLength; i++) {
    if(data[i] >= level)
      level = data[i];
  }

  return (int)level;
}


void EchoCancel::amplifySignal(SFLDataFormat *micData, SFLDataFormat *outputData, float amplify) {


  // Use delayed amplification factor due to sound card latency 
  for(int i = 0; i < _smplPerSeg; i++) {
    outputData[i] = (SFLDataFormat)(((float)micData[i])*_delayedAmplify[_amplIndexOut]);
  }

  _amplIndexOut++;
  _delayedAmplify[_amplIndexIn++] = amplify;

  if(_amplIndexOut >= MAX_DELAY)
    _amplIndexOut = 0;

  if(_amplIndexIn >= MAX_DELAY)
    _amplIndexIn = 0;
}



void EchoCancel::increaseFactor(float factor) {

  // Get 200 ms to get back to full amplitude
  _amplFactor += factor;

  if(_amplFactor > 1.0)
    _amplFactor = 1.0;

}


void EchoCancel::decreaseFactor() {

  // Takes about 50 ms to react
  _amplFactor -= 0.2;

  if(_amplFactor < 0.0)
    _amplFactor = 0.0;
}
