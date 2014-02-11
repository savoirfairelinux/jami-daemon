/*
 *  Copyright (C) 2013 Savoir-Faire Linux Inc.
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA  02110-1301 USA.
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

#ifndef __VIDEO_FRAME_H__
#define __VIDEO_FRAME_H__

#include "noncopyable.h"

#include <cstdlib>
#include <cstdint>
#include <memory>
#include <set>
#include <mutex>

// std::this_thread::sleep_for is by default supported since 4.8.1
#define GCC_VERSION (__GNUC__ * 10000 \
                     + __GNUC_MINOR__ * 100             \
                     + __GNUC_PATCHLEVEL__)
#if GCC_VERSION >= 40801
#include <chrono>
#include <thread>
#define MYSLEEP(x) std::this_thread::sleep_for(std::chrono::seconds(x))
#else
#include <unistd.h>
#define MYSLEEP(x) sleep(x)
#endif

class AVFrame;
class AVPacket;
class AVDictionary;

#ifndef AVFORMAT_AVIO_H
class AVIOContext;
#endif

enum VideoPixelFormat {
    VIDEO_PIXFMT_BGRA = -1,
    VIDEO_PIXFMT_YUV420P = -2,
};

/*=== API  ===================================================================*/

template <typename T> class ActiveWriter;
template <typename T> class PassiveWriter;
template <typename T> class ActiveReader;
template <typename T> class PassiveReader;

template <typename T> struct ActiveWriterI;
template <typename T> struct PassiveWriterI;
template <typename T> struct ActiveReaderI;
template <typename T> struct PassiveReaderI;

template <typename T>
struct PassiveReaderI
{
	virtual ~PassiveReaderI() {};
    virtual void attached(ActiveWriter<T>&) = 0;
    virtual void detached(ActiveWriter<T>&) = 0;

    // must be implemented by subclass
    virtual void update(ActiveWriter<T>&, T&) = 0;
};

template <typename T>
struct ActiveWriterI
{
	virtual ~ActiveWriterI() {};
	virtual bool attach_reader(PassiveReader<T>&) = 0;
	virtual bool detach_reader(PassiveReader<T>&) = 0;
	virtual void push(T& data) = 0;
	virtual int getReadersCount() = 0;
};

template <typename T>
struct PassiveWriterI
{
	virtual ~PassiveWriterI() {};
    virtual void attached(ActiveReader<T>&) = 0;
    virtual void detached(ActiveReader<T>&) = 0;

    // must be implemented by subclass
    virtual void request(ActiveReader<T>&, T&) = 0;
};

template <typename T>
struct ActiveReaderI
{
	virtual ~ActiveReaderI() {};
	virtual void attach_writer(PassiveWriter<T>&) = 0;
	virtual void detach_writer() = 0;
	virtual void pull(T& data) = 0;
};

/*=== PassiveReader ==========================================================*/

template <typename T>
class PassiveReader : public PassiveReaderI<T>
{
public:
    virtual void attached(ActiveWriter<T>&) {}
    virtual void detached(ActiveWriter<T>&) {}
};

/*=== ActiveWriter ===========================================================*/

template <typename T>
class ActiveWriter : public ActiveWriterI<T>
{
public:
    ActiveWriter() : readers_(), mutex_() {}
    ~ActiveWriter() {
        std::unique_lock<std::mutex> lk(mutex_);
        for (const auto reader : readers_)
            reader->detached(*this);
    }

    bool attach_reader(PassiveReader<T>& reader) {
        std::unique_lock<std::mutex> lk(mutex_);
        if (readers_.insert(&reader).second) {
            reader.attached(*this);
            return true;
        }
        return false;
    }

    bool detach_reader(PassiveReader<T>& reader) {
        std::unique_lock<std::mutex> lk(mutex_);
        if (readers_.erase(&reader)) {
            reader.detached(*this);
            return true;
        }
        return false;
    }

    void push(T& data) {
        std::unique_lock<std::mutex> lk(mutex_);
        for (const auto reader : readers_)
            reader->update(*this, data);
    }

    int getReadersCount() {
        std::unique_lock<std::mutex> lk(mutex_);
        return readers_.size();
    }

private:
    NON_COPYABLE(ActiveWriter<T>);
	std::set<PassiveReader<T>* > readers_;
    std::mutex mutex_; // to lock readers_
};

/*=== PassiveWriter ==========================================================*/

template <typename T>
class PassiveWriter : public PassiveWriterI<T>
{
public:
    virtual void attached(ActiveReader<T>&) {};
    virtual void detached(ActiveReader<T>&) {};
};

/*=== NullPassiveWriter ======================================================*/

// A PassiveWriter that never feeds data
template <typename T>
class NullPassiveWriter : public PassiveWriter<T>
{
public:
    static NullPassiveWriter<T>* get_instance() {
		if (!instance_)
			instance_ = new NullPassiveWriter<T>();
        return instance_;
    }
    void request(ActiveReader<T>&, T&) {};

private:
    static NullPassiveWriter<T>* instance_;
};

template <typename T> NullPassiveWriter<T>* NullPassiveWriter<T>::instance_ = nullptr;

/*=== ActiveReader ===========================================================*/

template <typename T>
class ActiveReader : public ActiveReaderI<T>
{
public:
    ActiveReader() : writer_(NullPassiveWriter<T>::get_instance()), mutex_() {};
    virtual ~ActiveReader() {};
    void attach_writer(PassiveWriter<T>& writer) {
		std::unique_lock<std::mutex> lk(mutex_);
		writer_ = &writer;
		writer_->attached(*this);
	}
    void detach_writer() {
		std::unique_lock<std::mutex> lk(mutex_);
		writer_->detached(*this);
		writer_ = NullPassiveWriter<T>::get_instance();
	}
    void pull(T& data) {
		std::unique_lock<std::mutex> lk(mutex_);
		writer_->request(*this, data);
	}

private:
	NON_COPYABLE(ActiveReader<T>);

    // Set by default and at detach call to a NullPassiveWriter instance,
    // so the pull method can be safely called at anytime.
    PassiveWriter<T>* writer_;
	std::mutex mutex_; // to lock writer_
};

namespace sfl_video {

class VideoFrame;

typedef int(*io_readcallback)(void *opaque, uint8_t *buf, int buf_size);
typedef int(*io_writecallback)(void *opaque, uint8_t *buf, int buf_size);
typedef int64_t(*io_seekcallback)(void *opaque, int64_t offset, int whence);

/*=== VideoPacket  ===========================================================*/

class VideoPacket {

public:
    VideoPacket();
    ~VideoPacket();
    AVPacket* get() { return packet_; };

private:
    NON_COPYABLE(VideoPacket);
    AVPacket *packet_;
};

/*=== VideoIOHandle  =========================================================*/

class VideoIOHandle {
public:
    VideoIOHandle(ssize_t buffer_size,
                  bool writeable,
                  io_readcallback read_cb,
                  io_writecallback write_cb,
                  io_seekcallback seek_cb,
                  void *opaque);
    ~VideoIOHandle();

    AVIOContext* getContext() { return ctx_; }

private:
    NON_COPYABLE(VideoIOHandle);
    AVIOContext *ctx_;
    unsigned char *buf_;
};

class VideoCodec {
public:
    VideoCodec();
    virtual ~VideoCodec() {}

    void setOption(const char *name, const char *value);

private:
    NON_COPYABLE(VideoCodec);

protected:
    AVDictionary *options_;
};

/*=== VideoFrame =============================================================*/

class VideoFrame {
public:
    VideoFrame();
    ~VideoFrame();

    AVFrame* get() { return frame_; };
    int getPixelFormat() const;
    int getWidth() const;
    int getHeight() const;
    void setGeometry(int width, int height, int pix_fmt);
    void setDestination(void *data);
    size_t getSize();
    static size_t getSize(int width, int height, int format);
    void setdefaults();
    bool allocBuffer(int width, int height, int pix_fmt);
    // YUV_420P only!
    int blit(VideoFrame& src, int xoff, int yoff);
    void copy(VideoFrame &src);
    void clear();
    int mirror();
    void test();

private:
    NON_COPYABLE(VideoFrame);
    AVFrame *frame_;
    bool allocated_;
};
typedef std::shared_ptr<VideoFrame> VideoFrameShrPtr;

/*=== Video related Reader/Writer classes ====================================*/

/* For push model */
typedef ActiveWriter<VideoFrameShrPtr> VideoFrameActiveWriter;
typedef PassiveReader<VideoFrameShrPtr> VideoFramePassiveReader;

/* For pull model */
typedef ActiveReader<VideoFrameShrPtr> VideoFrameActiveReader;
typedef PassiveWriter<VideoFrameShrPtr> VideoFramePassiveWriter;

/*=== VideoGenerator =========================================================*/

class VideoGenerator : public VideoFrameActiveWriter
{
public:
    VideoGenerator() : writableFrame_(), lastFrame_(), mutex_() {}

    virtual int getWidth() const = 0;
    virtual int getHeight() const = 0;
    virtual int getPixelFormat() const = 0;

    VideoFrameShrPtr obtainLastFrame();

protected:
    // getNewFrame and publishFrame must be called by the same thread only
    VideoFrame& getNewFrame();
    void publishFrame();

private:
    VideoFrameShrPtr writableFrame_;
    VideoFrameShrPtr lastFrame_;
    std::mutex mutex_; // lock writableFrame_/lastFrame_ access
};

}

#endif // __VIDEO_BASE_H__
