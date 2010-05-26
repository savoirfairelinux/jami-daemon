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

#ifndef DCBLOCKER_H
#define DCBLOCKER_H

#include "algorithm.h"
#include "global.h"

#include <vector>

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

class DcBlocker : public Algorithm {

public:

    DcBlocker();

    ~DcBlocker();

    virtual void reset(void);

    /**
     * Unused
     */
    virtual void putData(SFLDataFormat *inputData, int nbBytes);

    /**
     * Unused
     */
    virtual int getData(SFLDataFormat *outputData);

    /**
     * Perform dc blocking given the input data
     */
    virtual void process(SFLDataFormat *data, int nbBytes);

    /**
     * Perform echo cancellation using internal buffers
     * \param inputData containing mixed echo and voice data
     * \param outputData containing 
     */
    virtual int process(SFLDataFormat *inputData, SFLDataFormat *outputData, int nbBytes);

    /**
     * Perform echo cancellation, application must provide its own buffer
     * \param micData containing mixed echo and voice data
     * \param spkrData containing far-end voice data to be sent to speakers
     * \param outputData containing the processed data
     */
    virtual void process(SFLDataFormat *micData, SFLDataFormat *spkrData, SFLDataFormat *outputData, int nbBytes);

private:

    SFLDataFormat _y, _x, _xm1, _ym1;
};

#endif
