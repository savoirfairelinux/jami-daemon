/*
 *  Copyright (C) 2021 Savoir-faire Linux Inc.
 *
 *  Author: Andreas Traczyk <andreas.traczyk@savoirfairelinux.com>
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

#include "noncopyable.h"
#include "audio/audio_frame_resizer.h"
#include "audio/audiobuffer.h"
#include "libav_deps.h"

namespace jami {

class EchoCanceller
{
private:
    NON_COPYABLE(EchoCanceller);

public:
    EchoCanceller(AudioFormat format, unsigned frameSize)
        : playbackQueue_(format, frameSize)
        , recordQueue_(format, frameSize)
        , sampleRate_(format.sample_rate)
        , frameSize_(frameSize)
    {}
    virtual ~EchoCanceller() = default;

    virtual void putRecorded(std::shared_ptr<AudioFrame>&& buf)
    {
        recordQueue_.enqueue(std::move(buf));
    };
    virtual void putPlayback(const std::shared_ptr<AudioFrame>& buf)
    {
        auto c = buf;
        playbackQueue_.enqueue(std::move(c));
    };
    virtual std::shared_ptr<AudioFrame> getProcessed() = 0;
    virtual void done() = 0;

protected:
    AudioFrameResizer playbackQueue_;
    AudioFrameResizer recordQueue_;
    unsigned sampleRate_;
    unsigned frameSize_;
};

} // namespace jami
