/*
 *  Copyright (C) 2004-2025 Savoir-faire Linux Inc.
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

#include "noncopyable.h"
#include "media/audio/audio_frame_resizer.h"
#include "media/audio/resampler.h"
#include "media/audio/audio_format.h"
#include "media/libav_deps.h"
#include "logger.h"

#include <atomic>
#include <memory>
#include <cmath>

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
        // JAMI_DBG("<------ AudioProcessor::putRecorded num_samples: %d", buf->getFrameSize());
        enqueue(recordQueue_, std::move(buf));
    };

    virtual void putPlayback(const std::shared_ptr<AudioFrame>& buf)
    {
        playbackStarted_ = true;
        if (!recordStarted_)
            return;
        auto copy = buf;
        // JAMI_DBG("------> AudioProcessor::putPlayback num_samples: %d", copy->getFrameSize());
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

    /**
     * @brief Gets the current drift ratio between record and playback queues
     * @returns The current drift ratio (1.0 means perfectly synchronized)
     */
    float getDriftRatio() const { return currentDriftRatio_; }

    /**
     * @brief Calculate the delay between playback and record streams in milliseconds
     * @returns The delay in milliseconds (positive means playback is ahead of record)
     */
    int getStreamDelayMs() const
    {
        size_t playbackSamples = playbackQueue_.samples();
        size_t recordSamples = recordQueue_.samples();

        // Handle empty buffer case to prevent erratic values
        if (playbackSamples == 0 || recordSamples == 0) {
            return 0;
        }

        // Convert sample difference to milliseconds
        // Positive means playback is ahead of record (render is ahead of capture)
        int delayMs = 0;
        if (playbackSamples >= recordSamples) {
            delayMs = static_cast<int>(
                (static_cast<float>(playbackSamples - recordSamples) / format_.sample_rate) * 1000);
        } else {
            delayMs = -static_cast<int>(
                (static_cast<float>(recordSamples - playbackSamples) / format_.sample_rate) * 1000);
        }

        // Limit extreme delay values to prevent AEC issues
        // WebRTC generally accepts delays between -256 and 256 ms
        return std::clamp(delayMs, -250, 250);
    }

    /**
     * @brief Calculate the drift in samples per frame based on current drift ratio
     * @returns The number of samples of drift per frame
     */
    int getDriftSamplesPerFrame() const
    {
        // Limit extreme drift ratios to prevent computational issues
        float limitedDrift = std::clamp(currentDriftRatio_, 0.75f, 1.25f);

        // Current drift ratio (1.0 means no drift)
        float drift = limitedDrift - 1.0f;

        // Convert to samples per second
        float samplesPerSecond = drift * format_.sample_rate;

        // Convert to samples per frame
        return static_cast<int>(samplesPerSecond * frameDurationMs_ / 1000.0f);
    }

    /**
     * @brief Attempt to compensate for persistent drift by periodically dropping frames
     * This method should be called by getProcessed() implementations for better drift handling
     */
    void compensatePersistentDrift()
    {
        static int frameCount = 0;
        static const int frameInterval = 200; // Even less frequent checks

        // Only act very rarely to avoid causing audio gaps
        if (++frameCount % frameInterval != 0) {
            return;
        }

        // Only take action for extreme persistent drift
        if (currentDriftRatio_ > 1.3f) { // Even higher threshold
            // Record is consistently much faster than playback
            // Very occasionally drop a record frame to maintain balance
            size_t recordFrames = recordQueue_.samples() / recordQueue_.frameSize();
            if (recordFrames > 10) { // Much higher threshold - only if we have many frames
                JAMI_DEBUG("Compensating for extreme persistent drift (ratio: {:.3f}) - dropping "
                           "record frame",
                           currentDriftRatio_);
                smartDequeue(recordQueue_);
            }
        } else if (currentDriftRatio_ < 0.7f) { // Even lower threshold
            // Playback is consistently much faster than record
            // Very occasionally drop a playback frame to maintain balance
            size_t playbackFrames = playbackQueue_.samples() / playbackQueue_.frameSize();
            if (playbackFrames > 10) { // Much higher threshold - only if we have many frames
                JAMI_DEBUG("Compensating for extreme persistent drift (ratio: {:.3f}) - dropping "
                           "playback frame",
                           currentDriftRatio_);
                smartDequeue(playbackQueue_);
            }
        }
    }

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
     * @brief Tracks and compensates for clock drift between recording and playback devices
     * @returns True if there is underflow, false otherwise
     */
    bool synchronizeBuffers()
    {
        // Calculate current queue states
        size_t recordSamples = recordQueue_.samples();
        size_t playbackSamples = playbackQueue_.samples();

        // Special handling for when playback is empty but record has data
        // This prevents the imbalance shown in logs with continuous record overflows
        if (playbackSamples == 0 && recordSamples > recordQueue_.frameSize() * 3) {
            // Only drop record frames if we have many of them - not too aggressively
            JAMI_DEBUG("Dropping record frame due to empty playback, record: {:d}",
                       recordQueue_.samples());
            smartDequeue(recordQueue_);
            // Continue processing if we still have enough samples
            if (recordSamples > recordQueue_.frameSize()) {
                return false;
            }
        }

        // Check for underflow - not enough samples to process a frame
        if (recordSamples < recordQueue_.frameSize()
            || playbackSamples < playbackQueue_.frameSize()) {
            return true;
        }

        // With a frame size typically being 10-20ms, 10x multiplication means we allow up
        // to 100-200ms of buffering. This is generally considered acceptable for real-time
        // audio communication, as latencies under 200ms are not very noticeable to users
        static constexpr size_t maxQueueMultiple = 10; // Back to 10 - avoid overly aggressive drops

        // Some reasonable limit to prevent infinite loops
        static constexpr size_t maxIterations = 100;
        size_t iterations = 0;

        // Calculate target ratio based on frame sizes
        float targetRatio = static_cast<float>(recordQueue_.frameSize())
                            / playbackQueue_.frameSize();

        // Calculate current ratio
        float currentRatio = static_cast<float>(recordSamples)
                             / std::max<size_t>(playbackSamples, 1UL);

        // Track drift metrics over time using an Exponential Moving Average (EMA)
        // This helps identify and adjust for systematic clock drift between audio devices
        // When microphone and speakers are on different interfaces, their clocks can drift
        // 1.0 = perfect sync, >1.0 = record faster than playback, <1.0 = playback faster than record
        static float averageDriftRatio = 1.0f;

        // Use different smoothing factors depending on drift behavior
        // Less smoothing (higher alpha) for stable periods, more smoothing during unstable periods
        static constexpr float driftAlphaStable
            = 0.02f; // Lower - smoother update during stable periods
        static constexpr float driftAlphaUnstable
            = 0.005f; // Lower - very smooth update during unstable periods

        // Determine if we're in a stable or unstable period
        float currentAlpha;
        static float lastMeasuredDrift = 1.0f;
        float driftDelta = std::abs(currentRatio / targetRatio - lastMeasuredDrift);

        // If significant change from last measurement, we're in unstable period
        if (driftDelta > 0.05f) {
            currentAlpha = driftAlphaUnstable; // Use slower update during unstable periods
        } else {
            currentAlpha = driftAlphaStable; // Use faster update during stable periods
        }

        // Save for next comparison
        lastMeasuredDrift = currentRatio / targetRatio;

        // Update drift measurement with exponential moving average
        averageDriftRatio = (1.0f - currentAlpha) * averageDriftRatio
                            + currentAlpha * (currentRatio / targetRatio);

        // Limit extreme drift values to avoid over-correction
        if (averageDriftRatio > 2.0f) {
            JAMI_DEBUG("Limiting extreme drift ratio from {:.3f} to 2.0", averageDriftRatio);
            averageDriftRatio = 2.0f;
        } else if (averageDriftRatio < 0.5f) {
            JAMI_DEBUG("Limiting extreme drift ratio from {:.3f} to 0.5", averageDriftRatio);
            averageDriftRatio = 0.5f;
        }

        // Save current drift ratio for processors that want to access it
        currentDriftRatio_ = averageDriftRatio;

        // Significant drift detected - more aggressive for large drifts
        bool significantDrift = std::abs(averageDriftRatio - 1.0f)
                                > 0.1f; // Higher threshold (0.05 -> 0.1)
        bool extremeDrift = std::abs(averageDriftRatio - 1.0f)
                            > 0.2f; // Higher threshold (0.1 -> 0.2)

        // Maximum allowed buffer size adjustments based on drift severity
        // Less aggressive buffer size limits
        size_t maxRecordBuffer = recordQueue_.frameSize()
                                 * (extremeDrift ? 5 : (significantDrift ? 7 : maxQueueMultiple));
        size_t maxPlaybackBuffer = playbackQueue_.frameSize()
                                   * (extremeDrift ? 5 : (significantDrift ? 7 : maxQueueMultiple));

        // Handle immediate queue balance issues
        while (iterations++ < maxIterations) {
            recordSamples = recordQueue_.samples();
            playbackSamples = playbackQueue_.samples();

            // Check for overflow (with respect to the max queue size) in either queue
            bool recordOverflow = recordSamples > maxRecordBuffer;
            bool playbackOverflow = playbackSamples > maxPlaybackBuffer;

            // Exit early if any queue is empty after dequeuing
            if (recordSamples == 0 || playbackSamples == 0) {
                break;
            }

            // Calculate current ratio
            currentRatio = static_cast<float>(recordSamples)
                           / std::max<size_t>(playbackSamples, 1UL);

            // Check queue ratio
            // Use a wider ratio bound to be less aggressive in fixing it
            float ratioBound = extremeDrift ? 0.5f : (significantDrift ? 0.7f : 0.9f);
            bool ratioImbalance = std::abs(currentRatio - targetRatio) > targetRatio * ratioBound;

            // If there is no overflow and the ratio is close to the target, we can stop
            if (!recordOverflow && !playbackOverflow && !ratioImbalance) {
                break;
            }

            // When extreme drift is detected, be more conservative
            if (extremeDrift) {
                // If extreme drift is occurring, add more conservative balancing
                if (averageDriftRatio > 1.2f) { // Higher threshold (1.0 -> 1.2)
                    // Record is running faster than playback - drop record frames conservatively
                    if (recordSamples
                        > recordQueue_.frameSize() * 3) { // Higher threshold (1.5 -> 3)
                        JAMI_DEBUG("Conservative record overflow {:d} / {:d} - playback: {:d}, "
                                   "drift ratio: {:.3f}",
                                   recordSamples,
                                   recordQueue_.frameSize(),
                                   playbackSamples,
                                   averageDriftRatio);
                        smartDequeue(recordQueue_);
                        continue;
                    }
                } else if (averageDriftRatio < 0.8f) { // Lower threshold (1.0 -> 0.8)
                    // Playback is running faster than record - drop playback frames conservatively
                    if (playbackSamples
                        > playbackQueue_.frameSize() * 3) { // Higher threshold (1.5 -> 3)
                        JAMI_DEBUG("Conservative playback overflow {:d} / {:d} - record: {:d}, "
                                   "drift ratio: {:.3f}",
                                   playbackSamples,
                                   playbackQueue_.frameSize(),
                                   recordSamples,
                                   averageDriftRatio);
                        smartDequeue(playbackQueue_);
                        continue;
                    }
                }
            }

            // Only drop frames when there is a significant overflow
            if (recordOverflow && recordSamples > recordQueue_.frameSize() * 2) {
                JAMI_DEBUG("record overflow {:d} / {:d} - playback: {:d}, drift ratio: {:.3f}",
                           recordSamples,
                           recordQueue_.frameSize(),
                           playbackSamples,
                           averageDriftRatio);
                smartDequeue(recordQueue_);
            } else if (playbackOverflow && playbackSamples > playbackQueue_.frameSize() * 2) {
                JAMI_DEBUG("playback overflow {:d} / {:d} - record: {:d}, drift ratio: {:.3f}",
                           playbackSamples,
                           playbackQueue_.frameSize(),
                           recordSamples,
                           averageDriftRatio);
                smartDequeue(playbackQueue_);
            } else {
                // If no significant overflow, stop trying to balance
                break;
            }
        }

        // Log significant drift for debugging
        if (significantDrift && iterations == 1) {
            JAMI_DEBUG("Clock drift detected - ratio: {:.3f}, record: {:d}, playback: {:d}",
                       averageDriftRatio,
                       recordSamples,
                       playbackSamples);
        }

        return false;
    }

    /**
     * @brief Helper method for audio processors, should be called at start of getProcessed()
     *        Pops frames from audio queues if there's overflow. The objective is to ensure
     *        that the ratio between the record and playback queues is as close to 1 as possible.
     * @returns True if there is underflow, false otherwise
     */
    bool tidyQueues()
    {
        // With a frame size typically being 10-20ms, 10x multiplication means we allow up
        // to 100-200ms of buffering. This is generally considered acceptable for real-time
        // audio communication, as latencies under 200ms are not very noticeable to users
        static constexpr size_t maxQueueMultiple = 10;

        // Some reasonable limit to prevent infinite loops
        static constexpr size_t maxIterations = 100;
        size_t iterations = 0;

        // Calculate target ratio based on frame sizes
        float targetRatio = static_cast<float>(recordQueue_.frameSize())
                            / playbackQueue_.frameSize();

        while (iterations++ < maxIterations) {
            size_t recordSamples = recordQueue_.samples();
            size_t playbackSamples = playbackQueue_.samples();

            // Check for underflow first
            if (recordSamples < recordQueue_.frameSize()
                || playbackSamples < playbackQueue_.frameSize()) {
                return true;
            }

            // Check for overflow (with respect to the max queue size) in either queue
            bool recordOverflow = recordSamples > recordQueue_.frameSize() * maxQueueMultiple;
            bool playbackOverflow = playbackSamples > playbackQueue_.frameSize() * maxQueueMultiple;

            // Check queue ratio
            float currentRatio = static_cast<float>(recordSamples) / playbackSamples;
            bool ratioImbalance = std::abs(currentRatio - targetRatio) > targetRatio * 0.5f;

            // If there is no overflow and the ratio is close to the target, we can stop
            if (!recordOverflow && !playbackOverflow && !ratioImbalance) {
                break;
            }

            // Dequeue from the queue that has overflow
            if (currentRatio > targetRatio) {
                JAMI_DEBUG("record overflow {:d} / {:d} - playback: {:d}",
                           recordSamples,
                           frameSize_,
                           playbackSamples);
                recordQueue_.dequeue();
            } else {
                JAMI_DEBUG("playback overflow {:d} / {:d} - record: {:d}",
                           playbackSamples,
                           frameSize_,
                           recordSamples);
                playbackQueue_.dequeue();
            }
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

    /**
     * @brief Removes a frame from the queue with improved crossfade to prevent clicks/pops
     * @param queue The queue to remove a frame from
     * @return True if a frame was successfully dequeued
     */
    bool smoothDequeue(AudioFrameResizer& queue)
    {
        if (queue.samples() < queue.frameSize() * 2) {
            // Fall back to regular dequeue if not enough frames for crossfade
            auto frame = queue.dequeue();
            return frame != nullptr;
        }

        // Dequeue the first frame but save it for crossfade
        auto frame1 = queue.dequeue();
        if (!frame1) {
            return false;
        }

        // Peek at the next frame (don't remove it yet)
        auto frame2 = queue.peek();
        if (!frame2) {
            return false;
        }

        // Skip crossfade if formats don't match
        if (frame1->getFormat() != frame2->getFormat()) {
            return true;
        }

        const int sampleSize = av_get_bytes_per_sample(frame1->getFormat().sampleFormat);
        const int channels = frame1->getFormat().nb_channels;
        const int frameSizeSamples = frame2->getFrameSize();

        // Number of samples to crossfade (up to 30% of frame size for smoother transition)
        // Use a minimum of 5ms worth of samples at the given sample rate
        const int minFadeSamples = std::max<int>(static_cast<int>(
                                                     5 * frame1->getFormat().sample_rate / 1000),
                                                 20);
        const int maxFadeSamples = frameSizeSamples * 3 / 10;
        const int fadeSamples = std::min(maxFadeSamples,
                                         std::max(minFadeSamples, frameSizeSamples / 5));

        // Only crossfade if we have enough samples
        if (fadeSamples > 0 && frameSizeSamples > fadeSamples) {
            // Apply crossfade based on audio format
            for (int ch = 0; ch < channels; ch++) {
                uint8_t* src1 = frame1->pointer()->extended_data[ch]
                                + (frameSizeSamples - fadeSamples) * sampleSize;
                uint8_t* src2 = frame2->pointer()->extended_data[ch];

                // Apply better crossfade with raised cosine (Hann) window for smoother transition
                if (frame1->getFormat().sampleFormat == AV_SAMPLE_FMT_S16) {
                    int16_t* samples1 = reinterpret_cast<int16_t*>(src1);
                    int16_t* samples2 = reinterpret_cast<int16_t*>(src2);

                    for (int i = 0; i < fadeSamples; i++) {
                        // Use Hann window for smoother transition than linear
                        float ratio = static_cast<float>(i) / fadeSamples;
                        float weight2 = 0.5f * (1.0f - std::cos(M_PI * ratio));
                        float weight1 = 1.0f - weight2;

                        // Blend using the weights
                        samples2[i] = static_cast<int16_t>(samples1[i] * weight1
                                                           + samples2[i] * weight2);
                    }
                } else if (frame1->getFormat().sampleFormat == AV_SAMPLE_FMT_FLT) {
                    float* samples1 = reinterpret_cast<float*>(src1);
                    float* samples2 = reinterpret_cast<float*>(src2);

                    for (int i = 0; i < fadeSamples; i++) {
                        // Use Hann window for smoother transition than linear
                        float ratio = static_cast<float>(i) / fadeSamples;
                        float weight2 = 0.5f * (1.0f - std::cos(M_PI * ratio));
                        float weight1 = 1.0f - weight2;

                        // Blend using the weights
                        samples2[i] = samples1[i] * weight1 + samples2[i] * weight2;
                    }
                } else if (frame1->getFormat().sampleFormat == AV_SAMPLE_FMT_S32) {
                    int32_t* samples1 = reinterpret_cast<int32_t*>(src1);
                    int32_t* samples2 = reinterpret_cast<int32_t*>(src2);

                    for (int i = 0; i < fadeSamples; i++) {
                        float ratio = static_cast<float>(i) / fadeSamples;
                        float weight2 = 0.5f * (1.0f - std::cos(M_PI * ratio));
                        float weight1 = 1.0f - weight2;

                        samples2[i] = static_cast<int32_t>(samples1[i] * weight1
                                                           + samples2[i] * weight2);
                    }
                }
                // Add support for more formats as needed
            }
        }

        return true;
    }

    /**
     * @brief Smart dequeue that applies smoothing when dropping frames
     * @param queue The queue to dequeue from
     * @return The number of samples dequeued, or 0 if operation failed
     */
    size_t smartDequeue(AudioFrameResizer& queue)
    {
        size_t samplesBeforeDrop = queue.samples();

        // Try to apply smoothing if possible
        if (!smoothDequeue(queue)) {
            // Fall back to regular dequeue if smoothing not possible
            auto frame = queue.dequeue();
            if (!frame) {
                JAMI_DEBUG("Failed to dequeue frame - queue may be empty");
                return 0;
            }
            return frame->getFrameSize();
        }

        // Calculate number of samples that were dequeued
        size_t samplesAfterDrop = queue.samples();
        size_t samplesDequeued = (samplesBeforeDrop > samplesAfterDrop)
                                     ? samplesBeforeDrop - samplesAfterDrop
                                     : queue.frameSize();

        return samplesDequeued;
    }

private:
    void enqueue(AudioFrameResizer& frameResizer, std::shared_ptr<AudioFrame>&& buf)
    {
        if (buf->getFormat() != format_) {
            frameResizer.enqueue(resampler_->resample(std::move(buf), format_));
        } else
            frameResizer.enqueue(std::move(buf));
    };

    // Tracks the current drift ratio between record and playback queues
    float currentDriftRatio_ {1.0f};
};

} // namespace jami
