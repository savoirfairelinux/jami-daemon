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

#include <string>
#include <vector>

class AVFilterContext;
class AVFilterGraph;

namespace ring {

class MediaFilter {
    public:
        MediaFilter();
        ~MediaFilter();

        std::string getFilterDesc() const;

        int initialize(const std::string filterDesc, AVCodecContext* c);

        int feedInput(AVFrame* frame);
        AVFrame* readOutput(); // frame reference belongs to caller

    private:
        NON_COPYABLE(MediaFilter);

        int initOutputFilter(AVFilterInOut* out);
        int initInputFilter(AVFilterInOut* in, AVCodecContext* c);
        int peekInput(AVFilterContext* fctx, AVFrame* data);
        int fail(std::string msg, int err);
        void clean();

        // freed by avfilter_graph_free
        AVFilterGraph* graph_ = nullptr;
        std::vector<AVFilterContext*> inputs_;
        AVFilterContext* output_;

        std::string desc_ {};
        bool initialized_ {false};
        bool failed_ {false};

//        ThreadLoop loop_;
//        void processFilters();
};

}; // namespace ring
