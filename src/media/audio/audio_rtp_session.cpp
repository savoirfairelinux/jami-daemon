/*
 *  Copyright (C) 2014-2018 Savoir-faire Linux Inc.
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

#include "libav_deps.h" // MUST BE INCLUDED FIRST

#include "audio_rtp_session.h"

#include "logger.h"
#include "noncopyable.h"
#include "sip/sdp.h"

#ifdef RING_VIDEO
#include "video/video_base.h"
#endif //RING_VIDEO

#include "socket_pair.h"
#include "media_recorder.h"
#include "media_encoder.h"
#include "media_decoder.h"
#include "media_io_handle.h"
#include "media_device.h"

#include "audio/audio_input.h"
#include "audio/audiobuffer.h"
#include "audio/ringbufferpool.h"
#include "audio/resampler.h"
#include "client/videomanager.h"
#include "manager.h"
#include "observer.h"
#include "smartools.h"
#include <sstream>

namespace ring {

constexpr static auto NEWPARAMS_TIMEOUT = std::chrono::milliseconds(1000);

class AudioSender : public Observer<std::shared_ptr<AudioFrame>> {
    public:
        AudioSender(const std::string& id,
                    const std::string& dest,
                    const MediaDescription& args,
                    SocketPair& socketPair,
                    const uint16_t seqVal,
                    bool muteState,
                    const uint16_t mtu);
        ~AudioSender();

        void setMuted(bool isMuted);
        uint16_t getLastSeqValue();

        void update(Observable<std::shared_ptr<ring::AudioFrame>>*,
                    const std::shared_ptr<ring::AudioFrame>&) override;

        void initRecorder(std::shared_ptr<MediaRecorder>& rec);

    private:
        NON_COPYABLE(AudioSender);

        bool setup(SocketPair& socketPair);

        std::string id_;
        std::string dest_;
        MediaDescription args_;
        std::unique_ptr<MediaEncoder> audioEncoder_;
        std::unique_ptr<MediaIOHandle> muxContext_;
        std::unique_ptr<Resampler> resampler_;
        std::shared_ptr<AudioInput> audioInput_;
        std::weak_ptr<MediaRecorder> recorder_;

        uint64_t sent_samples = 0;

        AudioBuffer micData_;
        AudioBuffer resampledData_;
        const uint16_t seqVal_;
        bool muteState_ = false;
        uint16_t mtu_;

        const std::chrono::milliseconds msPerPacket_ {20};
};

AudioSender::AudioSender(const std::string& id,
                         const std::string& dest,
                         const MediaDescription& args,
                         SocketPair& socketPair,
                         const uint16_t seqVal,
                         bool muteState,
                         const uint16_t mtu) :
    id_(id),
    dest_(dest),
    args_(args),
    seqVal_(seqVal),
    muteState_(muteState),
    mtu_(mtu)
{
    setup(socketPair);
}

AudioSender::~AudioSender()
{
    if (auto rec = recorder_.lock())
        rec->stopRecording();
    audioInput_->detach(this);
    audioInput_.reset();
    audioEncoder_.reset();
    muxContext_.reset();
    micData_.clear();
    resampledData_.clear();
}

bool
AudioSender::setup(SocketPair& socketPair)
{
    audioEncoder_.reset(new MediaEncoder);
    muxContext_.reset(socketPair.createIOContext(mtu_));

    try {
        /* Encoder setup */
        RING_DBG("audioEncoder_->openLiveOutput %s", dest_.c_str());
        audioEncoder_->setMuted(muteState_);
        audioEncoder_->openLiveOutput(dest_, args_);
        audioEncoder_->setInitSeqVal(seqVal_);
        audioEncoder_->setIOContext(muxContext_);
        audioEncoder_->startIO();
    } catch (const MediaEncoderException &e) {
        RING_ERR("%s", e.what());
        return false;
    }

    Smartools::getInstance().setLocalAudioCodec(audioEncoder_->getEncoderName());

#ifdef DEBUG_SDP
    audioEncoder_->print_sdp();
#endif

    // NOTE do after encoder is ready to encode
    auto codec = std::static_pointer_cast<AccountAudioCodecInfo>(args_.codec);
    audioInput_ = ring::getAudioInput(id_);
    audioInput_->setFormat(codec->audioformat);
    audioInput_->attach(this);

    return true;
}

void
AudioSender::update(Observable<std::shared_ptr<ring::AudioFrame>>* /*obs*/, const std::shared_ptr<ring::AudioFrame>& framePtr)
{
    auto frame = framePtr->pointer();
    auto ms = MediaStream("a:local", frame->format, rational<int>(1, frame->sample_rate),
                          frame->sample_rate, frame->channels, frame->nb_samples);
    frame->pts = sent_samples;
    ms.firstTimestamp = frame->pts;
    sent_samples += frame->nb_samples;

    {
        auto rec = recorder_.lock();
        if (rec && rec->isRecording())
            rec->recordData(frame, ms);
    }

    if (audioEncoder_->encodeAudio(*framePtr) < 0)
        RING_ERR("encoding failed");
}

void
AudioSender::setMuted(bool isMuted)
{
    muteState_ = isMuted;
    audioEncoder_->setMuted(isMuted);
}

uint16_t
AudioSender::getLastSeqValue()
{
    return audioEncoder_->getLastSeqValue();
}

void
AudioSender::initRecorder(std::shared_ptr<MediaRecorder>& rec)
{
    recorder_ = rec;
    rec->incrementExpectedStreams(1);
}

class AudioReceiveThread
{
    public:
        AudioReceiveThread(const std::string &id,
                           const AudioFormat& format,
                           const std::string& sdp,
                           const uint16_t mtu);
        ~AudioReceiveThread();
        void addIOContext(SocketPair &socketPair);
        void startLoop();

        void initRecorder(std::shared_ptr<MediaRecorder>& rec);

    private:
        NON_COPYABLE(AudioReceiveThread);

        static constexpr auto SDP_FILENAME = "dummyFilename";

        static int interruptCb(void *ctx);
        static int readFunction(void *opaque, uint8_t *buf, int buf_size);

        void openDecoder();
        bool decodeFrame();

        std::weak_ptr<MediaRecorder> recorder_;

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

        uint16_t mtu_;

        ThreadLoop loop_;
        bool setup();
        void process();
        void cleanup();
};

AudioReceiveThread::AudioReceiveThread(const std::string& id,
                                       const AudioFormat& format,
                                       const std::string& sdp,
                                       const uint16_t mtu)
    : id_(id)
    , format_(format)
    , stream_(sdp)
    , sdpContext_(new MediaIOHandle(sdp.size(), false, &readFunction,
                                    0, 0, this))
    , mtu_(mtu)
    , loop_(std::bind(&AudioReceiveThread::setup, this),
            std::bind(&AudioReceiveThread::process, this),
            std::bind(&AudioReceiveThread::cleanup, this))
{}

AudioReceiveThread::~AudioReceiveThread()
{
    if (auto rec = recorder_.lock())
        rec->stopRecording();
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

    if (stream_.str().empty()) {
        RING_ERR("No SDP loaded");
        return false;
    }

    audioDecoder_->setIOContext(sdpContext_.get());
    if (audioDecoder_->openInput(args_)) {
        RING_ERR("Could not open input \"%s\"", SDP_FILENAME);
        return false;
    }

    // Now replace our custom AVIOContext with one that will read packets
    audioDecoder_->setIOContext(demuxContext_.get());
    if (audioDecoder_->setupFromAudioData()) {
        RING_ERR("decoder IO startup failed");
        return false;
    }

    Smartools::getInstance().setRemoteAudioCodec(audioDecoder_->getDecoderName());

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
            {
                auto rec = recorder_.lock();
                if (rec && rec->isRecording())
                    rec->recordData(decodedFrame.pointer(), audioDecoder_->getStream("a:remote"));
            }
            audioDecoder_->writeToRingBuffer(decodedFrame, *ringbuffer_,
                                             mainBuffFormat);
            return;

        case MediaDecoder::Status::DecodeError:
            RING_WARN("decoding failure, trying to reset decoder...");
            if (not setup()) {
                RING_ERR("fatal error, rx thread re-setup failed");
                loop_.stop();
            } else if (not audioDecoder_->setupFromAudioData()) {
                RING_ERR("fatal error, a-decoder setup failed");
                loop_.stop();
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

    auto count = is.gcount();
    return count ? count : AVERROR_EOF;
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
    demuxContext_.reset(socketPair.createIOContext(mtu_));
}

void
AudioReceiveThread::startLoop()
{
    loop_.start();
}

void
AudioReceiveThread::initRecorder(std::shared_ptr<MediaRecorder>& rec)
{
    recorder_ = rec;
    rec->incrementExpectedStreams(1);
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

    // sender sets up input correctly, we just keep a reference in case startSender is called
    audioInput_ = ring::getAudioInput(callID_);
    auto newParams = audioInput_->switchInput(input_);
    try {
        if (newParams.valid() &&
            newParams.wait_for(NEWPARAMS_TIMEOUT) == std::future_status::ready) {
            localAudioParams_ = newParams.get();
        } else {
            RING_ERR() << "No valid new audio parameters";
            return;
        }
    } catch (const std::exception& e) {
        RING_ERR() << "Exception while retrieving audio parameters: " << e.what();
        return;
    }

    // be sure to not send any packets before saving last RTP seq value
    socketPair_->stopSendOp();
    if (sender_)
        initSeqVal_ = sender_->getLastSeqValue() + 1;
    try {
        sender_.reset();
        socketPair_->stopSendOp(false);
        sender_.reset(new AudioSender(callID_, getRemoteRtpUri(), send_,
                                      *socketPair_, initSeqVal_, muteState_, mtu_));
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
                                                receive_.receiving_sdp,
                                                mtu_));
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
    if (sender_) {
        muteState_ = isMuted;
        sender_->setMuted(isMuted);
    }
}

void
AudioRtpSession::initRecorder(std::shared_ptr<MediaRecorder>& rec)
{
    if (receiveThread_)
        receiveThread_->initRecorder(rec);
    if (sender_)
        sender_->initRecorder(rec);
}

} // namespace ring
