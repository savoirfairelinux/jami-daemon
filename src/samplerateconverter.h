/*
 *  Copyright (C) 2008 Savoir-Faire Linux inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _SAMPLE_RATE_H
#define _SAMPLE_RATE_H

#include <samplerate.h>
#include <math.h>

#include "global.h"

class SamplerateConverter {
  public:
    /** Constructor */
    SamplerateConverter( void );
    /** Destructor */
    ~SamplerateConverter( void );

    /** 
     * Upsample from the samplerate1 to the samplerate2
     * @param data  The data buffer
     * @param SamplerateConverter1 The lower sample rate
     * @param SamplerateConverter2 The higher sample rate
     * @param nbSamples	  The number of samples to process
     * @return int The number of samples after the operation
     */
    int upsampleData( SFLDataFormat* dataIn , SFLDataFormat* dataOut , int samplerate1 , int samplerate2 , int nbSamples );

    /**
     * Downsample from the samplerate1 to the samplerate2
     * @param data  The data buffer
     * @param SamplerateConverter1 The lower sample rate
     * @param SamplerateConverter2 The higher sample rate
     * @param nbSamples	  The number of samples to process
     * @return int The number of samples after the operation
     */
    int downsampleData( SFLDataFormat* dataIn , SFLDataFormat* dataOut , int samplerate1 , int samplerate2 , int nbSamples );

  private:
    /** Downsampled float buffer */
    float32* _floatBufferDown;

    /** Upsampled float buffer */
    float32* _floatBufferUp;

    /** libSamplerateConverter converter for incoming voice */
    SRC_STATE*    _src_state_spkr;

    /** libSamplerateConverter converter for outgoing voice */
    SRC_STATE*    _src_state_mic;

    /** libSamplerateConverter error */
    int _src_err;
};

#endif //_SAMPLE_RATE_H

