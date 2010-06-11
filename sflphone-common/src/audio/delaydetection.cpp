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

DelayDetection::DelayDetection(){}

DelayDetection::~DelayDetection(){}

void DelayDetection::reset() {}

void DelayDetection::putData(SFLDataFormat *inputData, int nbBytes) {}

int DelayDetection::getData(SFLDataFormat *outputData) { return 0; }

void DelayDetection::process(SFLDataFormat *inputData, int nbBytes) {}

int DelayDetection::process(SFLDataFormat *intputData, SFLDataFormat *outputData, int nbBytes) { return 0; }

void DelayDetection::process(SFLDataFormat *micData, SFLDataFormat *spkrData, SFLDataFormat *outputData, int nbBytes) {}

void DelayDetection::crossCorrelate(double *ref, double *seg, double *res, short refSize, short segSize)
{

  _debug("CrossCorrelate");

  // Output has same size as the 
  short rsize = refSize;
  short ssize = segSize;
  short tmpsize = segSize-refSize+1;

  // perform autocorrelation on reference signal
  double acref  = correlate(ref, ref, rsize);

  double acseg = 0.0;
  double r;
  while(--tmpsize) {
      acseg = correlate(seg+tmpsize, seg+tmpsize, rsize);
      res[ssize] = correlate(ref, seg+tmpsize, rsize);
      r = sqrt(acref*acseg);
      if(r < 0.0001)
	res[ssize] = 0.0;
      else
	res[ssize] = res[ssize] / r;
      --ssize;
  }


  _debug("----------------start------------------");

  for (int j = 0; j < 10; j++)
      _debug("segment: %f", seg[j]);

  seg[0] = 0.0;
  // zerro padded region
  int i = 0;
  while(rsize) {
      _debug("rsize: %i, ssize: %i", rsize, ssize);
      acref = correlate(ref, ref, refSize);
      _debug("acref: %f", acref);
      acseg = correlate(seg, seg, rsize);
      _debug("acseg: %f", acseg);
      res[ssize] = correlate(ref+rsize, seg, rsize);
      _debug("res[ssize]: %f", res[ssize]);
      r = sqrt(acref*acseg);
      _debug("r: %f", r);
      if(r < 0.0001)
          res[ssize] = 0.0;
      else
          res[ssize] = res[ssize] / r;
      --ssize;
      --rsize;
      ++i;
  }
  
}

double DelayDetection::correlate(double *sig1, double *sig2, short size) {

  _debug("Correlate");

  short s = size;

  double ac = 0.0;

  for(int i = size-1; i >= 0; i--) {
    _debug("     %i:  s1, %f, s2 %f", i, sig1[i], sig2[i]);
  }
  while(s--)
      ac += sig1[s]*sig2[s];

  return ac;
}
