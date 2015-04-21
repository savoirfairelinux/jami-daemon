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
#ifndef WEBRTC_AUDIO_PROCESSING__H
#define WEBRTC_AUDIO_PROCESSING__H

#include "echocanceller.h"

#include <webrtc_audio_processing/audio_processing.h>
#include <webrtc_audio_processing/module_common_types.h>
#include <fstream>

#define BLOCK_SIZE_US

#define DEFAULT_HIGH_PASS_FILTER true
#define DEFAULT_NOISE_SUPPRESSION true
#define DEFAULT_ANALOG_GAIN_CONTROL false
#define DEFAULT_DIGITAL_GAIN_CONTROL true
//#define DEFAULT_MOBILE true
#define DEFAULT_MOBILE false
#define DEFAULT_ROUTING_MODE "speakerphone"
#define DEFAULT_COMFORT_NOISE true
#define DEFAULT_DRIFT_COMPENSATION true
//#define DEFAULT_DRIFT_COMPENSATION false

struct WebRtcEchoParams {
    webrtc::AudioProcessing *apm {nullptr};
};

class WebrtcAudioProcessing : public EchoCanceller {
    public:
        WebrtcAudioProcessing();
        ~WebrtcAudioProcessing();
        EchoCanceller * clone();

        void setPlaybackSamples(const int16_t * in_samples, int16_t * out_samples, long* captureVolume);

        void setCapturedSamples(const int16_t * in_samples);

        void setDrift(const float val);

        bool initWebRtcAudioProcessing();

    private:
        WebRtcEchoParams webRtcEchoParams_;
        std::ofstream fDebug_;
};
#endif // WEBRTC_AUDIO_PROCESSING__H
