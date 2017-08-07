/*  Copyright (C) 2004-2017 Savoir-faire Linux Inc.
 *
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *
 *  Inspired by tonegenerator of
 *   Laurielle Lea <laurielle.lea@savoirfairelinux.com> (2004)
 *  Inspired by ringbuffer of Audacity Project
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
#include <fstream>
#include <cmath>
#include <samplerate.h>
#include <cstring>
#include <vector>
#include <climits>

#ifndef RING_UWP
//#include <sndfile.hh>
#include "libav_deps.h"
#endif
extern "C"{
#include <libavutil/log.h>
}
#include "audiofile.h"
#include "audio/resampler.h"
#include "manager.h"
#include "client/ring_signal.h"

#include "logger.h"

namespace ring {

void
AudioFile::onBufferFinish()
{
    // We want to send values in milisecond
    const int divisor = buffer_->getSampleRate() / 1000;

    if (divisor == 0) {
        RING_ERR("Error cannot update playback slider, sampling rate is 0");
        return;
    }

    if ((updatePlaybackScale_ % 5) == 0)
        emitSignal<DRing::CallSignal::UpdatePlaybackScale>(filepath_,
                                                           (unsigned)(pos_ / divisor),
                                                           (unsigned)(buffer_->frames() / divisor));

    updatePlaybackScale_++;
}

#ifndef RING_UWP
static auto avFormatDeleter = [](AVFormatContext* fmtCtx){ avformat_close_input(&fmtCtx); };
#endif

static int
decodeFrame(AVCodecContext* ctx, AVFrame* frame, int* gotFrame, AVPacket* pkt)
{
    int ret = 0;
    *gotFrame = 0;

    if (pkt) {
        ret = avcodec_send_packet(ctx, pkt);
        // we don't expect AVERROR(EAGAIN), because we read all decoded frames with avcodec_receive_frame() until done.
        if (ret < 0)
            return (ret == AVERROR_EOF) ? 0 : ret;
    }

    ret = avcodec_receive_frame(ctx, frame);
    if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
        return ret;

    if (ret >= 0)
        *gotFrame = 1;

    return 0;
}

AudioFile::AudioFile(const std::string &fileName, unsigned int sampleRate) :
    AudioLoop(sampleRate), filepath_(fileName), updatePlaybackScale_(0)
{
#ifndef RING_UWP
    // setup begin

    int ret = 0;
    std::unique_ptr<AVFormatContext, std::function<void(AVFormatContext*)>> fmtCtx = std::unique_ptr<AVFormatContext, std::function<void(AVFormatContext*)>>(nullptr, avFormatDeleter);

    AVFormatContext* ctx = avformat_alloc_context();
    if ((ret = avformat_open_input(&ctx, fileName.c_str(), 0, 0)) < 0)
        throw AudioFileException("Could not open " + fileName);

    fmtCtx.reset(ctx);

    if ((ret = avformat_find_stream_info(fmtCtx.get(), 0)) < 0)
        throw AudioFileException("Failed to find stream info in " + fileName);

    if ((ret = av_find_best_stream(fmtCtx.get(), AVMEDIA_TYPE_AUDIO, -1, -1, 0, 0)) < 0)
        throw AudioFileException("No audio stream in " + fileName);

    // XXX put in an ifdef debug?
    av_dump_format(fmtCtx.get(), ret, fileName.c_str(), 0);

    AVStream* stream = fmtCtx->streams[ret];
    AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec)
        throw AudioFileException("No decoder found for " + fileName);

    AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecCtx, stream->codecpar);
    if ((ret = avcodec_open2(codecCtx, codec, 0)) < 0)
        throw AudioFileException("Could not open codec " + std::string(avcodec_get_name(codecCtx->codec_id)));

    // input parameters
    uint64_t inChannelLayout = av_get_default_channel_layout(codecCtx->channels); // not set in codec context
    AVSampleFormat inFormat = codecCtx->sample_fmt;
    int inRate = codecCtx->sample_rate;

    // output parameters
    uint64_t outChannelLayout = AV_CH_LAYOUT_STEREO;
    AVSampleFormat outFormat = AV_SAMPLE_FMT_S16;
    int outRate = static_cast<int32_t>(sampleRate);

    SwrContext* swrCtx = swr_alloc_set_opts(nullptr,
            outChannelLayout,
            outFormat,
            outRate,
            inChannelLayout,
            inFormat,
            inRate,
            av_log_get_level(),
            nullptr);
    if ((ret = swr_init(swrCtx)) < 0)
        throw AudioFileException("Failed to initialize resampling context");

    // decode start

    int totalSamples = 0;
    constexpr int maxFrameSize = 192000; // 1 second of 48 kHz 32 bit audio

    int outBufferSize = 0;
    std::vector<AudioSample> sampleBuffer;
    std::vector<uint8_t*> outBuffer;
    outBuffer.reserve(maxFrameSize * 2); // more space just in case

    AVFrame* frame = av_frame_alloc();
    AVPacket packet;
    av_init_packet(&packet);

    bool shouldContinue = true;
    int frameFinished = 0;

    while (av_read_frame(fmtCtx.get(), &packet) >= 0) {
        ret = decodeFrame(codecCtx, frame, &frameFinished, &packet);

        if (ret < 0) {
            throw AudioFileException("Unable to decode frame");
        }

        if (frameFinished) {
            outBufferSize = swr_get_out_samples(swrCtx, frame->nb_samples);
            int count = swr_convert(swrCtx, &outBuffer[0], maxFrameSize, (const uint8_t**)frame->extended_data, frame->nb_samples);

            // reserve space for 16 bit stereo (2 bytes * 2 channels)
            constexpr int multiplier = 2 * 2;
            sampleBuffer.reserve(sampleBuffer.size() + count * multiplier);
            // append outBuffer to sampleBuffer
            memcpy(&sampleBuffer[sampleBuffer.size()], &outBuffer[0], count * multiplier * frame->channels);
            totalSamples += count * frame->channels;
        }
    }

    av_packet_unref(&packet);
    av_frame_free(&frame);

    auto buffer = new AudioBuffer(sampleBuffer.data(), totalSamples, AudioFormat(outRate, av_get_channel_layout_nb_channels(outChannelLayout)));
    delete buffer_;
    buffer_ = buffer;

//    int format;
//    bool hasHeader = true;
//
//    if (filepath_.find(".wav") != std::string::npos) {
//        format = SF_FORMAT_WAV;
//    } else if (filepath_.find(".ul") != std::string::npos) {
//        format = SF_FORMAT_RAW | SF_FORMAT_ULAW;
//        hasHeader = false;
//    } else if (filepath_.find(".al") != std::string::npos) {
//        format = SF_FORMAT_RAW | SF_FORMAT_ALAW;
//        hasHeader = false;
//    } else if (filepath_.find(".au") != std::string::npos) {
//        format = SF_FORMAT_AU;
//    } else if (filepath_.find(".flac") != std::string::npos) {
//        format = SF_FORMAT_FLAC;
//    } else if (filepath_.find(".ogg") != std::string::npos) {
//        format = SF_FORMAT_OGG;
//    } else {
//        RING_WARN("No file extension, guessing WAV");
//        format = SF_FORMAT_WAV;
//    }
//
//    SndfileHandle fileHandle(fileName.c_str(), SFM_READ, format, hasHeader ? 0 : 1,
//                             hasHeader ? 0 : 8000);
//
//    if (!fileHandle)
//        throw AudioFileException("File handle " + fileName + " could not be created");
//    if (fileHandle.error()) {
//        RING_ERR("Error fileHandle: %s", fileHandle.strError());
//        throw AudioFileException("File " + fileName + " doesn't exist");
//    }
//
//    switch (fileHandle.channels()) {
//        case 1:
//        case 2:
//            break;
//        default:
//            throw AudioFileException("Unsupported number of channels");
//    }
//
//    // get # of bytes in file
//    const size_t fileSize = fileHandle.seek(0, SEEK_END);
//    fileHandle.seek(0, SEEK_SET);
//
//    const sf_count_t nbFrames = hasHeader ? fileHandle.frames() : fileSize / fileHandle.channels();
//
//    AudioSample * interleaved = new AudioSample[nbFrames * fileHandle.channels()];
//
//    // get n "items", aka samples (not frames)
//    fileHandle.read(interleaved, nbFrames * fileHandle.channels());
//
//    AudioBuffer * buffer = new AudioBuffer(nbFrames, AudioFormat(fileHandle.samplerate(), fileHandle.channels()));
//    buffer->deinterleave(interleaved, nbFrames, fileHandle.channels());
//    delete [] interleaved;
//
//    const int rate = static_cast<int32_t>(sampleRate);
//
//    if (fileHandle.samplerate() != rate) {
//        Resampler resampler(std::max(fileHandle.samplerate(), rate), fileHandle.channels(), true);
//        AudioBuffer * resampled = new AudioBuffer(nbFrames, AudioFormat(rate, fileHandle.channels()));
//        resampler.resample(*buffer, *resampled);
//        delete buffer;
//        delete buffer_;
//        buffer_ = resampled;
//    } else {
//        delete buffer_;
//        buffer_ = buffer;
//    }
#endif
}

} // namespace ring
