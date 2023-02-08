/*
 *  Copyright (C) 2021-2023 Savoir-faire Linux Inc.
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
 */

#pragma once

#include "noncopyable.h"
#include "media/audio/audio_frame_resizer.h"
#include "media/audio/resampler.h"
#include "media/audio/audiobuffer.h"
#include "media/libav_deps.h"

#include <atomic>
#include <memory>

namespace jami {

class AudioProcessor
{
private:
    NON_COPYABLE(AudioProcessor);

public:
    AudioProcessor(AudioFormat format, unsigned frameSize)
        : playbackQueue_(format, (int) frameSize)
        , recordQueue_(format, (int) frameSize)
        , resampler_(new Resampler)
        , format_(format)
        , frameSize_(frameSize)
        , frameDurationMs_((unsigned int) (frameSize_ * (1.0 / format_.sample_rate) * 1000))
    {}
    virtual ~AudioProcessor() = default;

    virtual void putRecorded(std::shared_ptr<AudioFrame>&& buf)
    {
        recordStarted_ = true;
        if (!playbackStarted_)
            return;
        enqueue(recordQueue_, std::move(buf));
    };
    virtual void putPlayback(const std::shared_ptr<AudioFrame>& buf)
    {
        playbackStarted_ = true;
        if (!recordStarted_)
            return;
        auto copy = buf;
        enqueue(playbackQueue_, std::move(copy));
    };

    /**
     * @brief Process and return a single AudioFrame
     */
    virtual std::shared_ptr<AudioFrame> getProcessed() = 0;

    /**
     * @brief Set the status of echo cancellation
     */
    virtual void enableEchoCancel(bool enabled) = 0;

    /**
     * @brief Set the status of noise suppression
     * includes de-reverb, de-noise, high pass filter, etc
     */
    virtual void enableNoiseSuppression(bool enabled) = 0;

    /**
     * @brief Set the status of automatic gain control
     */
    virtual void enableAutomaticGainControl(bool enabled) = 0;

    /**
     * @brief Set the status of voice activity detection
     */
    virtual void enableVoiceActivityDetection(bool enabled) = 0;

protected:
    AudioFrameResizer playbackQueue_;
    AudioFrameResizer recordQueue_;
    std::unique_ptr<Resampler> resampler_;
    std::atomic_bool playbackStarted_;
    std::atomic_bool recordStarted_;
    AudioFormat format_;
    unsigned int frameSize_;
    unsigned int frameDurationMs_;

    // artificially extend voice activity by this long
    unsigned int forceMinimumVoiceActivityMs {1000};

    // current number of frames to force the voice activity to be true
    unsigned int forceVoiceActiveFramesLeft {0};

    // voice activity must be active for this long _before_ it is considered legitimate
    unsigned int minimumConsequtiveDurationMs {200};

    // current number of frames that the voice activity has been true
    unsigned int consecutiveActiveFrames {0};

    /**
     * @brief Helper method for audio processors, should be called at start of getProcessed()
     *        Pops frames from audio queues if there's overflow
     * @returns True if there is underflow, false otherwise. An AudioProcessor should
     *          return a blank AudioFrame if there is underflow.
     */
    bool tidyQueues()
    {
        while (recordQueue_.samples() > recordQueue_.frameSize() * 10) {
            JAMI_DBG("record overflow %d / %d", recordQueue_.samples(), frameSize_);
            recordQueue_.dequeue();
        }
        while (playbackQueue_.samples() > playbackQueue_.frameSize() * 10) {
            JAMI_DBG("playback overflow %d / %d", playbackQueue_.samples(), frameSize_);
            playbackQueue_.dequeue();
        }
        if (recordQueue_.samples() < recordQueue_.frameSize()
            || playbackQueue_.samples() < playbackQueue_.frameSize()) {
            // If there are not enough samples in either queue, we can't
            // process anything.
            return true;
        }
        return false;
    }

    /**
     * @brief Stablilizes voice activity
     * @param voiceStatus the voice status that was detected by the audio processor
     *                    for the current frame
     * @returns The voice activity status that should be set on the current frame
     */
    bool getStabilizedVoiceActivity(bool voiceStatus)
    {
        bool newVoice = false;

        if (voiceStatus) {
            // we detected activity
            consecutiveActiveFrames += 1;

            // make sure that we have been active for necessary time
            if (consecutiveActiveFrames > minimumConsequtiveDurationMs / frameDurationMs_) {
                newVoice = true;

                // set number of frames that will be forced positive
                forceVoiceActiveFramesLeft = (int) forceMinimumVoiceActivityMs / frameDurationMs_;
            }
        } else if (forceVoiceActiveFramesLeft > 0) {
            // if we didn't detect voice, but we haven't elapsed the minimum duration,
            // force voice to be true
            newVoice = true;
            forceVoiceActiveFramesLeft -= 1;

            consecutiveActiveFrames += 1;
        } else {
            // else no voice and no need to force
            newVoice = false;
            consecutiveActiveFrames = 0;
        }

        return newVoice;
    }

private:
    void enqueue(AudioFrameResizer& frameResizer, std::shared_ptr<AudioFrame>&& buf)
    {
        if (buf->getFormat() != format_) {
            auto resampled = resampler_->resample(std::move(buf), format_);
            frameResizer.enqueue(std::move(resampled));
        } else
            frameResizer.enqueue(std::move(buf));
    };
};

} // namespace jami
