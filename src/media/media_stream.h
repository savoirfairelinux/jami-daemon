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
#include "rational.h"

#include <string>

namespace ring {

struct MediaStream {
    std::string name {};
    int format {-1};
    bool isVideo {false};
    rational<int> timeBase;
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
};

}; // namespace ring
