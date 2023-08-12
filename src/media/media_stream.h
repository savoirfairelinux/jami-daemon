/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
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

#pragma once

#include "libav_deps.h"
#include "logger.h"
#include "rational.h"
#include "audio/audio_format.h"

#include <string>

namespace jami {

struct MediaStream
{
    std::string name {};
    int format {-1};
    bool isVideo {false};
    rational<int> timeBase;
    int64_t firstTimestamp {0};
    int width {0};
    int height {0};
    int bitrate {0};
    rational<int> frameRate;
    int sampleRate {0};
    int nbChannels {0};
    int frameSize {0};

    MediaStream() {}

    MediaStream(const std::string& streamName,
                int fmt,
                rational<int> tb,
                int w,
                int h,
                int br,
                rational<int> fr)
        : name(streamName)
        , format(fmt)
        , isVideo(true)
        , timeBase(tb)
        , width(w)
        , height(h)
        , bitrate(br)
        , frameRate(fr)
    {}

    MediaStream(
        const std::string& streamName, int fmt, rational<int> tb, int sr, int channels, int size)
        : name(streamName)
        , format(fmt)
        , isVideo(false)
        , timeBase(tb)
        , sampleRate(sr)
        , nbChannels(channels)
        , frameSize(size)
    {}

    MediaStream(const std::string& streamName, AudioFormat fmt)
        : MediaStream(streamName, fmt, 0)
    {}

    MediaStream(const std::string& streamName, AudioFormat fmt, int64_t startTimestamp)
        : name(streamName)
        , format(fmt.sampleFormat)
        , isVideo(false)
        , timeBase(1, fmt.sample_rate)
        , firstTimestamp(startTimestamp)
        , sampleRate(fmt.sample_rate)
        , nbChannels(fmt.nb_channels)
        , frameSize(fmt.sample_rate / 50) // standard frame size for our encoder is 20 ms
    {}

    MediaStream(const std::string& streamName, AVCodecContext* c)
        : MediaStream(streamName, c, 0)
    {}

    MediaStream(const std::string& streamName, AVCodecContext* c, int64_t startTimestamp)
        : name(streamName)
        , firstTimestamp(startTimestamp)
    {
        if (c) {
            timeBase = c->time_base;
            switch (c->codec_type) {
            case AVMEDIA_TYPE_VIDEO:
                format = c->pix_fmt;
                isVideo = true;
                width = c->width;
                height = c->height;
                bitrate = c->bit_rate;
                frameRate = c->framerate;
                break;
            case AVMEDIA_TYPE_AUDIO:
                format = c->sample_fmt;
                isVideo = false;
                sampleRate = c->sample_rate;
                nbChannels = c->ch_layout.nb_channels;
                frameSize = c->frame_size;
                break;
            default:
                break;
            }
        } else {
            JAMI_WARN() << "Trying to get stream info from null codec context";
        }
    }

    MediaStream(const MediaStream& other) = default;

    bool isValid() const
    {
        if (format < 0)
            return false;
        if (isVideo)
            return width > 0 && height > 0;
        else
            return sampleRate > 0 && nbChannels > 0;
    }

    void update(AVFrame* f)
    {
        // update all info possible (AVFrame has no fps or bitrate data)
        format = f->format;
        if (isVideo) {
            width = f->width;
            height = f->height;
        } else {
            sampleRate = f->sample_rate;
            nbChannels = f->ch_layout.nb_channels;
            timeBase = rational<int>(1, f->sample_rate);
            if (!frameSize)
                frameSize = f->nb_samples;
        }
    }
};

inline std::ostream&
operator<<(std::ostream& os, const MediaStream& ms)
{
    if (ms.isVideo) {
        auto formatName = av_get_pix_fmt_name(static_cast<AVPixelFormat>(ms.format));
        os << (ms.name.empty() ? "(null)" : ms.name) << ": "
           << (formatName ? formatName : "(unknown format)") << " video, " << ms.width << "x"
           << ms.height << ", " << ms.frameRate << " fps (" << ms.timeBase << ")";
        if (ms.bitrate > 0)
            os << ", " << ms.bitrate << " kb/s";
    } else {
        os << (ms.name.empty() ? "(null)" : ms.name) << ": "
           << av_get_sample_fmt_name(static_cast<AVSampleFormat>(ms.format)) << " audio, "
           << ms.nbChannels << " channel(s), " << ms.sampleRate << " Hz (" << ms.timeBase << "), "
           << ms.frameSize << " samples per frame";
    }
    if (ms.firstTimestamp > 0)
        os << ", start: " << ms.firstTimestamp;
    return os;
}

}; // namespace jami
