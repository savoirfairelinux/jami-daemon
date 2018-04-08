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

#include "libav_deps.h"
#include "fileutils.h"
#include "logger.h"
#include "media_recorder.h"

#include <algorithm>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

namespace ring {

static std::string
createTimestamp()
{
    time_t rawtime = time(NULL);
    struct tm * timeinfo = localtime(&rawtime);
    std::stringstream out;

    // DATE
    out << timeinfo->tm_year + 1900;
    if (timeinfo->tm_mon < 9) // prefix jan-sep with 0
        out << 0;
    out << timeinfo->tm_mon + 1; // tm_mon is 0 based
    if (timeinfo->tm_mday < 10) // make sure there's 2 digits
        out << 0;
    out << timeinfo->tm_mday;

    out << '-';

    // HOUR
    if (timeinfo->tm_hour < 10) // make sure there's 2 digits
        out << 0;
    out << timeinfo->tm_hour;
    if (timeinfo->tm_min < 10) // make sure there's 2 digits
        out << 0;
    out << timeinfo->tm_min;
    if (timeinfo->tm_sec < 10) // make sure there's 2 digits
        out << 0;
    out << timeinfo->tm_sec;

    return out.str();
}

static std::string
sanitize(std::string s)
{
    // replace unprintable characters with '_'
    std::replace_if(s.begin(), s.end(),
                    [](char c){ return !(std::isalnum(c) || c == '_' || c == '.'); },
                    '_');
    return s;
}

MediaRecorder::MediaRecorder()
{
    setRecordingPath(fileutils::get_home_dir());
    initFilename("");
}

MediaRecorder::~MediaRecorder()
{
    flush();
    avformat_free_context(outputCtx_);
}

void
MediaRecorder::initFilename(const std::string &peerNumber)
{
    filename_ = createTimestamp();
    if (!peerNumber.empty()) {
        RING_DBG() << "Initialize recording for peer: " << peerNumber;
        filename_.append("-" + sanitize(peerNumber));
    }
    filename_.append(".mkv");
}

std::string
MediaRecorder::getFilename() const
{
    return dir_ + filename_;
}

bool
MediaRecorder::fileExists() const
{
    return fileutils::isFile(getFilename());
}

void
MediaRecorder::setRecordingPath(const std::string& dir)
{
    if (!dir.empty() || fileutils::isDirectory(dir)) {
        dir_ = dir;
        RING_DBG() << "Recording will be saved in '" << dir_ << "'";
    } else {
        dir_ = fileutils::get_home_dir();
        RING_WARN() << "Invalid directory '" << dir << "': defaulting to home folder '" << dir_ << "'";
    }
    if (dir_.back() != DIR_SEPARATOR_CH)
        dir_ = dir_ + DIR_SEPARATOR_CH;
}

bool
MediaRecorder::isRecording() const
{
    return isRecording_;
}

bool
MediaRecorder::toggleRecording()
{
    if (isRecording_)
        stopRecording();
    else
        startRecording();
    return isRecording_;
}

void
MediaRecorder::startRecording()
{
    RING_DBG() << "Start recording '" << filename_ << "'";
    isRecording_ = true;

    avformat_alloc_output_context2(&outputCtx_, NULL, NULL, getFilename().c_str());
    // allocate streams
}

void
MediaRecorder::stopRecording()
{
    RING_DBG() << "Stop recording '" << filename_ << "'";
    flush();
    isRecording_ = false;
}

void
MediaRecorder::recordData(AVPacket* packet, bool fromPeer, bool isVideo)
{
    // TODO find which AVStream we're on
}

void
MediaRecorder::recordData(AVPacket* packet, int streamIdx)
{
    if (!outputCtx_) {
        RING_ERR() << "Cannot record data (no output format)";
        return;
    }
}

void
MediaRecorder::flush()
{
    // all streams need to be flushed, video and audio, local and remote
    for (unsigned i = 0; i < outputCtx_->nb_streams; ++i) {
        AVPacket pkt;
        av_init_packet(&pkt);
        pkt.data = nullptr;
        pkt.size = 0;
        recordData(&pkt, i);
    }
}

} // namespace ring
