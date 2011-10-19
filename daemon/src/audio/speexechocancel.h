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

class SpeexEchoCancel {
    public:

        SpeexEchoCancel();
        ~SpeexEchoCancel();

        /**
         * Add speaker data into internal buffer
         * \param inputData containing far-end voice data to be sent to speakers
         */
        void putData(SFLDataFormat *, int samples);

        /**
         * Perform echo cancellation using internal buffers
         * \param inputData containing mixed echo and voice data
         * \param outputData containing
         */
        int process(SFLDataFormat *, SFLDataFormat *, int samples);

    private:

        SpeexEchoState *echoState_;

        SpeexPreprocessState *preState_;

        RingBuffer *micData_;
        RingBuffer *spkrData_;

        int echoDelay_;
        int echoTailLength_;

        bool spkrStopped_;

        SFLDataFormat tmpSpkr_[5000];
        SFLDataFormat tmpMic_[5000];
        SFLDataFormat tmpOut_[5000];
};

#endif
