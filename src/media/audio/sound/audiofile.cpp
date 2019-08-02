/*  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
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
    // We want to send values in milisecond
    const int divisor = buffer_->getSampleRate() / 1000;

    if (divisor == 0) {
        JAMI_ERR("Error cannot update playback slider, sampling rate is 0");
        return;
    }

    if ((updatePlaybackScale_ % 5) == 0)
        emitSignal<DRing::CallSignal::UpdatePlaybackScale>(filepath_,
                                                           (unsigned)(pos_ / divisor),
                                                           (unsigned)(buffer_->frames() / divisor));

    updatePlaybackScale_++;
}

AudioFile::AudioFile(const std::string &fileName, unsigned int sampleRate) :
    AudioLoop(sampleRate), filepath_(fileName), updatePlaybackScale_(0)
{
    auto decoder = std::make_unique<MediaDecoder>();
    DeviceParams dev;
    dev.input = fileName;

    if (decoder->openInput(dev) < 0)
        throw AudioFileException("File could not be opened: " + fileName);

    if (decoder->setupFromAudioData() < 0)
        throw AudioFileException("Decoder setup failed: " + fileName);

    auto resampler = std::make_unique<Resampler>();
    const auto& format = getFormat();
    auto buf = std::make_unique<AudioBuffer>(0, format);
    bool done = false;
    while (!done) {
        AudioFrame input;
        AudioFrame output;
        auto resampled = output.pointer();
        switch (decoder->decode(input)) {
        case MediaDecoder::Status::FrameFinished:
            resampled->sample_rate = format.sample_rate;
            resampled->channel_layout = av_get_default_channel_layout(format.nb_channels);
            resampled->channels = format.nb_channels;
            resampled->format = format.sampleFormat;
            if (resampler->resample(input.pointer(), resampled) < 0)
                throw AudioFileException("Frame could not be resampled");
            if (buf->append(output) < 0)
                throw AudioFileException("Error while decoding: " + fileName);
            break;
        case MediaDecoder::Status::DecodeError:
        case MediaDecoder::Status::ReadError:
            throw AudioFileException("File cannot be decoded: " + fileName);
        case MediaDecoder::Status::EOFError:
            done = true;
            break;
        case MediaDecoder::Status::Success:
        default:
            break;
        }
    }

    delete buffer_;
    buffer_ = buf.release();
}

} // namespace jami
