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


// decimation filter coefficient
float decimationCoefs[] = {0.1, 0.1, 0.1, 0.1, 0.1};
std::vector<double> ird(decimationCoefs, decimationCoefs + sizeof(decimationCoefs)/sizeof(float));


// decimation filter coefficient
float bandpassCoefs[] = {0.1, 0.1, 0.1, 0.1, 0.1};
std::vector<double> irb(bandpassCoefs, bandpassCoefs + sizeof(bandpassCoefs)/sizeof(float));


FirFilter::FirFilter(std::vector<double> ir) : _impulseResponse(ir),
					       _length(ir.size()),
					       _count(0)
{}

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


DelayDetection::DelayDetection() : _decimationFilter(ird), _bandpassFilter(irb) {}

DelayDetection::~DelayDetection(){}

void DelayDetection::reset() {}

void DelayDetection::putData(SFLDataFormat *inputData, int nbBytes) 
{
  int nbSamples = nbBytes/sizeof(SFLDataFormat);
  
  float tmp[nbSamples];
  float down[nbSamples];

  convertInt16ToFloat32(inputData, tmp, nbSamples);
  downsampleData(tmp, down, nbSamples, 8);
  bandpassFilter(down, nbSamples/8);
}

int DelayDetection::getData(SFLDataFormat *outputData) { return 0; }

void DelayDetection::process(SFLDataFormat *inputData, int nbBytes) {

  int nbSamples = nbBytes/sizeof(SFLDataFormat);

  float tmp[nbSamples];
  float down[nbSamples];

  convertInt16ToFloat32(inputData, tmp, nbSamples);
  downsampleData(tmp, down, nbSamples, 8);
  bandpassFilter(down, nbSamples/8);
}

int DelayDetection::process(SFLDataFormat *intputData, SFLDataFormat *outputData, int nbBytes) { return 0; }

void DelayDetection::process(SFLDataFormat *micData, SFLDataFormat *spkrData, SFLDataFormat *outputData, int nbBytes) {}

void DelayDetection::crossCorrelate(double *ref, double *seg, double *res, short refSize, short segSize) {

  int counter = 0;

  // Output has same size as the
  short rsize = refSize;
  short ssize = segSize;
  short tmpsize = segSize-refSize+1;

  // perform autocorrelation on reference signal
  double acref  = correlate(ref, ref, rsize);

  // perform crossrelation on signal
  double acseg = 0.0;
  double r;
  while(--tmpsize) {
    --ssize;
    acseg = correlate(seg+tmpsize, seg+tmpsize, rsize);
    res[ssize] = correlate(ref, seg+tmpsize, rsize);
    r = sqrt(acref*acseg);

    if(r < 0.0001)
      res[ssize] = 0.0;
    else
      res[ssize] = res[ssize] / r;
  }

  // perform crosscorrelation on zerro padded region
  int i = 0;
  while(rsize) {
    acseg = correlate(seg, seg, rsize);
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

double DelayDetection::correlate(double *sig1, double *sig2, short size) {

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
  
    float tmp[nbSamples];

    for(int i = 0; i < nbSamples; i++) {
        tmp[i] = _decimationFilter.getOutputSample(input[i]);
    }

    int j;
    for(j=_remainingIndex; j<nbSamples; j+=factor) {
        output[j] = tmp[j];
    }
    _remainingIndex = j - nbSamples;
    
}


void DelayDetection::bandpassFilter(float *input, int nbSamples) {

    for(int i = 0; i < nbSamples; i++) {
        input[i] = _bandpassFilter.getOutputSample(input[i]);
    }
}
