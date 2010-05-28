/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include "echocancel.h"

// #include <fstream>

EchoCancel::EchoCancel(int smplRate, int frameLength) : _samplingRate(smplRate),
							_frameLength(frameLength),
							_smplPerFrame(0),
							_smplPerSeg(0),
							_nbSegmentPerFrame(0),
							_micHistoryLength(0),
							_spkrHistoryLength(0),
							_spkrLevel(0),
							_micLevel(0),
							_spkrHistCnt(0),
							_micHistCnt(0),
							_amplFactor(0.0),
							_lastAmplFactor(0.0),
							_amplDelayIndexIn(0),
							_amplDelayIndexOut(0),
							_adaptDone(false),
							_adaptStarted(false),
							_adaptCnt(0),
							_spkrAdaptCnt(0),
							_micAdaptCnt(0),
							_spkrAdaptSize(SPKR_ADAPT_SIZE),
							_micAdaptSize(MIC_ADAPT_SIZE),
							_correlationSize(0),
							_processedByte(0),
							_echoActive(true),
							_noiseActive(true)
{
  _debug("EchoCancel: Instantiate echo canceller");

  /*
  micFile = new ofstream("micData", ofstream::binary);
  echoFile = new ofstream("echoData", ofstream::binary);
  spkrFile = new ofstream("spkrData", ofstream::binary);
  
  micLevelData = new ofstream("micLevelData", ofstream::binary);
  spkrLevelData = new ofstream("spkrLevelData", ofstream::binary);
  */

  _micData = new RingBuffer(50000);
  _spkrData = new RingBuffer(50000);
  _spkrDataOut = new RingBuffer(50000);

  _micData->createReadPointer();
  _spkrData->createReadPointer();
  _spkrDataOut->createReadPointer();

  // variable used to sync mic and spkr
  _spkrStoped = true;

  _smplPerFrame = (_samplingRate * _frameLength) / MS_PER_SEC;
  _smplPerSeg = (_samplingRate * SEGMENT_LENGTH) / MS_PER_SEC;
  _micHistoryLength = MIC_LENGTH / SEGMENT_LENGTH;
  _spkrHistoryLength = SPKR_LENGTH / SEGMENT_LENGTH;
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
  float f=0.0;
  speex_preprocess_ctl(_noiseState, SPEEX_PREPROCESS_SET_DEREVERB_DECAY, &f);
  f=0.0;
  speex_preprocess_ctl(_noiseState, SPEEX_PREPROCESS_SET_DEREVERB_LEVEL, &f);

  memset(_avgSpkrLevelHist, 0, BUFF_SIZE*sizeof(int));
  memset(_avgMicLevelHist, 0, BUFF_SIZE*sizeof(int));

  memset(_delayLineAmplify, 0, MAX_DELAY_LINE_AMPL*sizeof(float));

  _amplDelayIndexIn = 0;
  _amplDelayIndexOut = 0;

  _correlationSize = _spkrAdaptSize;
  
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

  micLevelData->close();
  spkrLevelData->close();
  delete micLevelData;
  delete spkrLevelData;
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
  _micHistoryLength = MIC_LENGTH / SEGMENT_LENGTH;
  _spkrHistoryLength = SPKR_LENGTH / SEGMENT_LENGTH;
  _nbSegmentPerFrame =  _frameLength / SEGMENT_LENGTH;

  memset(_delayLineAmplify, 0, MAX_DELAY_LINE_AMPL*sizeof(float));

  _amplDelayIndexIn = 0;
  _amplDelayIndexOut = ECHO_LENGTH / SEGMENT_LENGTH;

  _adaptDone = false;
  _adaptStarted = false;
  _adaptCnt = 0;
  _spkrAdaptCnt = 0;
  _micAdaptCnt = 0;

  _micData->flushAll();
  _spkrData->flushAll();
  _spkrDataOut->flushAll();

  speex_preprocess_state_destroy(_noiseState);

  _noiseState = speex_preprocess_state_init(_smplPerFrame, _samplingRate);
  int i=1;
  speex_preprocess_ctl(_noiseState, SPEEX_PREPROCESS_SET_DENOISE, &i);
  i=-30;
  speex_preprocess_ctl(_noiseState, SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &i);
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

  _spkrStoped = true;

  _processedByte = 0;
}

void EchoCancel::putData(SFLDataFormat *inputData, int nbBytes) 
{

  if(_spkrStoped) {
      _debug("EchoCancel: Flush data");
      _micData->flushAll();
      _spkrData->flushAll();
      _spkrStoped = false;
  }

  // Put data in speaker ring buffer
  _spkrData->Put(inputData, nbBytes);
}

int EchoCancel::getData(SFLDataFormat *outputData)
{

  int copied = 0;

  if(_processedByte > 0) {
       copied = _spkrDataOut->Get(outputData, _processedByte);
       _processedByte = 0;
  }

   return copied;
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

  // _debug("EchoCancel: speaker avail %d, mic avail %d, processed: %d", spkrAvail/320, micAvail/320, _processedByte/320);

  // Init number of frame processed
  int nbFrame = 0;

  // Get data from mic and speaker internal buffer
  while((spkrAvail >= byteSize) && (micAvail >= byteSize)) {

    // get synchronized data
    _spkrData->Get(_tmpSpkr, byteSize);
    _micData->Get(_tmpMic, byteSize);
    
    // micFile->write((const char *)_tmpMic, byteSize);
    // spkrFile->write((const char *)_tmpSpkr, byteSize);
    
    // Remove noise
    if(_noiseActive)
        speex_preprocess_run(_noiseState, _tmpMic);

    // Processed echo cancellation
    performEchoCancel(_tmpMic, _tmpSpkr, _tmpOut);

    // echoFile->write((const char *)_tmpOut, byteSize);

    bcopy(_tmpOut, outputData+(nbFrame*_smplPerFrame), byteSize);

    // used to sync with speaker 
    _processedByte += byteSize;

    spkrAvail = _spkrData->AvailForGet();
    micAvail = _micData->AvailForGet();

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

    reset();
  }
}


void EchoCancel::performEchoCancel(SFLDataFormat *micData, SFLDataFormat *spkrData, SFLDataFormat *outputData) {

  // int tempmiclevel[_nbSegmentPerFrame];
  // int tempspkrlevel[_nbSegmentPerFrame];

  for(int k = 0; k < _nbSegmentPerFrame; k++) {

    updateEchoCancel(micData+(k*_smplPerSeg), spkrData+(k*_smplPerSeg));

    _spkrLevel = getMaxAmplitude(_avgSpkrLevelHist, _spkrHistoryLength);
    _micLevel = getMaxAmplitude(_avgMicLevelHist, _micHistoryLength);

    // _debug("_spkrLevel: (max): %d", _spkrLevel);
    // _debug("_micLevel: (min): %d", _micLevel);

    // tempspkrlevel[k] = _spkrLevel;
    // tempmiclevel[k] = _micLevel;

    if(_spkrLevel >= MIN_SIG_LEVEL) {
        if(_micLevel > _spkrLevel) {
	  increaseFactor(0.2);
	  // _amplFactor = 0.0;
	}
	else {
	    _amplFactor = 0.0;
	}
    }
    else {
      increaseFactor(0.01);
    }

    // lowpass filtering
    float amplify = (_lastAmplFactor + _amplFactor) / 2;
    _lastAmplFactor = _amplFactor;

    if(!_echoActive)
        amplify = 1.0;

    amplifySignal(micData+(k*_smplPerSeg), outputData+(k*_smplPerSeg), amplify);
    
  }

  // micLevelData->write((const char *)tempmiclevel, sizeof(int)*_nbSegmentPerFrame);
  // spkrLevelData->write((const char *)tempspkrlevel, sizeof(int)*_nbSegmentPerFrame);
  
}


void EchoCancel::updateEchoCancel(SFLDataFormat *micData, SFLDataFormat *spkrData) {

  // TODO: we should find a way to normalize signal at this point
  int micLvl = computeAmplitudeLevel(micData, _smplPerSeg) / 6;
  int spkrLvl = computeAmplitudeLevel(spkrData, _smplPerSeg);

  // Add 1 to make sure we are not dividing by 0
  _avgMicLevelHist[_micHistCnt++] = micLvl+1;
  _avgSpkrLevelHist[_spkrHistCnt++] = spkrLvl+1;

  // _debug("micLevel: %d", micLvl);
  // _debug("spkrLevel: %d", spkrLvl);

  if(_micHistCnt >= _micHistoryLength)
    _micHistCnt = 0;

  if(_spkrHistCnt >= _spkrHistoryLength)
    _spkrHistCnt = 0;
    
  /*
  // if adaptation done, stop here
  // if(_adaptDone)
  if(true)
    return;

  // start learning only if there is data played through speaker
  if(!_adaptStarted) {
    if(spkrLvl > MIN_SIG_LEVEL)
      _adaptStarted = true;
    else
      return;
  }

  if(_spkrAdaptCnt < _spkrAdaptSize)
      _spkrAdaptArray[_spkrAdaptCnt++] = spkrLvl;

  if(_micAdaptCnt < _micAdaptSize)
      _micAdaptArray[_micAdaptCnt++] = micLvl;

  // perform correlation if spkr size is reached
  if(_adaptCnt > _spkrAdaptSize) {
      int k = _adaptCnt - _spkrAdaptSize;
      _correlationArray[k] = performCorrelation(_spkrAdaptArray, _micAdaptArray+k, _correlationSize); 
      // _debug("EchoCancel: Correlation: %d", _correlationArray[k]);
  }

  _adaptCnt++;

  // if we captured a full echo
  if(_adaptCnt == _micAdaptSize) {
    _debug("EchoCancel: Echo path adaptation completed");
    _adaptDone = true;
    _amplDelayIndexOut = 0;// getMaximumIndex(_correlationArray, _correlationSize);
    _debug("EchoCancel: Echo length %d", _amplDelayIndexOut);
  }
  */
}


int EchoCancel::computeAmplitudeLevel(SFLDataFormat *data, int size) {
  
  int level = 0;

  for(int i = 0; i < size; i++) {
    if(data[i] >= 0.0)
      level += (int)data[i];
    else
      level -= (int)data[i];
  }

  level = level / _smplPerSeg;

  return level;
}


int EchoCancel::getMaxAmplitude(int *data, int size) {

  SFLDataFormat level = 0.0;

  for(int i = 0; i < size; i++) {
    if(data[i] >= level)
      level = data[i];
  }

  return (int)level;
}


void EchoCancel::amplifySignal(SFLDataFormat *micData, SFLDataFormat *outputData, float amplify) {

  // for(int i = 0; i < _smplPerSeg; i++)
  //  outputData[i] = micData[i];

  // Use delayed amplification factor due to sound card latency 
  // do not increment amplitude array if adaptation is not done 
  // if (_adaptDone) {
  if (true) {
    for(int i = 0; i < _smplPerSeg; i++) {
        outputData[i] = (SFLDataFormat)(((float)micData[i])*_delayLineAmplify[_amplDelayIndexOut]);
    }
    _amplDelayIndexOut++;
    _delayLineAmplify[_amplDelayIndexIn++] = amplify;
  }
  else {
    for(int i = 0; i < _smplPerSeg; i++) {
        outputData[i] = micData[i];
    }
    return;
  }

  if(_amplDelayIndexOut >= MAX_DELAY_LINE_AMPL)
    _amplDelayIndexOut = 0;

  if(_amplDelayIndexIn >= MAX_DELAY_LINE_AMPL)
    _amplDelayIndexIn = 0;

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


int EchoCancel::performCorrelation(int *data1, int *data2, int size) {

  int correlation = 0;
  while(size) {
    size--;
    correlation += data1[size] * data2[size];
  }
  return correlation;
}


int EchoCancel::getMaximumIndex(int *data, int size) {

  int index = size;
  int max = data[size-1];
 
  while(size) {
    size--;
    if(data[size] > max) {
        index = size;
	max = data[size];
    }
  }

  return index;
}
