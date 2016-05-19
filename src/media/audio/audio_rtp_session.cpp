/*
 *  Copyright (C) 2014-2016 Savoir-faire Linux Inc.
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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
#include "audio_rtp_session.h"

#include "logger.h"
#include "noncopyable.h"
#include "sip/sdp.h"

#ifdef RING_VIDEO
#include "video/video_base.h"
#endif //RING_VIDEO

#include "socket_pair.h"
#include "media_encoder.h"
#include "media_decoder.h"
#include "media_io_handle.h"
#include "media_device.h"

#include "audio/audiobuffer.h"
#include "audio/ringbufferpool.h"
#include "audio/resampler.h"
#include "manager.h"
#include "smartools.h"
#include <sstream>

namespace ring {

class AudioSender {
    public:
        AudioSender(const std::string& id,
                    const std::string& dest,
                    const MediaDescription& args,
                    SocketPair& socketPair,
                    const uint16_t seqVal);
        ~AudioSender();

        void setMuted(bool isMuted);
        uint16_t getLastSeqValue();

    private:
        NON_COPYABLE(AudioSender);

        bool setup(SocketPair& socketPair);

        std::string id_;
        std::string dest_;
        MediaDescription args_;
        std::unique_ptr<MediaEncoder> audioEncoder_;
        std::unique_ptr<MediaIOHandle> muxContext_;
        std::unique_ptr<Resampler> resampler_;

        AudioBuffer micData_;
        AudioBuffer resampledData_;
        const uint16_t seqVal_;

        using seconds = std::chrono::duration<double, std::ratio<1>>;
        const seconds secondsPerPacket_ {0.02}; // 20 ms

        ThreadLoop loop_;
        void process();
        void cleanup();
};

AudioSender::AudioSender(const std::string& id,
                         const std::string& dest,
                         const MediaDescription& args,
                         SocketPair& socketPair,
                         const uint16_t seqVal) :
    id_(id),
    dest_(dest),
    args_(args),
    seqVal_(seqVal),
    loop_([&] { return setup(socketPair); },
          std::bind(&AudioSender::process, this),
          std::bind(&AudioSender::cleanup, this))
{
    loop_.start();
}

AudioSender::~AudioSender()
{
    loop_.join();
}

bool
AudioSender::setup(SocketPair& socketPair)
{
    audioEncoder_.reset(new MediaEncoder);
    muxContext_.reset(socketPair.createIOContext());

    try {
        /* Encoder setup */
        RING_DBG("audioEncoder_->openOutput %s", dest_.c_str());
        audioEncoder_->openOutput(dest_.c_str(), args_);
        audioEncoder_->setInitSeqVal(seqVal_);
        audioEncoder_->setIOContext(muxContext_);
        audioEncoder_->startIO();
    } catch (const MediaEncoderException &e) {
        RING_ERR("%s", e.what());
        return false;
    }

#ifdef DEBUG_SDP
    audioEncoder_->print_sdp();
#endif

    return true;
}

void
AudioSender::cleanup()
{
    audioEncoder_.reset();
    muxContext_.reset();
    micData_.clear();
    resampledData_.clear();
}

void
AudioSender::process()
{
    auto& mainBuffer = Manager::instance().getRingBufferPool();
    auto mainBuffFormat = mainBuffer.getInternalAudioFormat();

    // compute nb of byte to get corresponding to 1 audio frame
    const std::size_t samplesToGet = std::chrono::duration_cast<std::chrono::seconds>(mainBuffFormat.sample_rate * secondsPerPacket_).count();

    if (mainBuffer.availableForGet(id_) < samplesToGet) {
        const auto wait_time = std::chrono::duration_cast<std::chrono::milliseconds>(secondsPerPacket_);
        if (not mainBuffer.waitForDataAvailable(id_, samplesToGet, wait_time))
            return;
    }

    // get data
    micData_.setFormat(mainBuffFormat);
    micData_.resize(samplesToGet);
    const auto samples = mainBuffer.getData(micData_, id_);
    if (samples != samplesToGet)
        return;

    // down/upmix as needed
    auto accountAudioCodec = std::static_pointer_cast<AccountAudioCodecInfo>(args_.codec);
    micData_.setChannelNum(accountAudioCodec->audioformat.nb_channels, true);

    if (mainBuffFormat.sample_rate != accountAudioCodec->audioformat.sample_rate) {
        if (not resampler_) {
            RING_DBG("Creating audio resampler");
            resampler_.reset(new Resampler(accountAudioCodec->audioformat));
        }
        resampledData_.setFormat(accountAudioCodec->audioformat);
        resampledData_.resize(samplesToGet);
        resampler_->resample(micData_, resampledData_);
        Smartools::getInstance().setLocalAudioCodec(audioEncoder_->getEncoderName());
        if (audioEncoder_->encode_audio(resampledData_) < 0)
            RING_ERR("encoding failed");
    } else {
        if (audioEncoder_->encode_audio(micData_) < 0)
            RING_ERR("encoding failed");
    }
}
void
AudioSender::setMuted(bool isMuted)
{
    audioEncoder_->setMuted(isMuted);
}

uint16_t
AudioSender::getLastSeqValue()
{
    return audioEncoder_->getLastSeqValue();
}


class AudioReceiveThread
{
    public:
        AudioReceiveThread(const std::string &id,
                           const AudioFormat& format,
                           const std::string& sdp);
        ~AudioReceiveThread();
        void addIOContext(SocketPair &socketPair);
        void startLoop();

    private:
        NON_COPYABLE(AudioReceiveThread);

        static constexpr auto SDP_FILENAME = "dummyFilename";

        static int interruptCb(void *ctx);
        static int readFunction(void *opaque, uint8_t *buf, int buf_size);

        void openDecoder();
        bool decodeFrame();

        /*-----------------------------------------------------------------*/
        /* These variables should be used in thread (i.e. process()) only! */
        /*-----------------------------------------------------------------*/
        const std::string id_;
        const AudioFormat& format_;

        DeviceParams args_;

        std::istringstream stream_;
        std::unique_ptr<MediaDecoder> audioDecoder_;
        std::unique_ptr<MediaIOHandle> sdpContext_;
        std::unique_ptr<MediaIOHandle> demuxContext_;

        std::shared_ptr<RingBuffer> ringbuffer_;

        ThreadLoop loop_;
        bool setup();
        void process();
        void cleanup();
};

AudioReceiveThread::AudioReceiveThread(const std::string& id,
                                       const AudioFormat& format,
                                       const std::string& sdp)
    : id_(id)
    , format_(format)
    , stream_(sdp)
    , sdpContext_(new MediaIOHandle(sdp.size(), false, &readFunction,
                                    0, 0, this))
    , loop_(std::bind(&AudioReceiveThread::setup, this),
            std::bind(&AudioReceiveThread::process, this),
            std::bind(&AudioReceiveThread::cleanup, this))
{}

AudioReceiveThread::~AudioReceiveThread()
{
    loop_.join();
}


bool
AudioReceiveThread::setup()
{
    audioDecoder_.reset(new MediaDecoder());
    audioDecoder_->setInterruptCallback(interruptCb, this);
    // custom_io so the SDP demuxer will not open any UDP connections
    args_.input = SDP_FILENAME;
    args_.format = "sdp";
    args_.sdp_flags = "custom_io";
    EXIT_IF_FAIL(not stream_.str().empty(), "No SDP loaded");
    audioDecoder_->setIOContext(sdpContext_.get());
    EXIT_IF_FAIL(not audioDecoder_->openInput(args_),
        "Could not open input \"%s\"", SDP_FILENAME);
    // Now replace our custom AVIOContext with one that will read packets
    audioDecoder_->setIOContext(demuxContext_.get());

    EXIT_IF_FAIL(not audioDecoder_->setupFromAudioData(format_),
                 "decoder IO startup failed");

    ringbuffer_ = Manager::instance().getRingBufferPool().getRingBuffer(id_);
    return true;
}

void
AudioReceiveThread::process()
{
    AudioFormat mainBuffFormat = Manager::instance().getRingBufferPool().getInternalAudioFormat();
    AudioFrame decodedFrame;

    switch (audioDecoder_->decode(decodedFrame)) {

        case MediaDecoder::Status::FrameFinished:
            audioDecoder_->writeToRingBuffer(decodedFrame, *ringbuffer_,
                                             mainBuffFormat);
            // Refresh the remote audio codec in the callback SmartInfo
            Smartools::getInstance().setRemoteAudioCodec(audioDecoder_->getDecoderName());
            return;

        case MediaDecoder::Status::DecodeError:
            RING_WARN("decoding failure, trying to reset decoder...");
            if (not setup()) {
                RING_ERR("fatal error, rx thread re-setup failed");
                loop_.stop();
                break;
            }
            if (not audioDecoder_->setupFromAudioData(format_)) {
                RING_ERR("fatal error, a-decoder setup failed");
                loop_.stop();
                break;
            }
            break;

        case MediaDecoder::Status::ReadError:
            RING_ERR("fatal error, read failed");
            loop_.stop();
            break;

        case MediaDecoder::Status::Success:
        case MediaDecoder::Status::EOFError:
        default:
            break;
    }
}

void
AudioReceiveThread::cleanup()
{
    audioDecoder_.reset();
    demuxContext_.reset();
}

int
AudioReceiveThread::readFunction(void* opaque, uint8_t* buf, int buf_size)
{
    std::istream& is = static_cast<AudioReceiveThread*>(opaque)->stream_;
    is.read(reinterpret_cast<char*>(buf), buf_size);
    return is.gcount();
}

// This callback is used by libav internally to break out of blocking calls
int
AudioReceiveThread::interruptCb(void* data)
{
    auto context = static_cast<AudioReceiveThread*>(data);
    return not context->loop_.isRunning();
}

void
AudioReceiveThread::addIOContext(SocketPair& socketPair)
{
    demuxContext_.reset(socketPair.createIOContext());
}

void
AudioReceiveThread::startLoop()
{
    loop_.start();
}

AudioRtpSession::AudioRtpSession(const std::string& id)
    : RtpSession(id)
{
    // don't move this into the initializer list or Cthulus will emerge
    ringbuffer_ = Manager::instance().getRingBufferPool().createRingBuffer(callID_);
}

AudioRtpSession::~AudioRtpSession()
{
    stop();
}

void
AudioRtpSession::startSender()
{
    if (not send_.enabled or send_.holding) {
        RING_WARN("Audio sending disabled");
        if (sender_) {
            if (socketPair_)
                socketPair_->interrupt();
            sender_.reset();
        }
        return;
    }

    if (sender_)
        RING_WARN("Restarting audio sender");


    // be sure to not send any packets before saving last RTP seq value
    socketPair_->stopSendOp();
    if (sender_)
        initSeqVal_ = sender_->getLastSeqValue() + 1;
    try {
        sender_.reset();
        socketPair_->stopSendOp(false);
        sender_.reset(new AudioSender(callID_, getRemoteRtpUri(), send_,
                                      *socketPair_, initSeqVal_));
    } catch (const MediaEncoderException &e) {
        RING_ERR("%s", e.what());
        send_.enabled = false;
    }
}

void
AudioRtpSession::restartSender()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    // ensure that start has been called before restart
    if (not socketPair_)
        return;

    startSender();
}

void
AudioRtpSession::startReceiver()
{
    if (not receive_.enabled or receive_.holding) {
        RING_WARN("Audio receiving disabled");
        receiveThread_.reset();
        return;
    }

    if (receiveThread_)
        RING_WARN("Restarting audio receiver");

    auto accountAudioCodec = std::static_pointer_cast<AccountAudioCodecInfo>(receive_.codec);
    receiveThread_.reset(new AudioReceiveThread(callID_, accountAudioCodec->audioformat,
                                                receive_.receiving_sdp));
    receiveThread_->addIOContext(*socketPair_);
    receiveThread_->startLoop();
}

void
AudioRtpSession::start(std::unique_ptr<IceSocket> rtp_sock, std::unique_ptr<IceSocket> rtcp_sock)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (not send_.enabled and not receive_.enabled) {
        stop();
        return;
    }

    try {
        if (rtp_sock and rtcp_sock)
            socketPair_.reset(new SocketPair(std::move(rtp_sock), std::move(rtcp_sock)));
        else
            socketPair_.reset(new SocketPair(getRemoteRtpUri().c_str(), receive_.addr.getPort()));

        if (send_.crypto and receive_.crypto) {
            socketPair_->createSRTP(receive_.crypto.getCryptoSuite().c_str(),
                                    receive_.crypto.getSrtpKeyInfo().c_str(),
                                    send_.crypto.getCryptoSuite().c_str(),
                                    send_.crypto.getSrtpKeyInfo().c_str());
        }
    } catch (const std::runtime_error& e) {
        RING_ERR("Socket creation failed: %s", e.what());
        return;
    }

    startSender();
    startReceiver();
}

void
AudioRtpSession::stop()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (socketPair_)
        socketPair_->interrupt();

    receiveThread_.reset();
    sender_.reset();
    socketPair_.reset();
}
void
AudioRtpSession::setMuted(bool isMuted)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (sender_)
        sender_->setMuted(isMuted);
}

} // namespace ring
