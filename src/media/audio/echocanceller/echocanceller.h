/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
 *  Author:  Eloi Bail <eloi.bail@savoirfairelinux.com>
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
#ifndef ECHO_CANCELLER__H
#define ECHO_CANCELLER__H

#include <memory>
#include "audio/audiobuffer.h"
//namespace DRing::EchoCancellerPlugin {

enum EchoCancellerId : unsigned {
    ECHO_CANCELLER_NONE = 0,
    ECHO_CANCELLER_WEBRTC = 1,
};

struct EchoCancellerParams {
    bool hasDriftCompensation {false};
    bool hasAGC {false};
    unsigned blocksize_capture {0};
    unsigned blocksize_playback {0};
    unsigned bytesPerSample {0};
    unsigned currentVolume {0};
    std::shared_ptr<ring::AudioFormat> audioFormat ;
};


class EchoCanceller
{
    public:
        EchoCanceller(EchoCancellerId id);
        ~EchoCanceller();

        virtual EchoCanceller * clone() = 0;
        virtual void setPlaybackSamples(const int16_t * in_samples, int16_t * out_samples, long* captureVolume) = 0;
        virtual void setCapturedSamples(const int16_t * in_samples) = 0;
        virtual void setDrift(const float val) = 0;

        EchoCancellerId getId();

        EchoCancellerParams ecParams;

    protected:
        EchoCancellerId id_;


};
//}// namespace ring
#endif
