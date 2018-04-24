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
#include "noncopyable.h"
#include "rational.h"

#include <string>
#include <vector>

class AVFilterContext;
class AVFilterGraph;

namespace ring {

struct MediaFilterParameters {
    /* Video and audio */
    int format {-1};
    rational<int> timeBase;

    /* Video */
    int width {0};
    int height {0};
    rational<int> aspectRatio;
    rational<int> frameRate;

    /* Audio */
    int sampleRate {0};
    int nbChannels {0};

    MediaFilterParameters(int fmt, rational<int> tb, int w, int h, rational<int> sar, rational<int> fr)
        : format(fmt)
        , timeBase(tb)
        , width(w)
        , height(h)
        , aspectRatio(sar)
        , frameRate(fr)
    {}

    MediaFilterParameters(int fmt, rational<int> tb, int sr, int channels)
        : format(fmt)
        , timeBase(tb)
        , sampleRate(sr)
        , nbChannels(channels)
    {}
};

class MediaFilter {
    public:
        MediaFilter();
        ~MediaFilter();

        std::string getFilterDesc() const;

        // for simple filters (1 input, 1 output)
        int initialize(const std::string filterDesc, MediaFilterParameters mfp);

        // mfps must be in the same order as they appear in filterDesc
        int initialize(const std::string filterDesc, std::vector<MediaFilterParameters> mfps);

        int feedInput(AVFrame* frame);
        AVFrame* readOutput(); // frame reference belongs to caller

    private:
        NON_COPYABLE(MediaFilter);

        int initOutputFilter(AVFilterInOut* out);
        int initInputFilter(AVFilterInOut* in, MediaFilterParameters mfp);
        int fail(std::string msg, int err);
        void clean();

        // all freed by avfilter_graph_free
        AVFilterGraph* graph_ = nullptr;
        std::vector<AVFilterContext*> inputs_;
        AVFilterContext* output_;

        std::string desc_ {};
        bool initialized_ {false};
        bool failed_ {false};
};

}; // namespace ring
