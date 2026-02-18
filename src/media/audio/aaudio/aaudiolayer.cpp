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

#include "aaudiolayer.h"
#include "logger.h"

#include <aaudio/AAudio.h>
#include <dlfcn.h>

#include <string>

namespace jami {

// Signature for functions not available on older Android versions
using SetUsageFunc = void (*)(AAudioStreamBuilder *, aaudio_usage_t);
using SetContentTypeFunc = void (*)(AAudioStreamBuilder *, aaudio_content_type_t);
using SetInputPresetFunc = void (*)(AAudioStreamBuilder *, aaudio_input_preset_t);

AAudioLayer::AAudioLayer(const AudioPreference& pref)
    : AudioLayer(pref)
{
    setHasNativeAEC(true);
    setHasNativeNS(true);
}

AAudioLayer::~AAudioLayer()
{
    {
        std::lock_guard lk(mutex_);
        isRunning_ = false;
        loopCv_.notify_all();
    }
    if (loopThread_.joinable())
        loopThread_.join();
    stopStream(AudioDeviceType::ALL);
}

void
AAudioLayer::loop()
{
    std::unique_lock lk(mutex_);
    while (true) {
        loopCv_.wait(lk, [this] { return not isRunning_ or not streamsToRestart_.empty(); });
        if (!isRunning_)
            break;
        auto streams = std::move(streamsToRestart_);
        for (auto stream : streams) {
            JAMI_WARNING("Restarting stream type {} after disconnection", (unsigned) stream);
            stopStreamLocked(stream);
            startStreamLocked(stream);
        }
    }
}

AudioFormat
getStreamFormat(AAudioStream* stream)
{
    auto sampleRate = AAudioStream_getSampleRate(stream);
    auto channelCount = AAudioStream_getChannelCount(stream);
    auto aaFormat = AAudioStream_getFormat(stream);
    return AudioFormat(sampleRate,
                       channelCount,
                       (aaFormat == AAUDIO_FORMAT_PCM_FLOAT) ? AV_SAMPLE_FMT_FLT : AV_SAMPLE_FMT_S16);
}

AAudioLayer::AAudioStreamPtr
AAudioLayer::buildStream(AudioDeviceType type) {
    AAudioStreamBuilder *builder;
    AAudio_createStreamBuilder(&builder);
    AAudioStreamBuilder_setDirection(builder,
                                     (type == AudioDeviceType::CAPTURE) ? AAUDIO_DIRECTION_INPUT
                                                                        : AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
    //      type == AudioDeviceType::RINGTONE ? AAUDIO_SHARING_MODE_SHARED
    //                                        : AAUDIO_SHARING_MODE_EXCLUSIVE);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_FLOAT);
    AAudioStreamBuilder_setPerformanceMode(builder, type == AudioDeviceType::RINGTONE
                                                    ? AAUDIO_PERFORMANCE_MODE_POWER_SAVING
                                                    : AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);

    static auto setUsage = reinterpret_cast<SetUsageFunc>(dlsym(RTLD_DEFAULT,
                                                                "AAudioStreamBuilder_setUsage"));
    static auto setContentType = reinterpret_cast<SetContentTypeFunc>(dlsym(RTLD_DEFAULT,
                                                                            "AAudioStreamBuilder_setContentType"));
    static auto setInputPreset = reinterpret_cast<SetInputPresetFunc>(dlsym(RTLD_DEFAULT,
                                                                            "AAudioStreamBuilder_setInputPreset"));
    if (setUsage) {
        setUsage(builder,
                 type == AudioDeviceType::RINGTONE ? AAUDIO_USAGE_NOTIFICATION_RINGTONE
                                                   : AAUDIO_USAGE_VOICE_COMMUNICATION);
    } else {
        JAMI_WARNING("AAudioStreamBuilder_setUsage not available, stream usage will be unknown");
    }
    if (setContentType) {
        setContentType(builder,
                       type == AudioDeviceType::RINGTONE ? AAUDIO_CONTENT_TYPE_MUSIC
                                                         : AAUDIO_CONTENT_TYPE_SPEECH);
    } else {
        JAMI_WARNING(
                "AAudioStreamBuilder_setContentType not available, stream content type will be unknown");
    }
    if (type == AudioDeviceType::CAPTURE) {
        if (setInputPreset) {
            setInputPreset(builder, AAUDIO_INPUT_PRESET_VOICE_COMMUNICATION);
        } else  {
            JAMI_WARNING( "AAudioStreamBuilder_setInputPreset not available, input preset will be unknown");
        }
    }

    AAudioStreamBuilder_setDataCallback(builder, dataCallback, this);
    AAudioStreamBuilder_setErrorCallback(builder, errorCallback, this);

    AAudioStream* streamptr = nullptr;
    aaudio_result_t result = AAudioStreamBuilder_openStream(builder, &streamptr);
    if (result != AAUDIO_OK && type != AudioDeviceType::RINGTONE) {
        JAMI_ERROR("Error opening {} stream: {}. Retrying with shared.",
                   (type == AudioDeviceType::CAPTURE) ? "capture" : "playback",
                   AAudio_convertResultToText(result));
        AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
        result = AAudioStreamBuilder_openStream(builder, &streamptr);
    }
    AAudioStreamBuilder_delete(builder);
    if (result != AAUDIO_OK) {
        JAMI_ERROR("Error opening {} stream (shared): {}",
                   (type == AudioDeviceType::CAPTURE) ? "capture" : "playback",
                   AAudio_convertResultToText(result));
        return nullptr;
    }
    return AAudioStreamPtr(streamptr);
}

void
AAudioLayer::startStream(AudioDeviceType stream)
{
    std::lock_guard lock(mutex_);
    startStreamLocked(stream);
}

void
AAudioLayer::startStreamLocked(AudioDeviceType stream)
{
    JAMI_WARNING("Starting AAudio layer for stream type {}", (unsigned) stream);
    status_ = Status::Starting;

    // Playback
    if (stream == AudioDeviceType::PLAYBACK) {
        if (playStream_)
            return;
        playStream_ = buildStream(AudioDeviceType::PLAYBACK);
        if (!playStream_) {
            JAMI_ERROR("Failed to create playback stream");
            return;
        }
        AudioFormat format = getStreamFormat(playStream_.get());

        // Start with minimal buffer size (low latency) and let the callback increase it if underruns occur.
        AAudioStream_setBufferSizeInFrames(playStream_.get(), AAudioStream_getFramesPerBurst(playStream_.get()) * 2);
        //AAudioStream_setBufferSizeInFrames(playStream_.get(), 20 * format.sample_rate / 1000);
        auto bufferSize = AAudioStream_getBufferCapacityInFrames(playStream_.get());
        hardwareFormatAvailable(format, bufferSize);

        // Request start
        auto result = AAudioStream_requestStart(playStream_.get());
        if (result == AAUDIO_OK) {
            JAMI_WARNING("Playback stream started with format: {}, buffer size: {} frames",
                         format.toString(),
                         bufferSize);
            status_ = Status::Started;
            playbackChanged(true);
        } else {
            JAMI_ERROR("Error starting playback stream: {}", AAudio_convertResultToText(result));
        }
    } else if (stream == AudioDeviceType::RINGTONE) {
        if (ringStream_)
            return;
        ringStream_ = buildStream(AudioDeviceType::RINGTONE);
        if (!ringStream_) {
            JAMI_ERROR("Error opening ringtone stream");
            return;
        }
        AudioFormat format = getStreamFormat(ringStream_.get());
        AAudioStream_setBufferSizeInFrames(ringStream_.get(), 20 * format.sample_rate / 1000);
        auto bufferSize = AAudioStream_getBufferCapacityInFrames(ringStream_.get());
        hardwareFormatAvailable(format, bufferSize);

        auto result = AAudioStream_requestStart(ringStream_.get());
        if (result == AAUDIO_OK) {
            JAMI_WARNING("Ringtone stream started with format: {}, buffer size: {} frames",
                         format.toString(),
                         bufferSize);
            status_ = Status::Started;
            playbackChanged(true);
        } else {
            JAMI_ERROR("Error starting ringtone stream: {}", AAudio_convertResultToText(result));
        }
    } else if (stream == AudioDeviceType::CAPTURE) {
        if (recStream_)
            return;
        recStream_ = buildStream(AudioDeviceType::CAPTURE);
        if (!recStream_) {
            JAMI_ERROR("Error opening capture stream");
            return;
        }
        auto result = AAudioStream_requestStart(recStream_.get());
        if (result == AAUDIO_OK) {
            AudioFormat format = getStreamFormat(recStream_.get());
            hardwareInputFormatAvailable(format);
            JAMI_WARNING("Capture stream started with format: {}", format.toString());
            recordChanged(true);
        } else {
            JAMI_ERROR("Error starting capture stream: {}", AAudio_convertResultToText(result));
        }
    }

    if (!isRunning_) {
        isRunning_ = true;
        loopThread_ = std::thread([this] { loop(); });
    }
}

void
AAudioLayer::stopStream(AudioDeviceType stream)
{
    std::lock_guard lock(mutex_);
    if (stream == AudioDeviceType::ALL) {
        streamsToRestart_.clear();
    } else {
        streamsToRestart_.erase(stream);
    }
    stopStreamLocked(stream);
}

void
AAudioLayer::stopStreamLocked(AudioDeviceType stream)
{
    JAMI_WARNING("Stopping AAudio layer for stream type {}", (unsigned) stream);

    if (stream == AudioDeviceType::PLAYBACK || stream == AudioDeviceType::ALL) {
        if (playStream_) {
            AAudioStream_requestStop(playStream_.get());
            playStream_.reset();
            playbackChanged(false);
        }
    }

    if (stream == AudioDeviceType::RINGTONE || stream == AudioDeviceType::ALL) {
        if (ringStream_) {
            AAudioStream_requestStop(ringStream_.get());
            ringStream_.reset();
        }
    }

    if (stream == AudioDeviceType::CAPTURE || stream == AudioDeviceType::ALL) {
        if (recStream_) {
            AAudioStream_requestStop(recStream_.get());
            recStream_.reset();
            recordChanged(false);
        }
    }
    status_ = Status::Idle;
}

aaudio_data_callback_result_t
AAudioLayer::dataCallback(AAudioStream* stream, void* userData, void* audioData, int32_t numFrames)
{
    AAudioLayer* layer = static_cast<AAudioLayer*>(userData);
    if (!layer)
        return AAUDIO_CALLBACK_RESULT_STOP;

    auto direction = AAudioStream_getDirection(stream);
    auto format = getStreamFormat(stream);
    int32_t numSamples = numFrames * format.nb_channels;

    if (direction == AAUDIO_DIRECTION_OUTPUT) {
        // JAMI_WARNING("Playback callback: {} frames, format: {}, sample rate: {}", numFrames, (aaFormat ==
        // AAUDIO_FORMAT_PCM_FLOAT) ? "Float" : "I16", sampleRate);

        // Optimize buffer size (Check for underruns)
        int32_t underrunCount = AAudioStream_getXRunCount(stream);
        if (underrunCount > layer->previousUnderrunCount_) {
            layer->previousUnderrunCount_ = underrunCount;
            auto bufferSize = AAudioStream_getBufferSizeInFrames(stream);
            auto bufferCapacity = AAudioStream_getBufferCapacityInFrames(stream);
            auto burst = AAudioStream_getFramesPerBurst(stream);

            if (bufferSize + burst <= bufferCapacity) {
                AAudioStream_setBufferSizeInFrames(stream, bufferSize + burst);
                JAMI_WARNING("Underrun detected (count: {}), increasing buffer size to {} frames",
                             underrunCount,
                             bufferSize + burst);
            }
        }

        auto frame = stream == layer->ringStream_.get() ? layer->getToRing(format, numFrames)
                                                        : layer->getToPlay(format, numFrames);
        if (frame && frame->pointer() && frame->pointer()->data[0]) {
            float* output = static_cast<float*>(audioData);
            const float* src = reinterpret_cast<const float*>(frame->pointer()->data[0]);
            std::copy(src, src + numSamples, output);
            if (frame->pointer()->nb_samples != numFrames) {
                JAMI_WARNING("Frame size {} does not match requested size {}, filling remaining with silence",
                             frame->pointer()->nb_samples,
                             numFrames);
            }
        } else {
            JAMI_WARNING("Playback underflow: no data available, filling with silence");
            std::fill_n(static_cast<float*>(audioData), numSamples, 0.0f);
        }
    } else if (direction == AAUDIO_DIRECTION_INPUT) { // Capture
        auto out = std::make_shared<AudioFrame>(format, numFrames);
        if (out->pointer() && out->pointer()->data[0]) {
            auto* dst = reinterpret_cast<float*>(out->pointer()->data[0]);
            const auto* src = static_cast<const float*>(audioData);
            std::copy(src, src + numSamples, dst);
            layer->putRecorded(std::move(out));
        }
    }

    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

void
AAudioLayer::errorCallback(AAudioStream* stream, void* userData, aaudio_result_t error)
{
    AAudioLayer* layer = static_cast<AAudioLayer*>(userData);
    auto streamType = (stream == layer->playStream_.get())  ? AudioDeviceType::PLAYBACK
                                                            : (stream == layer->recStream_.get()) ? AudioDeviceType::CAPTURE
                                                                                                  : AudioDeviceType::RINGTONE;
    JAMI_ERROR("AAudio error: {} for type {}", AAudio_convertResultToText(error), (unsigned) streamType);

    if (error == AAUDIO_ERROR_DISCONNECTED) {
        std::lock_guard lk(layer->mutex_);
        layer->streamsToRestart_.insert(streamType);
        layer->loopCv_.notify_one();
    }
}

std::vector<std::string>
AAudioLayer::getCaptureDeviceList() const
{
    return {"Default"}; // Helper to properly list devices using Android Java API or Oboe later
}

std::vector<std::string>
AAudioLayer::getPlaybackDeviceList() const
{
    return {"Default"};
}

int
AAudioLayer::getAudioDeviceIndex(const std::string&, AudioDeviceType) const
{
    return 0;
}

std::string
AAudioLayer::getAudioDeviceName(int, AudioDeviceType) const
{
    return "Default";
}

void
AAudioLayer::updatePreference(AudioPreference&, int, AudioDeviceType)
{}

} // namespace jami
