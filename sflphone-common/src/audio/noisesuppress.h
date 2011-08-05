/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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

#ifndef NOISESUPPRESS_H
#define NOISESUPPRESS_H

#include <speex/speex_preprocess.h>
#include "algorithm.h"
#include "audioprocessing.h"


class NoiseSuppress : public Algorithm
{

    public:

        NoiseSuppress (int smplPerFrame, int samplingRate);

        ~NoiseSuppress (void);

        /**
             * Reset noise suppressor internal state at runtime. Usefull when making a new call
             */
        virtual void reset (void);

        /**
         * Unused
        */
        virtual void putData (SFLDataFormat *inputData, int nbBytes);

        /**
        * Unused
         */
        virtual int getData (SFLDataFormat *outputData);

        /**
         * Unused
         */
        virtual void process (SFLDataFormat *data, int nbBytes);

        /**
         * Unused
        */
        virtual int process (SFLDataFormat *inputData, SFLDataFormat *outputData, int nbBytes);

        /**
        * Unused
         */
        virtual void process (SFLDataFormat *micData, SFLDataFormat *spkrData, SFLDataFormat *outputData, int nbBytes);

    private:

        void initNewNoiseSuppressor (int _smplPerFrame, int samplingRate);

        /**
             * Noise reduction processing state
             */
        SpeexPreprocessState *_noiseState;

        int _smplPerFrame;

        int _samplingRate;

};

#endif
