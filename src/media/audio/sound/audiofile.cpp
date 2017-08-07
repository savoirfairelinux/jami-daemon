/*  Copyright (C) 2004-2017 Savoir-faire Linux Inc.
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
#include <samplerate.h>
#include <cstring>
#include <vector>
#include <climits>
#include <algorithm>

#ifndef RING_UWP
#include <sndfile.hh>
#endif

#include "audiofile.h"
#include "audio/resampler.h"
#include "manager.h"
#include "client/ring_signal.h"
#include "libav_deps.h"

#include "logger.h"

namespace ring {

void
AudioFile::onBufferFinish()
{
    // We want to send values in milisecond
    const int divisor = buffer_->getSampleRate() / 1000;

    if (divisor == 0) {
        RING_ERR("Error cannot update playback slider, sampling rate is 0");
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
#ifndef RING_UWP
    AVFormatContext* formatCtx = avformat_alloc_context();
    int err = 0;
    if ((err = avformat_open_input(&formatCtx, fileName.c_str(), 0, 0)) < 0) {
        RING_ERR("Could not open file %s (%d)", fileName.c_str(), err);
        throw AudioFileException("Could not open file " + fileName);
    }

    if (err = avformat_find_stream_info(formatCtx, nullptr)) {
        RING_ERR("Error reading file %s (%d)", fileName.c_str(), err);
        throw AudioFileException("Could not read file " + fileName);
    }

    int idx = av_find_best_stream(formatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, 0, 0);
    if (idx == AVERROR_DECODER_NOT_FOUND || idx == AVERROR_STREAM_NOT_FOUND) {
        RING_ERR("Could not find stream in %s (%d)", fileName.c_str(), idx);
        throw AudioFileException("Could not find stream in " + fileName);
    }

    AVStream* stream = formatCtx->streams[idx];
    int nbFrames = stream->nb_frames;
    int nbChannels = stream->codecpar->channels;
    int sRate = stream->codecpar->sample_rate;
    if (!nbChannels) nbChannels = 1;
    if (!sRate) sRate = 8000;

    switch (nbChannels) {
        case 1:
        case 2:
            break;
        default:
            throw AudioFileException("Unsupported number of channels");
    }

    if (!nbFrames) {
        AVPacket pkt;
        av_init_packet(&pkt);
        int ret = 0;
        while ((ret = av_read_frame(formatCtx, &pkt)) >= 0) {
            ++nbFrames;
            av_packet_unref(&pkt);
        }
    }

    AudioSample* interleaved = new AudioSample[nbFrames * nbChannels];
    AudioBuffer* buffer = new AudioBuffer(nbFrames, AudioFormat(sRate, nbChannels));
    buffer->deinterleave(interleaved, nbFrames, nbChannels);
    delete [] interleaved;

    const int rate = static_cast<int32_t>(sRate);

    // compare sampleRate (argument) or sRate (from file)?
    if (sampleRate != rate) {
        Resampler resampler(std::max(static_cast<int32_t>(sampleRate), rate), nbChannels, true);
        AudioBuffer* resampled = new AudioBuffer(nbFrames, AudioFormat(rate, nbChannels));
        resampler.resample(*buffer, *resampled);
        delete buffer;
        delete buffer_;
        buffer_ = resampled;
    } else {
        delete buffer_;
        buffer_ = buffer;
    }

    avformat_close_input(&formatCtx);
    avformat_free_context(formatCtx);
#endif
}

} // namespace ring
