/*
 *  Copyright (C) 2004-2006 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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

#ifndef __TONE_GENERATOR_H__
#define __TONE_GENERATOR_H__

#include <string>
#include <cc++/thread.h>

#include "../global.h"

/**
 * @file tonegenerator.h
 * @brief Sine generator to create tone with string definition
 */

class ToneGenerator {
  public:
    /**
     * Constructor
     * @param sampleRate  The sample rate of the generated samples
     */
    ToneGenerator (unsigned int sampleRate);
    
    /**
     * Destructor
     */
    ~ToneGenerator (void);

    /**
     * Calculate sinus with superposition of 2 frequencies
     * @param lowerfreq	Lower frequency
     * @param higherfreq  Higher frequency
     * @param ptr For result buffer
     * @param len The length of the data to be generated
     */
    void generateSin	(int, int, int16 *, int len) const;


    ///////////////////////////
    // Public members variable
    //////////////////////////
    int16 *sample;
    int freq1, freq2;
    int time;
    int totalbytes;

  private:
    /*
     * Initialisation of the supported tones according to the countries.
     */
    void		initTone (void);

    int16 _buf[SIZEBUF];
    int _sampleRate;
};

#endif // __TONE_GENRATOR_H__
