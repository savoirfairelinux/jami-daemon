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


#ifndef DELAYDETECTION_H
#define DELAYDETECTION_H

#include "algorithm.h"

// Template size in samples for correlation
#define WINDOW_SIZE 256

// Segment length in ms for correlation
#define MAX_DELAY 150

#define MAXFILTERSIZE 100

class FirFilter {

 public:

  /**
   * Constructor for this class
   */
  FirFilter(std::vector<double> ir);

  /**
   * SDestructor for this class
   */
  ~FirFilter();
  

 private:

  /**
   * Length of the filter
   */
  int _length;

  /**
   * Coefficient of the filter
   */
  std::vector<double> _impulseResponse;

  /**
   * Circular buffer
   */
  double _delayLine[MAXFILTERSIZE];

  /**
   * Counter
   */
  int _count;

  /**
   * Perform filtering on one sample
   */
  int getOutputSample(int inputSample);
  
};


class DelayDetection : public Algorithm {

 public:

    DelayDetection();

    ~DelayDetection();

    virtual void reset(void);

    virtual void putData(SFLDataFormat *inputData, int nbBytes);

    virtual int getData(SFLDataFormat *getData);

    virtual void process(SFLDataFormat *inputData, int nbBytes);

    virtual int process(SFLDataFormat *inputData, SFLDataFormat *outputData, int nbBytes);

    virtual void process(SFLDataFormat *micData, SFLDataFormat *spkrData, SFLDataFormat *outputData, int nbBytes);

 private:

    /**
     * Perform a normalized crosscorrelation between template and segment
     */
    void crossCorrelate(double *ref, double *seg, double *res, short refSize, short segSize);

    /**
     * Perform a correlation on specified signals (mac)
     */
    double correlate(double *sig1, double *sig2, short size);

    /**
     * Segment size in samples for correlation
     */
    short _segmentSize;

    /**
     * Resulting correlation size (s + w -1)
     */
    short _correlationSize;


 public:

    friend class DelayDetectionTest;
};

#endif
