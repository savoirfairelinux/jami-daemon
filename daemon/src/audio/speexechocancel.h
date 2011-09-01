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

#ifndef SPEEXECHOCANCEL_H
#define SPEEXECHOCANCEL_H

#include "global.h"

class RingBuffer;
class SpeexEchoState_;
typedef SpeexEchoState_ SpeexEchoState;
class SpeexPreprocessState_;
typedef SpeexPreprocessState_ SpeexPreprocessState;

class SpeexEchoCancel
{
    public:

        SpeexEchoCancel();
        ~SpeexEchoCancel();

        /**
         * Add speaker data into internal buffer
         * \param inputData containing far-end voice data to be sent to speakers
         */
        void putData (SFLDataFormat *, int);

        /**
         * Perform echo cancellation using internal buffers
         * \param inputData containing mixed echo and voice data
         * \param outputData containing
         */
        int process (SFLDataFormat *, SFLDataFormat *, int);

    private:

        SpeexEchoState *_echoState;

        SpeexPreprocessState *_preState;

        RingBuffer *_micData;
        RingBuffer *_spkrData;

        int _echoDelay;
        int _echoTailLength;

        bool _spkrStopped;

        SFLDataFormat _tmpSpkr[5000];
        SFLDataFormat _tmpMic[5000];
        SFLDataFormat _tmpOut[5000];

#ifdef DUMP_ECHOCANCEL_INTERNAL_DATA
        ofstream *micFile;
        ofstream *spkrFile;
        ofstream *micProcessFile;
        ofstream *spkrProcessFile;
        ofstream *echoFile;
#endif
};

#endif
