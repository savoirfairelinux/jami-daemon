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

#include "libav_deps.h" // MUST BE INCLUDED FIRST
#include "fileutils.h"
#include "logger.h"
#include "media_recorder.h"

#include <algorithm>
#include <sstream>
#include <sys/types.h>
#include <time.h>

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

MediaRecorder::MediaRecorder()
{}

MediaRecorder::~MediaRecorder()
{
    if (isRecording_)
        flush();
    if (outputCtx_ && outputCtx_->oformat && !(outputCtx_->oformat->flags & AVFMT_NOFILE))
        avio_closep(&outputCtx_->pb);
    avformat_free_context(outputCtx_);
}

std::string
MediaRecorder::getFilename() const
{
    return dir_ + filename_ + "." + ext_;
}

void
MediaRecorder::audioOnly(bool audioOnly)
{
    audioOnly_ = audioOnly;
    // TODO change container depending on codec
    if (audioOnly)
        ext_ = "mka";
    else
        ext_ = "mkv";
}

void
MediaRecorder::setRecordingPath(const std::string& dir)
{
    if (!dir.empty() && fileutils::isDirectory(dir))
        dir_ = dir;
    else
        dir_ = fileutils::get_home_dir();
    if (dir_.back() != DIR_SEPARATOR_CH)
        dir_ = dir_ + DIR_SEPARATOR_CH;
    RING_DBG() << "Recording will be saved in '" << dir_ << "'";
}

void
MediaRecorder::incrementStreams(int n)
{
    nbStreams_ += n;
}

bool
MediaRecorder::isRecording() const
{
    return isRecording_;
}

bool
MediaRecorder::toggleRecording()
{
    if (isRecording_) {
        stopRecording();
        isRecording_ = false;
    } else {
        startRecording();
        isRecording_ = true;
    }
    return isRecording_;
}

int
MediaRecorder::startRecording()
{
    std::lock_guard<std::mutex> lk(mutex_);
    filename_ = createTimestamp();

    RING_DBG() << "Start recording '" << getFilename() << "'";

    // close file before opening a new one
    if (outputCtx_ && outputCtx_->oformat && !(outputCtx_->oformat->flags & AVFMT_NOFILE))
        avio_closep(&outputCtx_->pb);
    avformat_free_context(outputCtx_);

    avformat_alloc_output_context2(&outputCtx_, NULL, NULL, getFilename().c_str());
    isRecording_ = true;
    return 0;
}

void
MediaRecorder::stopRecording()
{
    if (isRecording_) {
        RING_DBG() << "Stop recording '" << getFilename() << "'";
        flush();
    }
}

int
MediaRecorder::copyStream(AVStream* input, AVPacket* packet, bool fromPeer, bool isVideo)
{
    std::lock_guard<std::mutex> lk(mutex_);

    if (!outputCtx_) {
        RING_ERR() << "Failed to allocate output context for recording";
        return -1;
    }

    if (audioOnly_ && input->codecpar->codec_type != AVMEDIA_TYPE_AUDIO)
        return -1;

    auto key = std::make_pair(fromPeer, isVideo);
    if (streamMap_.find(key) != streamMap_.end()) {
        RING_WARN() << "Could not create stream: key already exists";
        return -1;
    }

    AVStream* outStream = avformat_new_stream(outputCtx_, nullptr);
    if (!outStream) {
        RING_ERR() << "Failed to allocate output stream for recording";
        return -1;
    }

    if (avcodec_parameters_copy(outStream->codecpar, input->codecpar) < 0) {
        RING_ERR() << "Failed to copy codec parameters to recorder";
        return -1;
    }

    outStream->codecpar->codec_tag = 0;
    outStream->time_base = input->time_base;

    // if packet contains side data, copy it to outStream
    if (packet && packet->side_data && outStream->codecpar->extradata_size == 0) {
        // needs to be av_malloc'd to avoid mismatched free (FFmpeg frees it with av_free)
        const int sideDataSize = packet->side_data->size + AV_INPUT_BUFFER_PADDING_SIZE;
        outStream->codecpar->extradata = static_cast<uint8_t*>(av_mallocz(sideDataSize));
        if (!outStream->codecpar->extradata) {
            RING_ERR() << "Could not copy side data to recorder";
            return -1;
        }
        std::copy(&packet->side_data->data[0],
                  &packet->side_data->data[0] + packet->side_data->size,
                  outStream->codecpar->extradata);
        outStream->codecpar->extradata_size = packet->side_data->size;
    }

    if (!(outputCtx_->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&outputCtx_->pb, getFilename().c_str(), AVIO_FLAG_WRITE) < 0) {
            RING_ERR() << "Could not open output file: " << getFilename();
            return -1;
        }
    }

    // all streams are set up, we can start writing to the file
    if (outputCtx_->nb_streams == nbStreams_) {
        int ret = 0;
        if ((ret = avformat_write_header(outputCtx_, nullptr)) < 0) {
            RING_ERR() << "Failed to write header for recording";
            isRecording_ = false; // if header cannot be written, recording cannot be started
            return -1;
        }
        isReady_ = true;
        RING_DBG() << "Recording initialized";
        av_dump_format(outputCtx_, 0, getFilename().c_str(), 1);
    }

    streamMap_[key] = outStream->index;
    lastTimestamp_[outStream->index] = 0;

    return 0;
}

int
MediaRecorder::recordData(AVPacket* packet, bool fromPeer, bool isVideo)
{
    std::lock_guard<std::mutex> lk(mutex_);

    if (!isRecording_)
        return 0;

    auto key = std::make_pair(fromPeer, isVideo);
    auto it = streamMap_.find(key);
    if (it == streamMap_.end()) {
        RING_WARN() << (fromPeer ? "Remote " : "Local ") << (isVideo ? "video" : "audio")
            << " stream not found";
        return -1;
    }
    return recordData(packet, streamMap_[key]);
}

int
MediaRecorder::recordData(AVPacket* packet, int streamIdx)
{
    if (!outputCtx_) {
        RING_ERR() << "Cannot record data (no output format)";
        return -1;
    }

    // TODO enqueue AVPacket if header not written yet
    if (!isReady_)
        return -1;

    AVStream* st = outputCtx_->streams[streamIdx];
    AVPacket* clone = av_packet_clone(packet);
    auto ts = av_gettime() * st->time_base.num / st->time_base.den;
    clone->stream_index = streamIdx;
    if (firstTimestamp_.find(streamIdx) == firstTimestamp_.end()) {
        firstTimestamp_[streamIdx] = ts;
    } else {
        clone->duration = ts - lastTimestamp_[streamIdx];
    }
    clone->pts = ts - firstTimestamp_[streamIdx];
    clone->dts = ts - firstTimestamp_[streamIdx];
    clone->pos = -1;
    lastTimestamp_[streamIdx] = ts;

    int ret = 0;
    if ((ret = av_write_frame(outputCtx_, clone)) < 0)
        RING_ERR() << "Could not write data to record file";

    av_packet_unref(clone);
    return ret;
}

int
MediaRecorder::flush()
{
    std::lock_guard<std::mutex> lk(mutex_);

    if (!outputCtx_)
        return -1;

    if (outputCtx_->nb_streams <= 0 || !isRecording_)
        return 0;

    for (unsigned i = 0; i < outputCtx_->nb_streams; ++i) {
        AVPacket pkt;
        av_init_packet(&pkt);
        pkt.data = nullptr;
        pkt.size = 0;
        recordData(&pkt, i);
    }
    av_write_trailer(outputCtx_);

    isRecording_ = false;
    return 0;
}

} // namespace ring
