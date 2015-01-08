/*
 *  Copyright (C) 2014 Savoir-Faire Linux Inc.
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
#include "avformat_rtp_session.h"

#include "logger.h"
#include "noncopyable.h"
#include "sip/sdp.h"
#include "video/socket_pair.h"
#include "video/video_base.h"
#include "video/video_encoder.h"
#include "video/video_decoder.h"
#include "video/libav_deps.h"
#include "audio/audiobuffer.h"
#include "audio/ringbufferpool.h"
#include "audio/resampler.h"
#include "manager.h"
#include <sstream>

namespace ring {
using ring::video::SocketPair;
using ring::video::VideoEncoder;
using ring::video::VideoIOHandle;
using ring::video::VideoEncoderException;

class AudioSender {
    public:
        AudioSender(const std::string& id,
                    std::map<std::string, std::string> txArgs,
                    ring::video::SocketPair& socketPair);
        ~AudioSender();

    private:
        NON_COPYABLE(AudioSender);

        bool waitForDataEncode(const std::chrono::milliseconds& max_wait) const;
        bool setup(ring::video::SocketPair& socketPair);

        std::string id_;
        std::map<std::string, std::string> args_;
        const AudioFormat format_;
        std::unique_ptr<ring::video::VideoEncoder> audioEncoder_;
        std::unique_ptr<ring::video::VideoIOHandle> muxContext_;
        std::unique_ptr<ring::Resampler> resampler_;
        const double secondsPerPacket_ {0.02}; // 20 ms

        ThreadLoop loop_;
        void process();
        void cleanup();
};

AudioSender::AudioSender(const std::string& id, std::map<std::string, std::string> txArgs, SocketPair& socketPair) :
    id_(id),
    args_(txArgs),
    format_(std::atoi(args_["sample_rate"].c_str()),
            std::atoi(args_["channels"].c_str())),
    loop_([&] { return setup(socketPair); },
          std::bind(&AudioSender::process, this),
          std::bind(&AudioSender::cleanup, this))
{
    std::ostringstream os;
    os << secondsPerPacket_ * format_.sample_rate;
    args_["frame_size"] = os.str();
    loop_.start();
}

AudioSender::~AudioSender()
{
    loop_.join();
}

bool
AudioSender::setup(SocketPair& socketPair)
{
    auto enc_name = args_["codec"].c_str();
    auto dest = args_["destination"].c_str();

    audioEncoder_.reset(new VideoEncoder);
    muxContext_.reset(socketPair.createIOContext());

    try {
        /* Encoder setup */
        audioEncoder_->setOptions(args_);
        audioEncoder_->openOutput(enc_name, "rtp", dest, NULL, false);
        audioEncoder_->setIOContext(muxContext_);
        audioEncoder_->startIO();
    } catch (const VideoEncoderException &e) {
        SFL_ERR("%s", e.what());
        return false;
    }

    std::string sdp;
    audioEncoder_->print_sdp(sdp);
    SFL_WARN("\n%s", sdp.c_str());

    return true;
}

void
AudioSender::cleanup()
{
    audioEncoder_.reset();
    muxContext_.reset();
}

void
AudioSender::process()
{
    auto mainBuffFormat = Manager::instance().getRingBufferPool().getInternalAudioFormat();
    double resampleFactor = mainBuffFormat.sample_rate / (double) format_.sample_rate;

    // compute nb of byte to get corresponding to 1 audio frame
    const size_t samplesToGet = resampleFactor * secondsPerPacket_ * format_.sample_rate;

    if (Manager::instance().getRingBufferPool().availableForGet(id_) < samplesToGet)
        return;

    // FIXME
    AudioBuffer micData(samplesToGet, mainBuffFormat);

    const size_t samples = Manager::instance().getRingBufferPool().getData(micData, id_);
    micData.setChannelNum(format_.nb_channels, true); // down/upmix as needed

    if (samples != samplesToGet) {
        SFL_ERR("Asked for %d samples from bindings on call '%s', got %d",
                samplesToGet, id_.c_str(), samples);
        return;
    }

    if (mainBuffFormat.sample_rate != format_.sample_rate)
    {
        if (not resampler_) {
            SFL_DBG("Creating audio resampler");
            resampler_.reset(new Resampler(format_));
        }
        AudioBuffer resampledData(samplesToGet, format_);
        resampler_->resample(micData, resampledData);
        if (audioEncoder_->encode_audio(resampledData) < 0)
            SFL_ERR("encoding failed");
    } else {
        if (audioEncoder_->encode_audio(micData) < 0)
            SFL_ERR("encoding failed");
    }

    const int millisecondsPerPacket = secondsPerPacket_ * 1000;
    if (waitForDataEncode(std::chrono::milliseconds(millisecondsPerPacket))) {
        // Data available !
    }
}

bool
AudioSender::waitForDataEncode(const std::chrono::milliseconds& max_wait) const
{
    auto& mainBuffer = Manager::instance().getRingBufferPool();
    auto mainBuffFormat = mainBuffer.getInternalAudioFormat();
    auto resampleFactor = (double) mainBuffFormat.sample_rate / format_.sample_rate;
    const size_t samplesToGet = resampleFactor * secondsPerPacket_ * format_.sample_rate;

    return mainBuffer.waitForDataAvailable(id_, samplesToGet, max_wait);
}

class AudioReceiveThread
{
    public:
        AudioReceiveThread(const std::string &id, const std::string &sdp);
        ~AudioReceiveThread();
        void addIOContext(ring::video::SocketPair &socketPair);
        void startLoop();

    private:
        NON_COPYABLE(AudioReceiveThread);

        static constexpr auto SDP_FILENAME = "dummyFilename";

        std::map<std::string, std::string> args_;

        static int interruptCb(void *ctx);
        static int readFunction(void *opaque, uint8_t *buf, int buf_size);

        void openDecoder();
        bool decodeFrame();

        /*-----------------------------------------------------------------*/
        /* These variables should be used in thread (i.e. process()) only! */
        /*-----------------------------------------------------------------*/
        const std::string id_;
        std::istringstream stream_;
        std::unique_ptr<ring::video::VideoDecoder> audioDecoder_;
        std::unique_ptr<ring::video::VideoIOHandle> sdpContext_;
        std::unique_ptr<ring::video::VideoIOHandle> demuxContext_;
        std::shared_ptr<ring::RingBuffer> ringbuffer_;

        ThreadLoop loop_;
        bool setup();
        void process();
        void cleanup();
};

AudioReceiveThread::AudioReceiveThread(const std::string& id, const std::string& sdp)
    : id_(id)
    , stream_(sdp)
    , sdpContext_(new VideoIOHandle(sdp.size(), false, &readFunction, 0, 0, this))
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
    audioDecoder_.reset(new ring::video::VideoDecoder());
    audioDecoder_->setInterruptCallback(interruptCb, this);
    // custom_io so the SDP demuxer will not open any UDP connections
    args_["sdp_flags"] = "custom_io";
    EXIT_IF_FAIL(not stream_.str().empty(), "No SDP loaded");
    audioDecoder_->setIOContext(sdpContext_.get());
    audioDecoder_->setOptions(args_);
    EXIT_IF_FAIL(not audioDecoder_->openInput(SDP_FILENAME, "sdp"),
                 "Could not open input \"%s\"", SDP_FILENAME);
    // Now replace our custom AVIOContext with one that will read packets
    audioDecoder_->setIOContext(demuxContext_.get());

    EXIT_IF_FAIL(not audioDecoder_->setupFromAudioData(),
                 "decoder IO startup failed");

    ringbuffer_ = Manager::instance().getRingBufferPool().getRingBuffer(id_);
    return true;
}

void
AudioReceiveThread::process()
{
    ring::AudioFormat mainBuffFormat = Manager::instance().getRingBufferPool().getInternalAudioFormat();
    std::unique_ptr<AVFrame, void(*)(AVFrame*)> decodedFrame(av_frame_alloc(), [](AVFrame*p){av_frame_free(&p);});

    switch (audioDecoder_->decode_audio(decodedFrame.get())) {

        case ring::video::VideoDecoder::Status::FrameFinished:
            audioDecoder_->writeToRingBuffer(decodedFrame.get(), *ringbuffer_,
                                             mainBuffFormat);
            return;

        case ring::video::VideoDecoder::Status::DecodeError:
            SFL_WARN("decoding failure, trying to reset decoder...");
            if (not setup()) {
                SFL_ERR("fatal error, rx thread re-setup failed");
                loop_.stop();
                break;
            }
            if (not audioDecoder_->setupFromAudioData()) {
                SFL_ERR("fatal error, a-decoder setup failed");
                loop_.stop();
                break;
            }
            break;

        case ring::video::VideoDecoder::Status::ReadError:
            SFL_ERR("fatal error, read failed");
            loop_.stop();
            break;

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

AVFormatRtpSession::AVFormatRtpSession(const std::string& id,
                                       const std::map<std::string, std::string>& txArgs)
    : id_(id), txArgs_(txArgs)
{
    // don't move this into the initializer list or Cthulus will emerge
    ringbuffer_ = Manager::instance().getRingBufferPool().createRingBuffer(id_);
}

AVFormatRtpSession::~AVFormatRtpSession()
{
    stop();
}

void
AVFormatRtpSession::updateSDP(const Sdp& sdp)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::string desc(sdp.getIncomingAudioDescription());

    // if port has changed
    if (not desc.empty() and desc != receivingSDP_) {
        receivingSDP_ = desc;
        SFL_WARN("Updated incoming SDP to:\n%s",
                receivingSDP_.c_str());
    }

    if (desc.empty()) {
        SFL_DBG("Audio is inactive");
        receiving_ = false;
        sending_ = false;
    } else if (desc.find("sendrecv") != std::string::npos) {
        SFL_DBG("Sending and receiving audio");
        receiving_ = true;
        sending_ = true;
    } else if (desc.find("inactive") != std::string::npos) {
        SFL_DBG("Audio is inactive");
        receiving_ = false;
        sending_ = false;
    } else if (desc.find("sendonly") != std::string::npos) {
        SFL_DBG("Receiving audio disabled, audio set to sendonly");
        receiving_ = false;
        sending_ = true;
    } else if (desc.find("recvonly") != std::string::npos) {
        SFL_DBG("Sending audio disabled, audio set to recvonly");
        sending_ = false;
        receiving_ = true;
    }
    // even if it says sendrecv or recvonly, our peer may disable audio by
    // setting the port to 0
    if (desc.find("m=audio 0") != std::string::npos) {
        SFL_DBG("Receiving audio disabled, port was set to 0");
        receiving_ = false;
    }

    if (sending_)
        sending_ = sdp.getOutgoingAudioSettings(txArgs_);
}

void
AVFormatRtpSession::updateDestination(const std::string& destination,
                                      unsigned int port)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (destination.empty()) {
        SFL_WARN("Destination is empty, ignoring");
        return;
    }

    std::stringstream tmp;
    tmp << "rtp://" << destination << ":" << port;

    // if destination has changed
    if (tmp.str() != txArgs_["destination"]) {
        if (sender_) {
            SFL_WARN("Audio is already being sent");
            return;
        }
        txArgs_["destination"] = tmp.str();
        SFL_DBG("updated dest to %s", txArgs_["destination"].c_str());
    }

    if (port == 0) {
        SFL_DBG("Sending audio disabled, port was set to 0");
        sending_ = false;
    }
}

void
AVFormatRtpSession::startSender()
{
    if (not sending_)
        return;

    if (sender_)
        SFL_WARN("Restarting audio sender");

    try {
        sender_.reset(new AudioSender(id_, txArgs_, *socketPair_));
    } catch (const VideoEncoderException &e) {
        SFL_ERR("%s", e.what());
        sending_ = false;
    }
}

void
AVFormatRtpSession::startReceiver()
{
    if (receiving_) {
        if (receiveThread_)
            SFL_WARN("restarting video receiver");
        receiveThread_.reset(new AudioReceiveThread(id_, receivingSDP_));
        receiveThread_->addIOContext(*socketPair_);
        receiveThread_->startLoop();
    } else {
        SFL_DBG("Audio receiving disabled");
        receiveThread_.reset();
    }
}

void
AVFormatRtpSession::start(int localPort)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (not sending_ and not receiving_) {
        stop();
        return;
    }

    try {
        socketPair_.reset(new SocketPair(txArgs_["destination"].c_str(), localPort));
    } catch (const std::runtime_error &e) {
        SFL_ERR("Socket creation failed on port %d: %s", localPort, e.what());
        return;
    }

    startSender();
    startReceiver();
}

void
AVFormatRtpSession::start(std::unique_ptr<IceSocket> rtp_sock,
                          std::unique_ptr<IceSocket> rtcp_sock)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (not sending_ and not receiving_) {
        stop();
        return;
    }

    socketPair_.reset(new SocketPair(std::move(rtp_sock), std::move(rtcp_sock)));

    startSender();
    startReceiver();
}

void
AVFormatRtpSession::stop()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (socketPair_)
        socketPair_->interrupt();

    receiveThread_.reset();
    sender_.reset();
    socketPair_.reset();
}

} // end namespace ring
