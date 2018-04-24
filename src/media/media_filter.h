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

class AVFilterContext;
class AVFilterGraph;
class AVFilterInOut;

namespace ring {

class MediaFilter {
    public:
        MediaFilter();
        ~MediaFilter();

        std::string getFilterChain() const;

        int initializeFilters(AVCodecContext* codecCtx, const std::string filterChain);

        // frees and replaces frame with a filtered frame
        int applyFilters(AVFrame* frame);

    private:
        NON_COPYABLE(MediaFilter);

        void clean();

        AVFilterContext* srcCtx_ = nullptr;
        AVFilterContext* sinkCtx_ = nullptr;
        AVFilterGraph* graph_ = nullptr;
        AVFilterInOut* outputs_ = nullptr;
        AVFilterInOut* inputs_ = nullptr;

        std::string filterChain_ {};
        bool isVideo_ {false};
};

}; // namespace ring
