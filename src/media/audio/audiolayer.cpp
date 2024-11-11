/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
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

#include "audiolayer.h"
#include "logger.h"
#include "manager.h"
#include "audio/ringbufferpool.h"
#include "audio/resampler.h"
#include "tonecontrol.h"
#include "client/ring_signal.h"

#include "audio-processing/null_audio_processor.h"
#include "tracepoint.h"
#if HAVE_WEBRTC_AP
#include "audio-processing/webrtc.h"
#endif
#if HAVE_SPEEXDSP
#include "audio-processing/speex.h"
#endif

#include <ctime>
#include <algorithm>

namespace jami {

AudioLayer::AudioLayer(const AudioPreference& pref)
    : isCaptureMuted_(pref.getCaptureMuted())
    , isPlaybackMuted_(pref.getPlaybackMuted())
    , captureGain_(pref.getVolumemic())
    , playbackGain_(pref.getVolumespkr())
    , pref_(pref)
    , mainRingBuffer_(
          Manager::instance().getRingBufferPool().getRingBuffer(RingBufferPool::DEFAULT_ID))
    , audioFormat_(Manager::instance().getRingBufferPool().getInternalAudioFormat())
    , audioInputFormat_(Manager::instance().getRingBufferPool().getInternalAudioFormat())
    , urgentRingBuffer_("urgentRingBuffer_id", SIZEBUF, audioFormat_)
    , resampler_(new Resampler)
    , lastNotificationTime_()
{
    urgentRingBuffer_.createReadOffset(RingBufferPool::DEFAULT_ID);

    JAMI_LOG("[audiolayer] AGC: {:d}, noiseReduce: {:s}, VAD: {:d}, echoCancel: {:s}, audioProcessor: {:s}",
              pref_.isAGCEnabled(),
              pref.getNoiseReduce(),
              pref.getVadEnabled(),
              pref.getEchoCanceller(),
              pref.getAudioProcessor());
}

AudioLayer::~AudioLayer() {}

void
AudioLayer::hardwareFormatAvailable(AudioFormat playback, size_t bufSize)
{
    JAMI_LOG("Hardware audio format available : {:s} {}", playback.toString(), bufSize);
    audioFormat_ = Manager::instance().hardwareAudioFormatChanged(playback);
    audioInputFormat_.sampleFormat = audioFormat_.sampleFormat;
    urgentRingBuffer_.setFormat(audioFormat_);
    nativeFrameSize_ = bufSize;
}

void
AudioLayer::hardwareInputFormatAvailable(AudioFormat capture)
{
    JAMI_LOG("Hardware input audio format available : {:s}", capture.toString());
}

void
AudioLayer::devicesChanged()
{
    emitSignal<libjami::AudioSignal::DeviceEvent>();
}

void
AudioLayer::flushMain()
{
    Manager::instance().getRingBufferPool().flushAllBuffers();
}

void
AudioLayer::flushUrgent()
{
    urgentRingBuffer_.flushAll();
}

void
AudioLayer::flush()
{
    Manager::instance().getRingBufferPool().flushAllBuffers();
    urgentRingBuffer_.flushAll();
}

void
AudioLayer::playbackChanged(bool started)
{
    playbackStarted_ = started;
}

void
AudioLayer::recordChanged(bool started)
{
    std::lock_guard lock(audioProcessorMutex);
    if (started) {
        // create audio processor
        createAudioProcessor();
    } else {
        // destroy audio processor
        destroyAudioProcessor();
    }
    recordStarted_ = started;
}

// helper function
static inline bool
shouldUseAudioProcessorEchoCancel(bool hasNativeAEC, const std::string& echoCancellerPref)
{
    return
        // user doesn't care which and there is not a system AEC
        (echoCancellerPref == "auto" && !hasNativeAEC)
        // user specifically wants audioProcessor
        or (echoCancellerPref == "audioProcessor");
}

// helper function
static inline bool
shouldUseAudioProcessorNoiseSuppression(bool hasNativeNS, const std::string& noiseSuppressionPref)
{
    return
        // user doesn't care which and there is no system noise suppression
        (noiseSuppressionPref == "auto" && !hasNativeNS)
        // user specifically wants audioProcessor
        or (noiseSuppressionPref == "audioProcessor");
}

void
AudioLayer::setHasNativeAEC(bool hasNativeAEC)
{
    JAMI_INFO("[audiolayer] setHasNativeAEC: %d", hasNativeAEC);
    std::lock_guard lock(audioProcessorMutex);
    hasNativeAEC_ = hasNativeAEC;
    // if we have a current audio processor, tell it to enable/disable its own AEC
    if (audioProcessor) {
        audioProcessor->enableEchoCancel(
            shouldUseAudioProcessorEchoCancel(hasNativeAEC, pref_.getEchoCanceller()));
    }
}

void
AudioLayer::setHasNativeNS(bool hasNativeNS)
{
    JAMI_INFO("[audiolayer] setHasNativeNS: %d", hasNativeNS);
    std::lock_guard lock(audioProcessorMutex);
    hasNativeNS_ = hasNativeNS;
    // if we have a current audio processor, tell it to enable/disable its own noise suppression
    if (audioProcessor) {
        audioProcessor->enableNoiseSuppression(
            shouldUseAudioProcessorNoiseSuppression(hasNativeNS, pref_.getNoiseReduce()));
    }
}

// must acquire lock beforehand
void
AudioLayer::createAudioProcessor()
{
    auto nb_channels = std::max(audioFormat_.nb_channels, audioInputFormat_.nb_channels);
    auto sample_rate = std::max(audioFormat_.sample_rate, audioInputFormat_.sample_rate);

    sample_rate = std::clamp(sample_rate, 16000u, 48000u);

    AudioFormat formatForProcessor {sample_rate, nb_channels};

    unsigned int frame_size;
    if (pref_.getAudioProcessor() == "speex") {
        // TODO: maybe force this to be equivalent to 20ms? as expected by speex
        frame_size = sample_rate / 50u;
    } else {
        frame_size = sample_rate / 100u;
    }

    JAMI_WARNING("Input {}", audioInputFormat_.toString());
    JAMI_WARNING("Output {}", audioFormat_.toString());
    JAMI_WARNING("Starting audio processor with: [{} Hz, {} channels, {} samples/frame]",
              sample_rate,
              nb_channels,
              frame_size);

    if (pref_.getAudioProcessor() == "webrtc") {
#if HAVE_WEBRTC_AP
        JAMI_WARN("[audiolayer] using WebRTCAudioProcessor");
        audioProcessor.reset(new WebRTCAudioProcessor(formatForProcessor, frame_size));
#else
        JAMI_ERR("[audiolayer] audioProcessor preference is webrtc, but library not linked! "
                 "using null AudioProcessor instead");
        audioProcessor.reset();
#endif
    } else if (pref_.getAudioProcessor() == "speex") {
#if HAVE_SPEEXDSP
        JAMI_WARN("[audiolayer] using SpeexAudioProcessor");
        audioProcessor.reset(new SpeexAudioProcessor(formatForProcessor, frame_size));
#else
        JAMI_ERR("[audiolayer] audioProcessor preference is speex, but library not linked! "
                 "using null AudioProcessor instead");
        audioProcessor.reset();
#endif
    } else if (pref_.getAudioProcessor() == "null") {
        JAMI_WARN("[audiolayer] using null AudioProcessor");
        audioProcessor.reset();
    } else {
        JAMI_ERR("[audiolayer] audioProcessor preference not recognized, using null AudioProcessor "
                 "instead");
        audioProcessor.reset();
    }

    if (audioProcessor) {
        audioProcessor->enableNoiseSuppression(
            shouldUseAudioProcessorNoiseSuppression(hasNativeNS_, pref_.getNoiseReduce()));

        audioProcessor->enableAutomaticGainControl(pref_.isAGCEnabled());

        audioProcessor->enableEchoCancel(
            shouldUseAudioProcessorEchoCancel(hasNativeAEC_, pref_.getEchoCanceller()));

        audioProcessor->enableVoiceActivityDetection(pref_.getVadEnabled());
    }
}

// must acquire lock beforehand
void
AudioLayer::destroyAudioProcessor()
{
    // delete it
    audioProcessor.reset();
}

void
AudioLayer::putUrgent(std::shared_ptr<AudioFrame> buffer)
{
    urgentRingBuffer_.put(std::move(buffer));
}

// Notify (with a beep) an incoming call when there is already a call in progress
void
AudioLayer::notifyIncomingCall()
{
    if (not playIncomingCallBeep_)
        return;

    auto now = std::chrono::system_clock::now();

    // Notify maximum once every 5 seconds
    if (now < lastNotificationTime_ + std::chrono::seconds(5))
        return;

    lastNotificationTime_ = now;

    Tone tone("440/160", getSampleRate(), audioFormat_.sampleFormat);
    size_t nbSample = tone.getSize();

    /* Put the data in the urgent ring buffer */
    urgentRingBuffer_.flushAll();
    urgentRingBuffer_.put(tone.getNext(nbSample));
}

std::shared_ptr<AudioFrame>
AudioLayer::getToRing(AudioFormat format, size_t writableSamples)
{
    if (auto fileToPlay = Manager::instance().getTelephoneFile()) {
        auto fileformat = fileToPlay->getFormat();
        bool resample = format != fileformat;

        size_t readableSamples = resample ? (rational<size_t>(writableSamples, format.sample_rate)
                                             * (size_t) fileformat.sample_rate)
                                                .real<size_t>()
                                          : writableSamples;

        return resampler_->resample(fileToPlay->getNext(readableSamples, isRingtoneMuted_), format);
    }
    return {};
}

std::shared_ptr<AudioFrame>
AudioLayer::getToPlay(AudioFormat format, size_t writableSamples)
{
    notifyIncomingCall();
    auto& bufferPool = Manager::instance().getRingBufferPool();

    if (not playbackQueue_)
        playbackQueue_.reset(new AudioFrameResizer(format, writableSamples));
    else
        playbackQueue_->setFrameSize(writableSamples);

    std::shared_ptr<AudioFrame> playbackBuf {};
    while (!(playbackBuf = playbackQueue_->dequeue())) {
        std::shared_ptr<AudioFrame> resampled;

        if (auto urgentSamples = urgentRingBuffer_.get(RingBufferPool::DEFAULT_ID)) {
            bufferPool.discard(1, RingBufferPool::DEFAULT_ID);
            resampled = resampler_->resample(std::move(urgentSamples), format);
        } else if (auto toneToPlay = Manager::instance().getTelephoneTone()) {
            resampled = resampler_->resample(toneToPlay->getNext(), format);
        } else if (auto buf = bufferPool.getData(RingBufferPool::DEFAULT_ID)) {
            resampled = resampler_->resample(std::move(buf), format);
        } else {
            std::lock_guard lock(audioProcessorMutex);
            if (audioProcessor) {
                auto silence = std::make_shared<AudioFrame>(format, writableSamples);
                libav_utils::fillWithSilence(silence->pointer());
                audioProcessor->putPlayback(silence);
            }
            break;
        }

        if (resampled) {
            std::lock_guard lock(audioProcessorMutex);
            if (audioProcessor) {
                audioProcessor->putPlayback(resampled);
            }
            playbackQueue_->enqueue(std::move(resampled));
        } else
            break;
    }

    jami_tracepoint(audio_layer_get_to_play_end);

    return playbackBuf;
}

void
AudioLayer::putRecorded(std::shared_ptr<AudioFrame>&& frame)
{
    std::lock_guard lock(audioProcessorMutex);
    if (audioProcessor && playbackStarted_ && recordStarted_) {
        audioProcessor->putRecorded(std::move(frame));
        while (auto rec = audioProcessor->getProcessed()) {
            mainRingBuffer_->put(std::move(rec));
        }
    } else {
        mainRingBuffer_->put(std::move(frame));
    }

    jami_tracepoint(audio_layer_put_recorded_end, );
}

} // namespace jami
