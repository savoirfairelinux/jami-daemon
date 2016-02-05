/*
 *  Copyright (C) 2004-2016 Savoir-faire Linux Inc.
 *
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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

#include "threadloop.h"
#include "noncopyable.h"

#include <string>
#include <memory>

namespace ring {

class RingBufferPool;
class AudioRecord;
class AudioBuffer;

class AudioRecorder {
public:
    AudioRecorder(AudioRecord* arec, RingBufferPool& rbp);
    ~AudioRecorder();

    std::string getRecorderID() const {
        return recorderId_;
    }

    /**
     * Call to start recording.
     */
    void start();

private:
    NON_COPYABLE(AudioRecorder);
    static unsigned nextProcessID() noexcept;
    void process();

    std::string recorderId_;
    RingBufferPool& ringBufferPool_;
    std::unique_ptr<AudioBuffer> buffer_;
    AudioRecord* arecord_;
    ThreadLoop thread_;
};

} // namespace ring
