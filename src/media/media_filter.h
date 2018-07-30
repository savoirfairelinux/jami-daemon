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
#include "media_stream.h"
#include "noncopyable.h"

#include <map>
#include <string>
#include <vector>

struct AVFilterContext;
struct AVFilterGraph;
struct AVFilterInOut;

namespace ring {

/**
 * Provides access to libavfilter.
 *
 * Can be used for simple filters (1 input, 1 output), or complex filters (multiple inputs, 1 output).
 * Multiple outputs are not supported. They add complexity for little gain.
 *
 * For information on how to write a filter graph description, see:
 * https://ffmpeg.org/ffmpeg-filters.html
 * http://trac.ffmpeg.org/wiki/FilteringGuide
 *
 * For complex filters, it is required to name each filter graph input. These names are used to feed the correct input.
 * It is the same name that will be passed as second argument to feedInput(AVFrame*, std::string). This is not required
 * for simple filters, as there is only one input.
 *
 * Simple filter: "scale=320:240"
 * Scales the input to 320x240. No need to specify input names.
 *
 * Complex filter: "[in1] scale=iw/4:ih/4 [mid]; [in2] [mid] overlay=main_w-overlay_w-10:main_h-overlay_h-10"
 * in1 will be scaled to 1/16th its size and placed over in2 in the bottom right corner. When feeding frames to
 * the filter, you need to specify whether the frame is destined for in1 or in2.
 */
class MediaFilter {
    public:
        MediaFilter();
        ~MediaFilter();

        /**
         * Returns the current filter graph string.
         */
        std::string getFilterDesc() const;

        /**
         * Initializes the filter graph with 1 input.
         *
         * NOTE This method will fail if @filterDesc has more than 1 input.
         * NOTE Wraps @msp in a vector and calls initialize.
         */
        int initialize(const std::string& filterDesc, MediaStream msp);

        /**
         * Initializes the filter graph with one or more inputs and one output. Returns a negative code on error.
         */
        int initialize(const std::string& filterDesc, std::vector<MediaStream> msps);

        /**
         * Returns a MediaStream object describing the input.
         *
         * NOTE This is a shortcut for simple filters and will fail when called on a complex filter.
         */
        MediaStream getInputParams() const;

        /**
         * Returns a MediaStream object describing the input specified by @inputName.
         */
        MediaStream getInputParams(const std::string& inputName) const;

        /**
         * Returns a MediaStream struct describing the frames that will be output.
         *
         * When called in an invalid state, the returned format will be invalid (less than 0).
         */
        MediaStream getOutputParams() const;

        /**
         * Give the filter graph an input frame. Caller is responsible for freeing the frame.
         *
         * NOTE This is a wrapper for feedInput(AVFrame*, std::string)
         * NOTE This is for filters with 1 input.
         */
        int feedInput(AVFrame* frame);

        /**
         * Give the specified source filter an input frame. Caller is responsible for freeing the frame.
         *
         * NOTE Will fail if @inputName is not found in the graph.
         */
        int feedInput(AVFrame* frame, std::string inputName);

        /**
         * Pull a frame from the filter graph. Caller owns the frame reference.
         *
         * Returns AVERROR(EAGAIN) if filter graph requires more input.
         *
         * NOTE Frame reference belongs to the caller
         */
        AVFrame* readOutput();

        /**
         * Passes a frame through a simple filter (1 input, 1 output).
         *
         * This is a shortcut for feedInput(AVFrame*)+readOutput().
         *
         * NOTE Returns nullptr if the filter graph has multiple inputs/outputs.
         * NOTE Caller is responsible for freeing the input and output frames.
         */
        AVFrame* apply(AVFrame* frame);

    private:
        NON_COPYABLE(MediaFilter);

        /**
         * Initializes output of filter graph.
         */
        int initOutputFilter(AVFilterInOut* out);

        /**
         * Initializes an input of filter graph.
         */
        int initInputFilter(AVFilterInOut* in, MediaStream msp, bool simple);

        /**
         * Convenience method that prints @msg and returns err.
         *
         * NOTE @msg should not be null.
         */
        int fail(std::string msg, int err) const;

        /**
         * Frees resources used by MediaFilter.
         */
        void clean();

        /**
         * Filter graph pointer.
         */
        AVFilterGraph* graph_ = nullptr;

        /**
         * Filter graph output. Corresponds to a buffersink/abuffersink filter.
         */
        AVFilterContext* output_ = nullptr;

        /**
         * List of filter graph inputs. Each corresponds to a buffer/abuffer filter.
         */
        std::vector<AVFilterContext*> inputs_;

        /**
         * List of filter graph input parameters. Same order as @inputs_.
         */
        std::vector<MediaStream> inputParams_;

        /**
         * Filter graph string.
         */
        std::string desc_ {};

        /**
         * Flag to know whether or not the filter graph is initialized.
         */
        bool initialized_ {false};
};

}; // namespace ring
