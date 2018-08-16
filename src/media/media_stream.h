/*
 *  Copyright (C) 2018 Savoir-faire Linux Inc.
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

#include "config.h"
#include "audio/audiobuffer.h"
#include "libav_deps.h"
#include "rational.h"

#include <string>

namespace ring {

struct MediaStream {
    std::string name {};
    int format {-1};
    bool isVideo {false};
    rational<int> timeBase;
    int64_t firstTimestamp {0};
    int width {0};
    int height {0};
    rational<int> aspectRatio;
    rational<int> frameRate;
    int sampleRate {0};
    int nbChannels {0};

    MediaStream()
    {}

    MediaStream(std::string name, int fmt, rational<int> tb, int w, int h,
                          rational<int> sar, rational<int> fr)
        : name(name)
        , format(fmt)
        , isVideo(true)
        , timeBase(tb)
        , width(w)
        , height(h)
        , aspectRatio(sar)
        , frameRate(fr)
    {}

    MediaStream(std::string name, int fmt, rational<int> tb, int sr, int channels)
        : name(name)
        , format(fmt)
        , isVideo(false)
        , timeBase(tb)
        , sampleRate(sr)
        , nbChannels(channels)
    {}

    MediaStream(std::string name, AudioFormat fmt)
        : name(name)
        , format(fmt.sampleFormat)
        , isVideo(false)
        , timeBase(1, fmt.sample_rate)
        , sampleRate(fmt.sample_rate)
        , nbChannels(fmt.nb_channels)
    {}

    MediaStream(std::string name, AVCodecContext* c)
        : MediaStream(name, c, 0)
    {}

    MediaStream(std::string name, AVCodecContext* c, int64_t firstTimestamp)
        : name(name)
    {
        timeBase = c->time_base;
        this->firstTimestamp = firstTimestamp;
        switch (c->codec_type) {
        case AVMEDIA_TYPE_VIDEO:
            format = c->pix_fmt;
            isVideo = true;
            width = c->width;
            height = c->height;
            aspectRatio = c->sample_aspect_ratio;
            frameRate = c->framerate;
            break;
        case AVMEDIA_TYPE_AUDIO:
            format = c->sample_fmt;
            isVideo = false;
            sampleRate = c->sample_rate;
            nbChannels = c->channels;
            break;
        default:
            break;
        }
    }

    MediaStream(const MediaStream& other)
        : name(other.name)
        , format(other.format)
        , isVideo(other.isVideo)
        , timeBase(other.timeBase)
        , firstTimestamp(other.firstTimestamp)
        , width(other.width)
        , height(other.height)
        , aspectRatio(other.aspectRatio)
        , frameRate(other.frameRate)
        , sampleRate(other.sampleRate)
        , nbChannels(other.nbChannels)
    {}
};

inline std::ostream& operator<<(std::ostream& os, const MediaStream& ms)
{
    if (ms.isVideo) {
        os << (ms.name.empty() ? "(null)" : ms.name) << ": "
            << av_get_pix_fmt_name(static_cast<AVPixelFormat>(ms.format)) << " video, "
            << ms.width << "x" << ms.height << ", "
            << ms.frameRate << " fps (" << ms.timeBase << ")";
    } else {
        os << (ms.name.empty() ? "(null)" : ms.name) << ": "
            << av_get_sample_fmt_name(static_cast<AVSampleFormat>(ms.format)) << " audio, "
            << ms.nbChannels << " channel(s), "
            << ms.sampleRate << " Hz (" << ms.timeBase << ")";
    }
    if (ms.firstTimestamp > 0)
        os << ", start: " << ms.firstTimestamp;
    return os;
}

}; // namespace ring
