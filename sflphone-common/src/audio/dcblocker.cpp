/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
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

#include "dcblocker.h"

FirFilter::FirFilter(std::vector<double> ir) : _impulseResponse(ir), 
					       _length(ir.size()),
					       _count(0)
{}

FirFilter::~FirFilter() {}

int FirFilter::getOutputSample(int inputSample) 
{
  _delayLine[_count] = (double)inputSample;
  double result = 0.0;
  int index = _count;
  for(int i = 0; i < _length; i++) {
    result = result + _impulseResponse[i] * _delayLine[index--];
    if(index < 0)
      index = _length-1;
  }
  _count++;
  if(_count >= _length)
    _count = 0;

  return (int)result;
}


DcBlocker::DcBlocker() : _y(0), _x(0), _xm1(0), _ym1(0) {}

DcBlocker::~DcBlocker() {}

void DcBlocker::reset()
{
  _y = 0;
  _x = 0;
  _xm1 = 0;
  _ym1 = 0;
}

void DcBlocker::putData(SFLDataFormat *inputData, int nbBytes) {}

void DcBlocker::process (SFLDataFormat *data, int nbBytes)
{
    // y(n) = x(n) - x(n-1) + R y(n-1) , R = 0.9999

    int nbSamples = nbBytes / sizeof(SFLDataFormat); 
    for (int i = 0; i < nbSamples; i++) {

        _x = data[i];

        _y = (SFLDataFormat) ( (float) _x - (float) _xm1 + 0.9999 * (float) _ym1);
        _xm1 = _x;
        _ym1 = _y;

        data[i] = _y;

    }
}

int DcBlocker::process(SFLDataFormat *inputData, SFLDataFormat *outputData, int nbBytes) { return 0;}

void DcBlocker::process(SFLDataFormat *micData, SFLDataFormat *spkrData, SFLDataFormat *outputData, int nbBytes) {}
