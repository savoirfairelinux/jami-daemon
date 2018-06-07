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
#include "media_encoder.h"
#include "media_filter.h"
#include "media_stream.h"
#include "noncopyable.h"
#include "threadloop.h"

#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <utility>

struct AVFrame;

namespace ring {

class MediaRecorder {
    public:
        MediaRecorder();
        ~MediaRecorder();

        std::string getPath() const;

        void setPath(const std::string& path);

        void audioOnly(bool audioOnly);

        // replaces %TIMESTAMP with time at start of recording
        // default title: "Conversation at %Y-%m-%d %H:%M:%S"
        // default description: "Recorded with Ring https://ring.cx"
        void setMetadata(const std::string& title, const std::string& desc);

        [[deprecated("use setPath to set full recording path")]]
        void setRecordingPath(const std::string& dir);

        // adjust nb of streams before recording
        // used to know when all streams are set up
        void incrementStreams(int n);

        bool isRecording() const;

        bool toggleRecording();

        int startRecording();

        void stopRecording();

        int addStream(bool isVideo, bool fromPeer, MediaStream ms);

        int recordData(AVFrame* frame, bool isVideo, bool fromPeer);

    private:
        NON_COPYABLE(MediaRecorder);

        int initRecord();
        MediaStream setupVideoOutput();
        std::string buildVideoFilter();
        MediaStream setupAudioOutput();
        void emptyFilterGraph();
        int sendToEncoder(AVFrame* frame, int streamIdx);
        int flush();

        std::unique_ptr<MediaEncoder> encoder_;
        std::unique_ptr<MediaFilter> videoFilter_;
        std::unique_ptr<MediaFilter> audioFilter_;

        std::mutex mutex_; // protect against concurrent file writes

        // isVideo is first key, fromPeer is second
        std::map<bool, std::map<bool, MediaStream>> streams_;

        std::tm startTime_;
        std::string title_;
        std::string description_;

        std::string path_;

        [[deprecated]]
        std::string dir_;
        [[deprecated]]
        std::string filename_;

        unsigned nbExpectedStreams_ = 0;
        unsigned nbReceivedVideoStreams_ = 0;
        unsigned nbReceivedAudioStreams_ = 0;
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
        std::queue<RecordFrame> frames_;
};

}; // namespace ring
