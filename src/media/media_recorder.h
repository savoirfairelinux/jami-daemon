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
#include "media_buffer.h"
#include "media_encoder.h"
#include "media_filter.h"
#include "media_stream.h"
#include "noncopyable.h"
#include "observer.h"
#include "threadloop.h"
#ifdef RING_VIDEO
#include "video/video_base.h"
#endif

#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <utility>

struct AVFrame;

namespace ring {

class MediaRecorder : public Observer<std::shared_ptr<AudioFrame>>
#ifdef RING_VIDEO
                    , public video::VideoFramePassiveReader
#endif
{
public:
    MediaRecorder();
    ~MediaRecorder();

    /**
     * Gets whether or not the recorder is active.
     */
    bool isRecording() const;

    /**
     * Get file path of file to be recorded. Same path as sent to @setPath, but with
     * file extension appended.
     */
    std::string getPath() const;

    /**
     * Sets whether or not output file will have audio. Determines the extension of the
     * output file (.ogg or .webm).
     */
    void audioOnly(bool audioOnly);

    /**
     * Sets output file path.
     */
    void setPath(const std::string& path);

    /**
     * Sets title and description metadata for the file. Uses default if either is empty.
     * Default title is "Conversation at %Y-%m-%d %H:%M:%S".
     * Default description is "Recorded with Ring https://ring.cx".
     *
     * NOTE replaces %TIMESTAMP with time at start of recording
     */
    void setMetadata(const std::string& title, const std::string& desc);

    /**
     * Starts the record. Streams must have already been added using Observable::attach
     * with the recorder as parameter.
     */
    int startRecording();

    /**
     * Stops the record. Streams must be removed using Observable::detach afterwards.
     */
    void stopRecording();

    /**
     * Sets up the stream of the passed in observable.
     */
    void attached(Observable<std::shared_ptr<AudioFrame>>* ob) override;
    void attached(Observable<std::shared_ptr<VideoFrame>>* ob) override;

    /**
     * Updates the recorder with an audio or video frame.
     */
    void update(Observable<std::shared_ptr<AudioFrame>>* ob, const std::shared_ptr<AudioFrame>& a) override;
    void update(Observable<std::shared_ptr<VideoFrame>>* ob, const std::shared_ptr<VideoFrame>& v) override;

private:
    NON_COPYABLE(MediaRecorder);

    int recordData(AVFrame* frame, const MediaStream& ms);

    int addStream(const MediaStream& ms);
    int initRecord();
    MediaStream setupVideoOutput();
    std::string buildVideoFilter(const std::vector<MediaStream>& peers, const MediaStream& local) const;
    MediaStream setupAudioOutput();
    std::string buildAudioFilter(const std::vector<MediaStream>& peers, const MediaStream& local) const;
    void emptyFilterGraph();
    int sendToEncoder(AVFrame* frame, int streamIdx);
    int flush();

    std::unique_ptr<MediaEncoder> encoder_;
    std::unique_ptr<MediaFilter> videoFilter_;
    std::unique_ptr<MediaFilter> audioFilter_;

    std::mutex mutex_; // protect against concurrent file writes

    std::map<std::string, const MediaStream> streams_;

    std::tm startTime_;
    std::string title_;
    std::string description_;

    std::string path_;

    // NOTE do not use dir_ or filename_, use path_ instead
    std::string dir_;
    std::string filename_;

    bool hasAudio_ {false};
    bool hasVideo_ {false};
    int videoIdx_ = -1;
    int audioIdx_ = -1;
    bool isRecording_ = false;
    bool isReady_ = false;
    bool audioOnly_ = false;

    struct RecordFrame {
        AVFrame* frame;
        bool isVideo;
        bool fromPeer;
        RecordFrame() {}
        RecordFrame(AVFrame* f, bool v, bool p)
            : frame(f)
            , isVideo(v)
            , fromPeer(p)
        {}
    };
    InterruptedThreadLoop loop_;
    void process();
    std::mutex qLock_;
    std::deque<RecordFrame> frames_;
};

}; // namespace ring
