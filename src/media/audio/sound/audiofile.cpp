/*  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *
 *  Inspired by tonegenerator of
 *   Laurielle Lea <laurielle.lea@savoirfairelinux.com> (2004)
 *  Inspired by ringbuffer of Audacity Project
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
#include <fstream>
#include <cmath>
#include <cstring>
#include <vector>
#include <climits>

#include "libav_deps.h"
#include "audiofile.h"
#include "audio/resampler.h"
#include "manager.h"
#include "media_decoder.h"
#include "client/ring_signal.h"

#include "logger.h"

namespace jami {

void
AudioFile::onBufferFinish()
{
    if (buffer_->sample_rate == 0) {
        JAMI_ERR("Error cannot update playback slider, sampling rate is 0");
        return;
    }

    // We want to send values in milisecond
    if ((updatePlaybackScale_ % 5) == 0)
        emitSignal<libjami::CallSignal::UpdatePlaybackScale>(filepath_,
                                                           (unsigned) (1000lu * pos_ / buffer_->sample_rate),
                                                           (unsigned) (1000lu * buffer_->nb_samples / buffer_->sample_rate));

    updatePlaybackScale_++;
}

AudioFile::AudioFile(const std::string& fileName, unsigned int sampleRate, AVSampleFormat sampleFormat)
    : AudioLoop(AudioFormat(sampleRate, 1, sampleFormat))
    , filepath_(fileName)
    , updatePlaybackScale_(0)
{
    std::list<std::shared_ptr<AudioFrame>> buf;
    size_t total_samples = 0;

    auto start = std::chrono::steady_clock::now();
    Resampler r {};
    auto decoder = std::make_unique<MediaDecoder>(
        [&r, this, &buf, &total_samples](const std::shared_ptr<MediaFrame>& frame) mutable {
            auto resampled = r.resample(std::static_pointer_cast<AudioFrame>(frame), format_);
            total_samples += resampled->getFrameSize();
            buf.emplace_back(std::move(resampled));
        });
    DeviceParams dev;
    dev.input = fileName;
    dev.name = fileName;

    if (decoder->openInput(dev) < 0)
        throw AudioFileException("File could not be opened: " + fileName);

    if (decoder->setupAudio() < 0)
        throw AudioFileException("Decoder setup failed: " + fileName);

    while (decoder->decode() != MediaDemuxer::Status::EndOfFile)
        ;

    buffer_->nb_samples = total_samples;
    buffer_->format = format_.sampleFormat;
    buffer_->sample_rate = format_.sample_rate;
    av_channel_layout_default(&buffer_->ch_layout, format_.nb_channels);
    av_frame_get_buffer(buffer_.get(), 0);

    size_t outPos = 0;
    for (auto& frame : buf) {
        av_samples_copy(buffer_->data, frame->pointer()->data, outPos, 0, frame->getFrameSize(), format_.nb_channels, format_.sampleFormat);
        outPos += frame->getFrameSize();
    }
    auto end = std::chrono::steady_clock::now();
    auto audioDuration = std::chrono::duration<double>(total_samples/(double)format_.sample_rate);
    JAMI_LOG("AudioFile: loaded {} samples ({}) as {} in {} from {:s}",
        total_samples, audioDuration, format_.toString(), dht::print_duration(end-start), fileName);
}

} // namespace jami
