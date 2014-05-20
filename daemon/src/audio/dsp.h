/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
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

#ifndef DSP_H_
#define DSP_H_

#include <speex/speex_preprocess.h>
#include <vector>
#include <memory>
#include "noncopyable.h"

class AudioBuffer;

class DSP {
    public:
        DSP(int smplPerFrame, int channels, int samplingRate);
        void enableAGC();
        void disableAGC();
        void enableDenoise();
        void disableDenoise();
        void process(AudioBuffer& buf, int samples);

    private:
        NON_COPYABLE(DSP);
        static void speexStateDeleter(SpeexPreprocessState *state);
        typedef std::unique_ptr<SpeexPreprocessState, decltype(&speexStateDeleter)> SpeexStatePtr;

        int smplPerFrame_;
        // one state per channel
        std::vector<SpeexStatePtr> dspStates_;
};

#endif // DSP_H_
