/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Philippe Groarke <philippe.groarke@savoirfairelinux.com>
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include "corelayer.h"
#include "manager.h"
#include "noncopyable.h"
#include "audio/ringbufferpool.h"
#include "audio/ringbuffer.h"

#include <thread>
#include <atomic>

namespace sfl {

// Actual audio thread.
class CoreAudioThread {
public:
    CoreAudioThread(CoreLayer* coreAudio);
    ~CoreAudioThread();
    void start();
private:
    NON_COPYABLE(CoreAudioThread);
    void run();
    std::thread thread_;
    CoreLayer* coreAudio_;
    std::atomic_bool running_;
};

CoreAudioThread::CoreAudioThread(CoreLayer* coreAudio)
    : thread_(), coreAudio_(coreAudio), running_(false)
{}

CoreAudioThread::~CoreAudioThread()
{
    running_ = false;
    if (thread_.joinable())
        thread_.join();
}

void CoreAudioThread::start()
{
    running_ = true;
    thread_ = std::thread(&CoreAudioThread::run, this);
}

void CoreAudioThread::run()
{
    // Actual playback is here.
    while (running_) {

    }
}


// AudioLayer implementation.
CoreLayer::CoreLayer(const AudioPreference &pref)
    : AudioLayer(pref)
    , indexIn_(pref.getAlsaCardin())
    , indexOut_(pref.getAlsaCardout())
    , indexRing_(pref.getAlsaCardring())
    , playbackBuff_(0, audioFormat_)
    , captureBuff_(0, audioFormat_)
    , playbackIBuff_(1024)
    , captureIBuff_(1024)
    , is_playback_prepared_(false)
    , is_capture_prepared_(false)
    , is_playback_running_(false)
    , is_capture_running_(false)
    , is_playback_open_(false)
    , is_capture_open_(false)
    , audioThread_(nullptr)
    , mainRingBuffer_(Manager::instance().getRingBufferPool().getRingBuffer(RingBufferPool::DEFAULT_ID))
{}

CoreLayer::~CoreLayer()
{
    isStarted_ = false;
}

std::vector<std::string> CoreLayer::getCaptureDeviceList() const
{
    return {};
}

std::vector<std::string> CoreLayer::getPlaybackDeviceList() const
{
    return {};
}

void CoreLayer::startStream()
{
    dcblocker_.reset();

    if (is_playback_running_ and is_capture_running_)
        return;
}

void CoreLayer::stopStream()
{
    isStarted_ = false;

    /* Flush the ring buffers */
    flushUrgent();
    flushMain();
}

void CoreLayer::updatePreference(AudioPreference &preference, int index, DeviceType type)
{
    switch (type) {
        case DeviceType::PLAYBACK:
            preference.setAlsaCardout(index);
            break;

        case DeviceType::CAPTURE:
            preference.setAlsaCardin(index);
            break;

        case DeviceType::RINGTONE:
            preference.setAlsaCardring(index);
            break;

        default:
            break;
    }
}

}
