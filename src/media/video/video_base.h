/*
 *  Copyright (C) 2013-2016 Savoir-faire Linux Inc.
 *
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#pragma GCC diagnostic ignored "-Wshadow"
#include <yaml-cpp/yaml.h>
#pragma GCC diagnostic pop

#include <cstdlib>
#include <cstdint>
#include <memory>
#include <set>
#include <mutex>

class AVPacket;
class AVDictionary;

#ifndef AVFORMAT_AVIO_H
class AVIOContext;
#endif

namespace ring {
class VideoFrame;
}

namespace ring { namespace video {

enum VideoPixelFormat {
    VIDEO_PIXFMT_BGRA = -1,
    VIDEO_PIXFMT_YUV420P = -2,
    VIDEO_PIXFMT_YUYV422 = -3,
    VIDEO_PIXFMT_RGBA = -4,
    VIDEO_PIXFMT_NV21 = -5,
};

template <typename T> class Observer;
template <typename T> class Observable;

/*=== Observable =============================================================*/

template <typename T>
class Observable
{
public:
    Observable() : observers_(), mutex_() {}
    virtual ~Observable() {
        std::lock_guard<std::mutex> lk(mutex_);
        for (auto& o : observers_)
            o->detached(this);
    };

    bool attach(Observer<T>* o) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (o and observers_.insert(o).second) {
            o->attached(this);
            return true;
        }
        return false;
    }

    bool detach(Observer<T>* o) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (o and observers_.erase(o)) {
            o->detached(this);
            return true;
        }
        return false;
    }

    void notify(T data) {
        std::lock_guard<std::mutex> lk(mutex_);
        for (auto observer : observers_)
            observer->update(this, data);
    }

    int getObserversCount() {
        std::lock_guard<std::mutex> lk(mutex_);
        return observers_.size();
    }

private:
    NON_COPYABLE(Observable<T>);

    std::set<Observer<T>*> observers_;
    std::mutex mutex_; // lock observers_
};

/*=== Observer =============================================================*/

template <typename T>
class Observer
{
public:
    virtual ~Observer() {};
    virtual void update(Observable<T>*, T) = 0;
    virtual void attached(Observable<T>*) {};
    virtual void detached(Observable<T>*) {};
};

struct VideoFrameActiveWriter: Observable<std::shared_ptr<VideoFrame>> {};
struct VideoFramePassiveReader: Observer<std::shared_ptr<VideoFrame>> {};

/*=== VideoGenerator =========================================================*/

class VideoGenerator : public VideoFrameActiveWriter
{
public:
    VideoGenerator() { }

    virtual int getWidth() const = 0;
    virtual int getHeight() const = 0;
    virtual int getPixelFormat() const = 0;

    std::shared_ptr<VideoFrame> obtainLastFrame();

protected:
    // getNewFrame and publishFrame must be called by the same thread only
    VideoFrame& getNewFrame();
    void publishFrame();
    void flushFrames();

private:
    std::shared_ptr<VideoFrame> writableFrame_ = nullptr;
    std::shared_ptr<VideoFrame> lastFrame_ = nullptr;
    std::mutex mutex_ {}; // lock writableFrame_/lastFrame_ access
};

struct VideoSettings
{
    VideoSettings() {}
    VideoSettings(const std::map<std::string, std::string>& settings);

    std::map<std::string, std::string> to_map() const;

    std::string name {};
    std::string channel {};
    std::string video_size {};
    std::string framerate {};
};

}} // namespace ring::video

namespace YAML {
template<>
struct convert<ring::video::VideoSettings> {
    static Node encode(const ring::video::VideoSettings& rhs);
    static bool decode(const Node& node, ring::video::VideoSettings& rhs);
};

Emitter& operator << (Emitter& out, const ring::video::VideoSettings& v);

} // namespace YAML
