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

#ifndef ALGORITHM_H
#define ALGORITHM_H

#include "global.h"

/**
 * \class Algorithm
 * 
 * Abstract interface used to implement audio processing algorithm
 */
class Algorithm {

 public:

  virtual void reset(void) = 0;

  /**
   * Put data to be processed
   */
  virtual void putData(SFLDataFormat *inputData, int nbBytes) = 0;

  /**
   * Class implementing this interface must define this function 
   * for audio processing that require synchronization between spkrdata and
   */
  virtual void process(SFLDataFormat *inputData, int nbBytes) = 0;

  /**
   * Class implementing this interface must define this function 
   * for audio processing that require synchronization between spkrdata and
   */
  virtual int process(SFLDataFormat *inputData, SFLDataFormat *outputData, int nbBytes) = 0;

  /**
   * Class implementing this interface must define this function 
   * for audio processing that require synchronization between spkr and mic 
   * \param micData
   * \param spkrData
   * \param outputData
   */
  virtual void process(SFLDataFormat *micData, SFLDataFormat *spkrData, SFLDataFormat *outputData, int nbBytes) = 0;

};

#endif
