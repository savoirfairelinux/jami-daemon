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
#include "noncopyable.h"

#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>

class AVFormatContext;
class AVStream;
struct AVPacket;
struct AVRational;

namespace ring {

class MediaRecorder {
    public:
        MediaRecorder();
        ~MediaRecorder();

        void initFilename(const std::string &peerNumber);

        std::string getFilename() const;

        bool fileExists() const;

        void setRecordingPath(const std::string& dir);

        bool isRecording() const;

        bool toggleRecording();

        int startRecording();

        void stopRecording();

        int copyStream(AVStream* input, AVPacket* packet, bool fromPeer, bool isVideo);

        int recordData(AVPacket* packet, bool fromPeer, bool isVideo);

    private:
        NON_COPYABLE(MediaRecorder);

        int recordData(AVPacket* packet, int streamIdx);
        int flush();

        AVFormatContext* outputCtx_ = nullptr;

        std::string filename_;
        std::string dir_;
        bool isRecording_ = false;
        bool wroteHeader_ = false;
        std::mutex mutex_;

        std::map<std::pair<bool, bool>, int> streamMap_; // map fromPeer and isVideo to a stream index
        std::map<int, AVRational> timebaseMap_; // not needed? (outStream->time_base = inStream->time_base)
};

}; // namespace ring
