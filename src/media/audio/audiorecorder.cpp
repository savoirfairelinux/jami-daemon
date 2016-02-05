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

#include "audiorecorder.h"

#include "audiorecord.h"
#include "ringbufferpool.h"
#include "audiobuffer.h"

#include <chrono>
#include <thread>
#include <sstream>
#include <algorithm> // std::min

namespace ring {

static constexpr std::size_t BUFFER_LENGTH {10000};
static constexpr auto SLEEP_TIME = std::chrono::milliseconds(20);

AudioRecorder::AudioRecorder(AudioRecord* arec, RingBufferPool& rbp)
    : ringBufferPool_(rbp)
    , buffer_(new AudioBuffer(BUFFER_LENGTH, AudioFormat::NONE()))
    , arecord_(arec)
    , thread_(
        [this] { return true; },
        [this] { process(); },
        [] {})
{
    std::string id("processd_");

    // convert count into string
    std::string s;
    std::ostringstream out;
    out << nextProcessID();
    s = out.str();

    recorderId_ = id.append(s);
}

AudioRecorder::~AudioRecorder()
{
    thread_.join();
}

unsigned
AudioRecorder::nextProcessID() noexcept
{
    static unsigned id = 0;
    return ++id;
}

void
AudioRecorder::start()
{
    if (thread_.isRunning())
        return;
    buffer_->setFormat(ringBufferPool_.getInternalAudioFormat());
    thread_.start();
}

void
AudioRecorder::process()
{
    auto availableSamples = ringBufferPool_.availableForGet(recorderId_);
    buffer_->resize(std::min(availableSamples, BUFFER_LENGTH));
    ringBufferPool_.getData(*buffer_, recorderId_);

    if (availableSamples > 0)
        arecord_->recData(*buffer_);

    std::this_thread::sleep_for(SLEEP_TIME);
}

} // namespace ring
