/*
 *  Copyright (C) 2007-2019 Savoir-faire Linux Inc.
 *
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Adrien Beraud <adrien.beraud@gmail.com>
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

#include "audiobuffer.h"
#include "noncopyable.h"
#include "audio_frame_resizer.h"
#include "resampler.h"

#include <condition_variable>
#include <mutex>
#include <chrono>
#include <map>
#include <vector>
#include <fstream>

namespace ring {

/**
 * A ring buffer for mutichannel audio samples
 */
class RingBuffer {
public:
    using clock = std::chrono::high_resolution_clock;
    using time_point = clock::time_point;
    using FrameCallback = std::function<void(const std::shared_ptr<AudioFrame>&)>;

    /**
     * Constructor
     * @param size  Size of the buffer to create
     */
    RingBuffer(const std::string& id, size_t size,
                AudioFormat format=AudioFormat::MONO());

    const std::string& getId() const { return id; }

    /**
     * Reset the counters to 0 for this read offset
     */
    void flush(const std::string &call_id);

    void flushAll();

    inline AudioFormat getFormat() const {
        return format_;
    }

    inline void setFormat(const AudioFormat& format) {
        format_ = format;
        resizer_.setFormat(format, format.sample_rate / 50);
    }

    /**
     * Add a new readoffset for this ringbuffer
     */
    void createReadOffset(const std::string &call_id);

    void createReadOffset(const std::string &call_id, FrameCallback cb);

    /**
     * Remove a readoffset for this ringbuffer
     */
    void removeReadOffset(const std::string &call_id);

    size_t readOffsetCount() const { return readoffsets_.size(); }

    /**
     * Write data in the ring buffer
     * @param buffer Data to copied
     * @param toCopy Number of bytes to copy
     */
        void put(std::shared_ptr<AudioFrame>&& data);

    /**
     * To get how much samples are available in the buffer to read in
     * @return int The available (multichannel) samples number
     */
    size_t availableForGet(const std::string &call_id) const;

    /**
     * Get data in the ring buffer
     * @param buffer Data to copied
     * @param toCopy Number of bytes to copy
     * @return size_t Number of bytes copied
     */
    std::shared_ptr<AudioFrame> get(const std::string &call_id);

    /**
     * Discard data from the buffer
     * @param toDiscard Number of samples to discard
     * @return size_t Number of samples discarded
     */
    size_t discard(size_t toDiscard, const std::string &call_id);

    /**
     * Total length of the ring buffer which is available for "putting"
     * @return int
     */
    size_t putLength() const;

    size_t getLength(const std::string &call_id) const;

    inline bool isFull() const {
        return putLength() == buffer_.size();
    }

    inline bool isEmpty() const {
        return putLength() == 0;
    }

    /**
     * Blocks until min_data_length samples of data is available, or until deadline has passed.
     *
     * @param call_id The read offset for which data should be available.
     * @param min_data_length Minimum number of samples that should be available for the call to return
     * @param deadline The call is guaranteed to end after this time point. If no deadline is provided, the call blocks indefinitely.
     * @return available data for call_id after the call returned (same as calling getLength(call_id) ).
     */
    size_t waitForDataAvailable(const std::string& call_id, const time_point& deadline = time_point::max()) const;

    /**
     * Debug function print mEnd, mStart, mBufferSize
     */
    void debug();

private:
    struct ReadOffset {
        size_t offset;
        FrameCallback callback;
    };
    using ReadOffsetMap = std::map<std::string, ReadOffset>;
    NON_COPYABLE(RingBuffer);

    void putToBuffer(std::shared_ptr<AudioFrame>&& data);

    bool hasNoReadOffsets() const;

    /**
     * Return the smalest readoffset. Useful to evaluate if ringbuffer is full
     */
    size_t getSmallestReadOffset() const;

    /**
     * Get read offset coresponding to this call
     */
    size_t getReadOffset(const std::string &call_id) const;

    /**
     * Move readoffset forward by offset
     */
    void storeReadOffset(size_t offset, const std::string &call_id);

    /**
     * Test if readoffset coresponding to this call is still active
     */
    bool hasThisReadOffset(const std::string &call_id) const;

    /**
     * Discard data from all read offsets to make place for new data.
     */
    size_t discard(size_t toDiscard);

    const std::string id;

    /** Offset on the last data */
    size_t endPos_;

    /** Data */
    AudioFormat format_ {AudioFormat::DEFAULT()};
    std::vector<std::shared_ptr<AudioFrame>> buffer_ {16};

    mutable std::mutex lock_;
    mutable std::condition_variable not_empty_;

    ReadOffsetMap readoffsets_;

    Resampler resampler_;
    AudioFrameResizer resizer_;

    double rmsLevel_ {0};
    int rmsFrameCount_ {0};
};

} // namespace ring
