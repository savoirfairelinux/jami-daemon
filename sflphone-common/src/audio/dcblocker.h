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

#ifndef DCBLOCKER_H
#define DCBLOCKER_H

#include "algorithm.h"
#include "global.h"

class DcBlocker : public Algorithm {

public:

    DcBlocker();

    ~DcBlocker();

    /**
     * Unused
     */
    virtual void putData(SFLDataFormat *inputData, int nbBytes);

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
