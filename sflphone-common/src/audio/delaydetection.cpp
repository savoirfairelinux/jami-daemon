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



#include "delaydetection.h"
#include "math.h"
// #include <stdio.h>
#include <string.h>
#include <samplerate.h>

// decimation filter coefficient
float decimationCoefs[] = {-0.09870257, 0.07473655, 0.05616626, 0.04448337, 0.03630817, 0.02944626,
			   0.02244098, 0.01463477, 0.00610982, -0.00266367, -0.01120109, -0.01873722,
			   -0.02373243, -0.02602213, -0.02437806, -0.01869834, -0.00875287, 0.00500204,
			   0.02183252, 0.04065763, 0.06015944, 0.0788299, 0.09518543, 0.10799179,
			   0.1160644,  0.12889288, 0.1160644, 0.10799179, 0.09518543, 0.0788299,
			   0.06015944, 0.04065763, 0.02183252, 0.00500204, -0.00875287, -0.01869834,
			   -0.02437806, -0.02602213, -0.02373243, -0.01873722, -0.01120109, -0.00266367,
			   0.00610982, 0.01463477, 0.02244098, 0.02944626, 0.03630817, 0.04448337,
			   0.05616626,  0.07473655, -0.09870257};
std::vector<double> ird(decimationCoefs, decimationCoefs + sizeof(decimationCoefs)/sizeof(float));


// decimation filter coefficient
float bandpassCoefs[] = {0.06278034, -0.0758545, -0.02274943, -0.0084497, 0.0702427, 0.05986113,
			 0.06436469, -0.02412049, -0.03433526, -0.07568665, -0.03214543, -0.07236507,
			 -0.06979052, -0.12446371, -0.05530828, 0.00947243, 0.15294699, 0.17735563,
			 0.15294699, 0.00947243, -0.05530828, -0.12446371, -0.06979052, -0.07236507,
			 -0.03214543, -0.07568665, -0.03433526, -0.02412049,  0.06436469, 0.05986113,
			 0.0702427, -0.0084497, -0.02274943, -0.0758545, 0.06278034};
std::vector<double> irb(bandpassCoefs, bandpassCoefs + sizeof(bandpassCoefs)/sizeof(float));


FirFilter::FirFilter(std::vector<double> ir) : _length(ir.size()),
                                               _impulseResponse(ir),
					       _count(0)
{
  memset(_taps, 0, sizeof(double)*MAXFILTERSIZE);
}

FirFilter::~FirFilter() {}

float FirFilter::getOutputSample(float inputSample) 
{
  _taps[_count] = inputSample;
  double result = 0.0;
  int index = _count;
  for(int i = 0; i < _length; i++) {
    result = result + _impulseResponse[i] * _taps[index--];
    if(index < 0)
      index = _length-1;
  }
  _count++;
  if(_count >= _length)
    _count = 0;

  return result;
}

void FirFilter::reset(void)
{
  for(int i = 0; i < _length; i++) {
    _impulseResponse[i] = 0.0;
  }
}


DelayDetection::DelayDetection() : _internalState(WaitForSpeaker), _decimationFilter(ird), _bandpassFilter(irb), _segmentSize(DELAY_BUFF_SIZE), _downsamplingFactor(8) 
{
    _micDownSize = WINDOW_SIZE / _downsamplingFactor;
    _spkrDownSize = DELAY_BUFF_SIZE / _downsamplingFactor;

    memset(_spkrReference, 0, sizeof(float)*WINDOW_SIZE*2);
    memset(_capturedData, 0, sizeof(float)*DELAY_BUFF_SIZE*2);
    memset(_spkrReferenceDown, 0, sizeof(float)*WINDOW_SIZE*2);
    memset(_captureDataDown, 0, sizeof(float)*DELAY_BUFF_SIZE*2);
    memset(_spkrReferenceFilter, 0, sizeof(float)*WINDOW_SIZE*2);
    memset(_captureDataFilter, 0, sizeof(float)*DELAY_BUFF_SIZE*2);
    memset(_correlationResult, 0, sizeof(float)*DELAY_BUFF_SIZE*2);

}

DelayDetection::~DelayDetection(){}

void DelayDetection::reset() 
{
    _nbMicSampleStored = 0;
    _nbSpkrSampleStored = 0;

    _decimationFilter.reset();
    _bandpassFilter.reset();

    memset(_spkrReference, 0, sizeof(float)*WINDOW_SIZE*2);
    memset(_capturedData, 0, sizeof(float)*DELAY_BUFF_SIZE*2);
    memset(_spkrReferenceDown, 0, sizeof(float)*WINDOW_SIZE*2);
    memset(_captureDataDown, 0, sizeof(float)*DELAY_BUFF_SIZE*2);
    memset(_spkrReferenceFilter, 0, sizeof(float)*WINDOW_SIZE*2);
    memset(_captureDataFilter, 0, sizeof(float)*DELAY_BUFF_SIZE*2);
    memset(_correlationResult, 0, sizeof(float)*DELAY_BUFF_SIZE*2);
    
    _internalState = WaitForSpeaker;
}

void DelayDetection::putData(SFLDataFormat *inputData, int nbBytes) 
{

  // Machine may already got a spkr and is waiting for mic or computing correlation 
  if(_nbSpkrSampleStored == WINDOW_SIZE)
      return;

  int nbSamples = nbBytes/sizeof(SFLDataFormat);

  if((_nbSpkrSampleStored + nbSamples) > WINDOW_SIZE)
    nbSamples = WINDOW_SIZE - _nbSpkrSampleStored;
  

  if (nbSamples) {

    float tmp[nbSamples];
    float down[nbSamples];

    convertInt16ToFloat32(inputData, tmp, nbSamples);
    memcpy(_spkrReference+_nbSpkrSampleStored, tmp, nbSamples*sizeof(float));

    downsampleData(tmp, down, nbSamples, _downsamplingFactor);
    bandpassFilter(down, nbSamples/_downsamplingFactor);
    memcpy(_spkrReferenceDown+(_nbSpkrSampleStored/_downsamplingFactor), down, (nbSamples/_downsamplingFactor)*sizeof(float));
  
    _nbSpkrSampleStored += nbSamples;

  }

  // Update the state
  _internalState = WaitForMic;

}

int DelayDetection::getData(SFLDataFormat *outputData) { return 0; }

void DelayDetection::process(SFLDataFormat *inputData, int nbBytes) {

  if(_internalState != WaitForMic)
      return;

  int nbSamples = nbBytes/sizeof(SFLDataFormat);

  if((_nbMicSampleStored + nbSamples) > DELAY_BUFF_SIZE)
    nbSamples = DELAY_BUFF_SIZE - _nbMicSampleStored;

  if(nbSamples) {
    float tmp[nbSamples];
    float down[nbSamples];

    convertInt16ToFloat32(inputData, tmp, nbSamples);
    memcpy(_capturedData+_nbMicSampleStored, tmp, nbSamples);

    downsampleData(tmp, down, nbSamples, _downsamplingFactor);

    /*
    for(int i = 0; i < 10; i++)
      _debug("up: %.10f", tmp[i]);

    for(int i = 0; i < 10; i++)
      _debug("down: %.10f", down[i]);

    bandpassFilter(down, nbSamples/_downsamplingFactor);

    for(int i = 0; i < 10; i++)
      _debug("band: %.10f", down[i]);
    */

    memcpy(_captureDataDown+(_nbMicSampleStored/_downsamplingFactor), down, (nbSamples/_downsamplingFactor)*sizeof(float));

    _nbMicSampleStored += nbSamples;

  }

  if(_nbMicSampleStored == DELAY_BUFF_SIZE)
      _internalState = ComputeCorrelation;
  else
    return;

  /*
  for(int i = 0; i < 10; i++)
    _debug("spkrRef: %.10f", _spkrReferenceDown[i]);

  for(int i = 0; i < 10; i++)
    _debug("micSeg: %.10f", _captureDataDown[i]);
  */

  _debug("_spkrDownSize: %d, _micDownSize: %d", _spkrDownSize, _micDownSize);
  crossCorrelate(_spkrReferenceDown, _captureDataDown, _correlationResult, _micDownSize, _spkrDownSize);

  int maxIndex = getMaxIndex(_correlationResult, _spkrDownSize);

  _debug("MaxIndex: %d", maxIndex);

  // reset();
}

int DelayDetection::process(SFLDataFormat *intputData, SFLDataFormat *outputData, int nbBytes) { return 0; }

void DelayDetection::process(SFLDataFormat *micData, SFLDataFormat *spkrData, SFLDataFormat *outputData, int nbBytes) {}

void DelayDetection::crossCorrelate(float *ref, float *seg, float *res, int refSize, int segSize) {

  _debug("CrossCorrelate");

  int counter = 0;

  // Output has same size as the
  int rsize = refSize;
  int ssize = segSize;
  int tmpsize = segSize-refSize+1;

  /*
  for(int i = 0; i < 32; i++)
      _debug("ref: %.10f", ref[i]);

  for(int i = 0; i < 150; i++)
      _debug("seg: %.10f", seg[i]);
  */

  // perform autocorrelation on reference signal
  float acref = correlate(ref, ref, rsize);
  // _debug("acref: %f", acref);
  
  // perform crossrelation on signal
  float acseg = 0.0;
  float r;
  while(--tmpsize) {
    --ssize;
    acseg = correlate(seg+tmpsize, seg+tmpsize, rsize);
    // _debug("acseg: %f", acseg);
    res[ssize] = correlate(ref, seg+tmpsize, rsize);
    r = sqrt(acref*acseg);

    if(r < 0.0000001)
      res[ssize] = 0.0;
    else
      res[ssize] = res[ssize] / r;
  }

  // perform crosscorrelation on zerro padded region
  int i = 0;
  while(rsize) {
    acseg = correlate(seg, seg, rsize);
    // _debug("acseg: %f", acseg);
    res[ssize-1] = correlate(ref+i, seg, rsize);
    r = sqrt(acref*acseg);

    if(r < 0.0001)
      res[ssize-1] = 0.0;
    else
      res[ssize-1] = res[ssize-1] / r;
                                                                                                                                                                                                                                            
    --rsize;
    --ssize;
    ++i;
  }
}

double DelayDetection::correlate(float *sig1, float *sig2, short size) {

  short s = size;

  double ac = 0.0;
  while(s--)
      ac += sig1[s]*sig2[s];

  return ac;
}


void DelayDetection::convertInt16ToFloat32(SFLDataFormat *input, float *output, int nbSamples) {

    // factor is 1/(2^15), used to rescale the short int range to the
    // [-1.0 - 1.0] float range.
#define S2F_FACTOR .000030517578125f;
  int len = nbSamples;

  while(len) {
    len--;
    output[len] = (float)input[len] * S2F_FACTOR;
  }
}


void DelayDetection::downsampleData(float *input, float *output, int nbSamples, int factor) {

  /*
  float tmp[nbSamples];
  
  for(int i = 0; i < nbSamples; i++) {
      tmp[i] = _decimationFilter.getOutputSample(input[i]);
  }

  int j;
  for(j=_remainingIndex; j<nbSamples; j+=factor) {
      output[j] = tmp[j];
  }
  _remainingIndex = j - nbSamples;
  */
 
  /*
  double downsampleFactor = (double) samplerate1 / samplerate2;
  
  int nbSamplesMax = (int) (samplerate1 * getFramesize() / 1000);
  */

  int _src_err;

  SRC_STATE *_src_state  = src_new (SRC_LINEAR, 1, &_src_err);

  double downfactor = 1.0 / (double)factor;

  if (downfactor != 1.0) {
    SRC_DATA src_data;
    src_data.data_in = input;
    src_data.data_out = output;
    src_data.input_frames = nbSamples;
    src_data.output_frames = nbSamples / factor;
    src_data.src_ratio = downfactor;
    src_data.end_of_input = 0; // More data will come
    
    //src_short_to_float_array (dataIn, _floatBufferUpMic, nbSamples);
    //_debug("downsample %d %f %d" ,  src_data.output_frames, src_data.src_ratio , nbSamples);
    src_process (_src_state, &src_data);
    //_debug("downsample %d %f %d" ,  src_data.output_frames, src_data.src_ratio , nbSamples);
    // nbSamples  = (src_data.output_frames_gen > nbSamplesMax) ? nbSamplesMax : src_data.output_frames_gen;
    //_debug("downsample %d %f %d" ,  src_data.output_frames, src_data.src_ratio , nbSamples);
    // src_float_to_short_array (_floatBufferDownMic , dataOut , nbSamples);
  }
}


void DelayDetection::bandpassFilter(float *input, int nbSamples) {

    for(int i = 0; i < nbSamples; i++) {
        input[i] = _bandpassFilter.getOutputSample(input[i]);
    }
}


int DelayDetection::getMaxIndex(float *data, int size) {

  float max = 0.0;
  int k;

  for(int i = 0; i < size; i++) {
    if(data[i] >= max) {
      max = data[i];
      k = i;
    }
  }

  return k;
}
