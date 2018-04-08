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
#include "media_io_handle.h"
#include "media_recorder.h"
#include "system_codec_container.h"

extern "C" {
#include <libavutil/frame.h>
}

#include <algorithm>
#include <sstream>
#include <sys/types.h>
#include <time.h>

namespace ring {

static std::string
createTimestamp()
{
    time_t rawtime = time(nullptr);
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

    // TIME
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
}

std::string
MediaRecorder::getFilename() const
{
    return dir_ + filename_ + ".ogg";
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
    nbExpectedStreams_ += n;
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
    } else {
        startRecording();
    }
    return isRecording_;
}

int
MediaRecorder::startRecording()
{
    filename_ = createTimestamp();
    encoder_.reset(new MediaEncoder);

    RING_DBG() << "Start recording '" << getFilename() << "'";
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
    isRecording_ = false;
}

int
MediaRecorder::addStream(bool isVideo, bool fromPeer, MediaStream ms)
{
    // video not yet implemented
    if (isVideo)
        return 0;

    // overwrite stream name for simplicity's sake
    std::string streamName;
    ms.name = (fromPeer ? "a:peer" : "a:local");
    ++nbReceivedAudioStreams_;
    streamParams_[isVideo][fromPeer] = ms;

    // wait until all streams are ready before writing to the file
    if (nbExpectedStreams_ != nbReceivedAudioStreams_)
        return 0;
    else
        return initRecord();
}

int
MediaRecorder::initRecord()
{
    std::lock_guard<std::mutex> lk(mutex_);

    // use peer parameters if possible, else fall back on local parameters
    int sampleRate = streamParams_[false][true].sampleRate;
    if (sampleRate == 0) sampleRate = streamParams_[false][false].sampleRate;
    int nbChannels = streamParams_[false][true].nbChannels;
    if (nbChannels == 0) nbChannels = streamParams_[false][false].nbChannels;

    std::map<std::string, std::string> options;
    options["sample_rate"] = std::to_string(sampleRate);
    options["channels"] = std::to_string(nbChannels);

    encoder_->openFileOutput(getFilename(), options);

    if (nbReceivedAudioStreams_ > 0) {
        std::vector<MediaStream> params;
        std::string aFilter;
        switch (nbReceivedAudioStreams_) {
        case 1:
            if (streamParams_[false].count(true) > 0)
                params.emplace_back(streamParams_[false][true]);
            else
                params.emplace_back(streamParams_[false][false]);
            audioFilter_.reset(); // no filter needed
            break;
        case 2:
            params.emplace_back(streamParams_[false][true]);
            params.emplace_back(streamParams_[false][false]);
            aFilter = "[a:local] [a:peer] amix, aresample=osr=48000:ocl=stereo:osf=s16";
            audioFilter_.reset(new MediaFilter);
            if (audioFilter_->initialize(aFilter, params) < 0) {
                RING_ERR() << "Failed to initialize audio filter";
                return -1;
            }
            break;
        default:
            RING_ERR() << "Recording more than 2 audio streams is not supported";
            return AVERROR(ENOTSUP);
        }

        auto audioCodec = std::static_pointer_cast<ring::SystemAudioCodecInfo>(
            getSystemCodecContainer()->searchCodecByName("opus", ring::MEDIA_AUDIO));
        audioIdx_ = encoder_->addStream(*audioCodec.get());
        if (audioIdx_ < 0) {
            RING_ERR() << "Failed to add audio stream to encoder";
            return -1;
        }
    } else
        audioFilter_.reset();

    isReady_ = (nbReceivedAudioStreams_ > 0 && audioIdx_ >= 0); // has audio and valid stream index
    if (isReady_) {
        std::unique_ptr<MediaIOHandle> ioHandle;
        try {
            encoder_->setIOContext(ioHandle);
            encoder_->startIO();
        } catch (const MediaEncoderException& e) {
            RING_ERR() << "Could not start recorder: " << e.what();
            return -1;
        }
        RING_DBG() << "Recording initialized";
        return 0;
    } else {
        RING_ERR() << "Something went wrong when initializing the recorder";
        return -1;
    }
}

int
MediaRecorder::recordData(AVFrame* frame, bool isVideo, bool fromPeer)
{
    // video not yet implemented
    if (isVideo)
        return 0;

    std::lock_guard<std::mutex> lk(mutex_);
    if (!isRecording_ || !isReady_)
        return 0;

    int streamIdx = audioIdx_;
    auto filter = audioFilter_.get();
    if (streamIdx < 0 || !filter) {
        RING_ERR() << "Specified stream is invalid: "
            << (fromPeer ? "remote " : "local ") << (isVideo ? "video" : "audio");
        return -1;
    }

    std::string inputName;
    if (!isVideo && nbReceivedAudioStreams_ == 2)
        inputName = (fromPeer ? "a:peer" : "a:local");

    // new reference because we are changing the timestamp
    AVFrame* input = av_frame_clone(frame);
    input->pts = nextTimestamp_[isVideo][fromPeer];
    nextTimestamp_[isVideo][fromPeer] += (isVideo ? 1 : input->nb_samples);

    if (inputName.empty()) // #nofilters
        return sendToEncoder(input, streamIdx);

    // empty filter graph output before sending more frames
    emptyFilterGraph();

    int err = filter->feedInput(input, inputName);
    av_frame_unref(input);

    return err;
}

void
MediaRecorder::emptyFilterGraph()
{
    AVFrame* output;
    if (audioIdx_ >= 0)
        while ((output = audioFilter_->readOutput()))
            sendToEncoder(output, audioIdx_);
}

int
MediaRecorder::sendToEncoder(AVFrame* frame, int streamIdx)
{
    try {
        encoder_->encode(frame, streamIdx);
    } catch (const MediaEncoderException& e) {
        RING_ERR() << "MediaEncoderException: " << e.what();
        av_frame_unref(frame);
        return -1;
    }
    av_frame_unref(frame);
    return 0;
}

int
MediaRecorder::flush()
{
    std::lock_guard<std::mutex> lk(mutex_);
    if (!isRecording_ || encoder_->getStreamCount() <= 0)
        return 0;

    encoder_->flush();

    return 0;
}

} // namespace ring
