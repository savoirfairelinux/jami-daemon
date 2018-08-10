/*
 *  Copyright (C) 2018 Savoir-faire Linux Inc.
 *
 *  Author: Hugo Lefeuvre <hugo.lefeuvre@savoirfairelinux.com>
 *  Author: Philippe Gorley <philippe.gorley@savoirfairelinux.com>
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


#include "audio_input.h"
#include "media_encoder.h"

namespace ring { namespace audio {

AudioInput::AudioInput(bool muteState) :
    muteState_(muteState),
    loop_([&] { return setup(socketPair); },
          std::bind(&AudioInput::process, this),
          std::bind(&AudioInput::cleanup, this))
{
    loop_.start();
}

AudioInput::~AudioInput()
{
    if (auto rec = recorder_.lock())
        rec->stopRecording();
    loop_.join();
}

void
AudioInput::process()
{
    auto& mainBuffer = Manager::instance().getRingBufferPool();
    auto mainBuffFormat = mainBuffer.getInternalAudioFormat();

    // compute nb of byte to get corresponding to 1 audio frame
    const std::size_t samplesToGet = std::chrono::duration_cast<std::chrono::seconds>(mainBuffFormat.sample_rate * secondsPerPacket_).count();

    if (mainBuffer.availableForGet(id_) < samplesToGet) {
        const auto wait_time = std::chrono::duration_cast<std::chrono::milliseconds>(secondsPerPacket_);
        if (not mainBuffer.waitForDataAvailable(id_, samplesToGet, wait_time))
            return;
    }

    // get data
    micData_.setFormat(mainBuffFormat);
    micData_.resize(samplesToGet);
    const auto samples = mainBuffer.getData(micData_, id_);
    if (samples != samplesToGet)
        return;

    // down/upmix as needed
    auto accountAudioCodec = std::static_pointer_cast<AccountAudioCodecInfo>(args_.codec);
    micData_.setChannelNum(accountAudioCodec->audioformat.nb_channels, true);

    Smartools::getInstance().setLocalAudioCodec(audioEncoder_->getEncoderName());

    AudioBuffer buffer;
    if (mainBuffFormat.sample_rate != accountAudioCodec->audioformat.sample_rate) {
        if (not resampler_) {
            RING_DBG("Creating audio resampler");
            resampler_.reset(new Resampler);
        }
        resampledData_.setFormat(accountAudioCodec->audioformat);
        resampledData_.resize(samplesToGet);
        resampler_->resample(micData_, resampledData_);
        buffer = resampledData_;
    } else {
        buffer = micData_;
    }

    if (muteState_) // audio is muted, set samples to 0
        buffer.reset();

    AVFrame* frame = buffer.toAVFrame();
    auto ms = MediaStream("audio", buffer.getFormat());
    frame->pts = getNextTimestamp(sent_samples, ms.sampleRate, static_cast<rational<int64_t>>(ms.timeBase));
    sent_samples += frame->nb_samples;

    {
        auto rec = recorder_.lock();
        if (rec && !recordingStarted_ && rec->addStream(false, false, ms) >= 0) {
                recordingStarted_ = true;
        }

        if (rec && recordingStarted_) {
            rec->recordData(frame, false, false);
        } else {
            recordingStarted_ = false;
            recorder_ = std::weak_ptr<MediaRecorder>();
        }
    }
}

void
AudioInput::cleanup()
{
    micData_.clear();
    resampledData_.clear();
}

void
AudioSender::setMuted(bool isMuted)
{
    muteState_ = isMuted;
    audioEncoder_->setMuted(isMuted);
}

bool
AudioInput::isCapturing() const noexcept
{
    return loop_.isRunning();
}

bool AudioInput::captureFrame()
{
    // TODO
}

std::shared_future<DeviceParams>
AudioInput::switchInput(const std::string& resource)
{
    // TODO
}

void
AudioInput::initRecorder(const std::shared_ptr<MediaRecorder>& rec)
{
    rec->incrementStreams(1);
    recorder_ = rec;
    if (auto r = recorder_.lock()) {
        r->incrementStreams(1);
    }
}

}} // namespace ring::audio
