/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "audio_processor.h"

#if HAVE_WEBRTC_AP_LEGACY
namespace webrtc {
class AudioProcessing;
}
#else
#include <api/audio/audio_processing.h>
#include <api/scoped_refptr.h>
typedef struct WebRtcVadInst VadInst;
#endif

namespace jami {

class WebRTCAudioProcessor final : public AudioProcessor
{
public:
    WebRTCAudioProcessor(AudioFormat format, unsigned frameSize);
    ~WebRTCAudioProcessor() = default;

    // Inherited via AudioProcessor
    std::shared_ptr<AudioFrame> getProcessed() override;

    void enableEchoCancel(bool enabled) override;
    void enableNoiseSuppression(bool enabled) override;
    void enableAutomaticGainControl(bool enabled) override;
    void enableVoiceActivityDetection(bool enabled) override;

private:
#if HAVE_WEBRTC_AP_LEGACY
    std::unique_ptr<webrtc::AudioProcessing> apm;
#else
    void applyConfig();

    rtc::scoped_refptr<webrtc::AudioProcessing> apm;
    webrtc::AudioProcessing::Config config_;
    struct VadDeleter { void operator()(VadInst*) const; };
    std::unique_ptr<VadInst, VadDeleter> vad_;
    std::vector<int16_t> vadBuffer_;
    bool detectVoice_ {false};
#endif
    int analogLevel_ {0};
};
} // namespace jami
