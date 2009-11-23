/*
 *  Copyright (C) 2008 Savoir-Faire Linux inc.
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

#include "audiodsp.h"

AudioDSP::AudioDSP()
{

    bufPointer_ = 0;
    bufferLength_ = 1024;
    circBuffer_ = new float[bufferLength_];

}


AudioDSP::~AudioDSP()
{

    delete[] circBuffer_;

}


float AudioDSP::getRMS (int data)
{
    // printf("AudioDSP::getRMS() : bufPointer_ %i  ", bufPointer_);
    printf ("AudioDSP::getRMS() : %i ", data);
    circBuffer_[bufPointer_++] = (float) data;

    if (bufPointer_ >= bufferLength_)
        bufPointer_ = 0;

    return computeRMS();
}


float AudioDSP::computeRMS()
{

    rms = 0.0;


    for (int i = 0; i < bufferLength_; i++) {
        // printf("AudioDSP::computeRMS() : i_ %i  ", i);
        rms += (float) (circBuffer_[i]*circBuffer_[i]);
    }

    rms = sqrt (rms / (float) bufferLength_);

    // printf("AudioDSP::computeRMS() : RMS VALUE: %f ", rms);
    return rms;

}
