/*
 *  Copyright (C) 2004-2018 Savoir-faire Linux Inc.
 *
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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

#include "opensllayer.h"

#include "client/ring_signal.h"

#include "audio/resampler.h"
#include "audio/ringbufferpool.h"
#include "audio/ringbuffer.h"
#include "audio/dcblocker.h"
#include "manager.h"
#include "logger.h"
#include "array_size.h"

#include <thread>
#include <chrono>
#include <cstdio>
#include <cassert>
#include <unistd.h>

#include "SLES/OpenSLES_AndroidConfiguration.h"

/* available only from api 14 */
#ifndef SL_ANDROID_RECORDING_PRESET_VOICE_COMMUNICATION
#define SL_ANDROID_RECORDING_PRESET_VOICE_COMMUNICATION ((SLuint32) 0x00000004)
#endif

namespace ring {

// Constructor
OpenSLLayer::OpenSLLayer(const AudioPreference &pref)
    : AudioLayer(pref),
     audioInputDescriptor_(),
     mainRingBuffer_(Manager::instance().getRingBufferPool().getRingBuffer(RingBufferPool::DEFAULT_ID))
{}

// Destructor
OpenSLLayer::~OpenSLLayer()
{
    shutdownAudioEngine();
}

void
OpenSLLayer::init()
{
    initAudioEngine();
    initAudioPlayback();
    initAudioCapture();

    flushMain();
    flushUrgent();
}

void
OpenSLLayer::startStream()
{
    dcblocker_.reset();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (status_ != Status::Idle)
            return;
        status_ = Status::Starting;
    }

    RING_DBG("Start OpenSL audio layer");

    std::vector<int32_t> hw_infos;
    hw_infos.reserve(4);
    emitSignal<DRing::ConfigurationSignal::GetHardwareAudioFormat>(&hw_infos);
    hardwareFormat_ = AudioFormat(hw_infos[0], 1); // Mono on Android
    hardwareBuffSize_ = hw_infos[1];
    hardwareFormatAvailable(hardwareFormat_);

    std::thread launcher([this](){
        init();
        startAudioPlayback();
        startAudioCapture();
        RING_WARN("OpenSL audio layer started");
        {
            std::lock_guard<std::mutex> lock(mutex_);
            status_ = Status::Started;
        }
        startedCv_.notify_all();
    });
    launcher.detach();
}

void
OpenSLLayer::stopStream()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (status_ != Status::Started)
            return;
        status_ = Status::Idle;
    }

    RING_WARN("Stop OpenSL audio layer");

    stopAudioPlayback();
    stopAudioCapture();

    flushMain();
    flushUrgent();

    if (engineObject_ != nullptr) {
        (*engineObject_)->Destroy(engineObject_);
        engineObject_ = nullptr;
        engineInterface_ = nullptr;
    }

    freePlayBufQueue_.clear();
    freeRingBufQueue_.clear();
    playBufQueue_.clear();
    ringBufQueue_.clear();
    freeRecBufQueue_.clear();
    recBufQueue_.clear();
    bufs_.clear();
}

void
OpenSLLayer::initAudioEngine()
{
    SLresult result;

    result = slCreateEngine(&engineObject_, 0, nullptr, 0, nullptr, nullptr);
    SLASSERT(result);

    result = (*engineObject_)->Realize(engineObject_, SL_BOOLEAN_FALSE);
    SLASSERT(result);

    result = (*engineObject_)->GetInterface(engineObject_, SL_IID_ENGINE, &engineInterface_);
    SLASSERT(result);

    uint32_t bufSize = hardwareBuffSize_ * hardwareFormat_.getBytesPerFrame();
    bufs_ = allocateSampleBufs(BUF_COUNT*3, bufSize);
    for(int i=0; i<BUF_COUNT; i++)
        freePlayBufQueue_.push(&bufs_[i]);
    for(int i=BUF_COUNT; i<2*BUF_COUNT; i++)
        freeRingBufQueue_.push(&bufs_[i]);
    for(int i=2*BUF_COUNT; i<3*BUF_COUNT; i++)
        freeRecBufQueue_.push(&bufs_[i]);
}

void
OpenSLLayer::shutdownAudioEngine()
{
    // destroy engine object, and invalidate all associated interfaces
    RING_DBG("Shutdown audio engine");
    stopStream();
}

uint32_t
OpenSLLayer::dbgEngineGetBufCount() {
    uint32_t count_player = player_->dbgGetDevBufCount();
    count_player += freePlayBufQueue_.size();
    count_player += playBufQueue_.size();

    uint32_t count_ringtone = ringtone_->dbgGetDevBufCount();
    count_ringtone += freeRingBufQueue_.size();
    count_ringtone += ringBufQueue_.size();

    RING_ERR("Buf Disrtibutions: PlayerDev=%d, PlayQ=%d, FreePlayQ=%d",
         player_->dbgGetDevBufCount(),
         playBufQueue_.size(),
         freePlayBufQueue_.size());
    RING_ERR("Buf Disrtibutions: RingDev=%d, RingQ=%d, FreeRingQ=%d",
         ringtone_->dbgGetDevBufCount(),
         ringBufQueue_.size(),
         freeRingBufQueue_.size());

    if(count_player != BUF_COUNT) {
        RING_ERR("====Lost Bufs among the queue(supposed = %d, found = %d)",
             BUF_COUNT, count_player);
    }
    return count_player;
}

void
OpenSLLayer::engineServicePlay(bool waiting) {
    if (waiting) {
        playCv.notify_one();
        return;
    }
    sample_buf* buf;
    while (player_ and freePlayBufQueue_.front(&buf)) {
        const AudioBuffer& dat = getToPlay(hardwareFormat_, hardwareBuffSize_);
        if (dat.frames() != 0) {
            buf->size_ = dat.interleave((AudioSample*)buf->buf_) * sizeof(AudioSample);
            if (!playBufQueue_.push(buf)) {
                RING_WARN("playThread player_ PLAY_KICKSTART_BUFFER_COUNT 1");
                break;
            } else
                freePlayBufQueue_.pop();
        } else
            break;
    }
}

void
OpenSLLayer::engineServiceRing(bool waiting) {
    if (waiting) {
        playCv.notify_one();
        return;
    }
    sample_buf* buf;
    while (ringtone_ and freeRingBufQueue_.front(&buf)) {
        freeRingBufQueue_.pop();
        const AudioBuffer& dat = getToRing(hardwareFormat_, hardwareBuffSize_);
        if (dat.frames() != 0) {
            buf->size_ = dat.interleave((AudioSample*)buf->buf_) * sizeof(AudioSample);
            if (!ringBufQueue_.push(buf)) {
                RING_WARN("playThread ringtone_ PLAY_KICKSTART_BUFFER_COUNT 1");
                freeRingBufQueue_.push(buf);
                break;
            }
        } else {
            freeRingBufQueue_.push(buf);
            break;
        }
    }
}

void
OpenSLLayer::engineServiceRec(bool waiting) {
    playCv.notify_one();
    recCv.notify_one();
    return;
}

void
OpenSLLayer::initAudioPlayback()
{
    using namespace std::placeholders;
    std::lock_guard<std::mutex> lck(playMtx);
    player_.reset(new opensl::AudioPlayer(hardwareFormat_, engineInterface_, SL_ANDROID_STREAM_VOICE));
    player_->setBufQueue(&playBufQueue_, &freePlayBufQueue_);
    player_->registerCallback(std::bind(&OpenSLLayer::engineServicePlay, this, _1));

    ringtone_.reset(new opensl::AudioPlayer(hardwareFormat_, engineInterface_, SL_ANDROID_STREAM_VOICE));
    ringtone_->setBufQueue(&ringBufQueue_, &freeRingBufQueue_);
    ringtone_->registerCallback(std::bind(&OpenSLLayer::engineServiceRing, this, _1));
}

void
OpenSLLayer::initAudioCapture()
{
    using namespace std::placeholders;
    std::lock_guard<std::mutex> lck(recMtx);
    recorder_.reset(new opensl::AudioRecorder(hardwareFormat_, engineInterface_));
    recorder_->setBufQueues(&freeRecBufQueue_, &recBufQueue_);
    recorder_->registerCallback(std::bind(&OpenSLLayer::engineServiceRec, this, _1));
}

void
OpenSLLayer::startAudioPlayback()
{
    RING_WARN("Start audio playback");

    player_->start();
    ringtone_->start();
    playThread = std::thread([&]() {
        std::unique_lock<std::mutex> lck(playMtx);
        while (player_ || ringtone_) {
            playCv.wait(lck);
            if (player_ && player_->waiting_) {
                std::lock_guard<std::mutex> lk(player_->m_);
                engineServicePlay(false);
                auto n = playBufQueue_.size();
                if (n >= PLAY_KICKSTART_BUFFER_COUNT)
                    player_->playAudioBuffers(n);
            }
            if (ringtone_ && ringtone_->waiting_) {
                std::lock_guard<std::mutex> lk(ringtone_->m_);
                engineServiceRing(false);
                auto n = ringBufQueue_.size();
                if (n >= PLAY_KICKSTART_BUFFER_COUNT)
                    ringtone_->playAudioBuffers(n);
            }
        }
    });
    RING_WARN("Audio playback started");
}

void
OpenSLLayer::startAudioCapture()
{
    RING_DBG("Start audio capture");

    recorder_->start();
    recThread = std::thread([&]() {
        std::unique_lock<std::mutex> lck(recMtx);
        while (recorder_) {
            recCv.wait(lck);
            while (true) {
                sample_buf *buf;
                if(!recBufQueue_.front(&buf))
                    break;
                recBufQueue_.pop();
                if (buf->size_ > 0) {
                    AudioBuffer dat {(const AudioSample*)buf->buf_, buf->size_ / hardwareFormat_.getBytesPerFrame(), hardwareFormat_};
                    audioCaptureFillBuffer(dat);
                }
                buf->size_ = 0;
                freeRecBufQueue_.push(buf);
            }
        }
    });

    RING_DBG("Audio capture started");
}

void
OpenSLLayer::stopAudioPlayback()
{
    RING_DBG("Stop audio playback");

    {
        std::lock_guard<std::mutex> lck(playMtx);
        if (player_) {
            player_->stop();
            player_.reset();
        }
        if (ringtone_) {
            ringtone_->stop();
            ringtone_.reset();
        }
    }
    if (playThread.joinable()) {
        playCv.notify_all();
        playThread.join();
    }

    RING_DBG("Audio playback stopped");
}

void
OpenSLLayer::stopAudioCapture()
{
    RING_DBG("Stop audio capture");

    {
        std::lock_guard<std::mutex> lck(recMtx);
        recorder_->stop();
        recorder_.reset();
    }
    if (recThread.joinable()) {
        recCv.notify_all();
        recThread.join();
    }

    RING_DBG("Audio capture stopped");
}

std::vector<std::string>
OpenSLLayer::getCaptureDeviceList() const
{
    std::vector<std::string> captureDeviceList;

    // Although OpenSL ES specification allows enumerating
    // available output (and also input) devices, NDK implementation is not mature enough to
    // obtain or select proper one (SLAudioIODeviceCapabilitiesItf, the official interface
    // to obtain such an information)-> SL_FEATURE_UNSUPPORTED

    SLuint32 InputDeviceIDs[MAX_NUMBER_INPUT_DEVICES];
    SLint32 numInputs = 0;
    SLboolean mic_available = SL_BOOLEAN_FALSE;
    SLuint32 mic_deviceID = 0;

    SLresult res;

    // Get the Audio IO DEVICE CAPABILITIES interface, implicit
    RING_DBG("Get the Audio IO DEVICE CAPABILITIES interface, implicit");

    res = (*engineObject_)->GetInterface(engineObject_, SL_IID_AUDIOIODEVICECAPABILITIES, (void*)&AudioIODeviceCapabilitiesItf);
    if (res != SL_RESULT_SUCCESS)
        return captureDeviceList;

    RING_DBG("Get the Audio IO DEVICE CAPABILITIES interface, implicit");
    numInputs = MAX_NUMBER_INPUT_DEVICES;

    res = (*AudioIODeviceCapabilitiesItf)->GetAvailableAudioInputs(AudioIODeviceCapabilitiesItf, &numInputs, InputDeviceIDs);
    if (res != SL_RESULT_SUCCESS)
        return captureDeviceList;

    // Search for either earpiece microphone or headset microphone input
    // device - with a preference for the latter
    for (int i = 0; i < numInputs; i++) {
        res = (*AudioIODeviceCapabilitiesItf)->QueryAudioInputCapabilities(AudioIODeviceCapabilitiesItf,
                                                                           InputDeviceIDs[i],
                                                                           (SLAudioInputDescriptor_ *)&audioInputDescriptor_);
        if (res != SL_RESULT_SUCCESS)
            return captureDeviceList;

        if (audioInputDescriptor_.deviceConnection == SL_DEVCONNECTION_ATTACHED_WIRED and
            audioInputDescriptor_.deviceScope == SL_DEVSCOPE_USER and
            audioInputDescriptor_.deviceLocation == SL_DEVLOCATION_HEADSET) {
            RING_DBG("SL_DEVCONNECTION_ATTACHED_WIRED : mic_deviceID: %d", InputDeviceIDs[i] );
            mic_deviceID = InputDeviceIDs[i];
            mic_available = SL_BOOLEAN_TRUE;
            break;
        } else if (audioInputDescriptor_.deviceConnection == SL_DEVCONNECTION_INTEGRATED and
                   audioInputDescriptor_.deviceScope == SL_DEVSCOPE_USER and
                   audioInputDescriptor_.deviceLocation == SL_DEVLOCATION_HANDSET) {
            RING_DBG("SL_DEVCONNECTION_INTEGRATED : mic_deviceID: %d", InputDeviceIDs[i] );
            mic_deviceID = InputDeviceIDs[i];
            mic_available = SL_BOOLEAN_TRUE;
            break;
        }
    }

    if (!mic_available)
        RING_ERR("No mic available");

    return captureDeviceList;
}

std::vector<std::string>
OpenSLLayer::getPlaybackDeviceList() const
{
    std::vector<std::string> playbackDeviceList;
    return playbackDeviceList;
}

void
OpenSLLayer::updatePreference(AudioPreference& /*preference*/, int /*index*/, DeviceType /*type*/)
{}

void OpenSLLayer::audioCaptureFillBuffer(AudioBuffer &buffer)
{
    RingBufferPool &mbuffer = Manager::instance().getRingBufferPool();
    const AudioFormat mainBufferFormat = mbuffer.getInternalAudioFormat();
    const bool resample = mainBufferFormat.sample_rate != audioFormat_.sample_rate;

    buffer.applyGain(isCaptureMuted_ ? 0.0 : captureGain_);

    if (resample) {
        int outSamples = buffer.frames() * (static_cast<double>(audioFormat_.sample_rate) / mainBufferFormat.sample_rate);
        AudioBuffer out(outSamples, mainBufferFormat);
        resampler_->resample(buffer, out);
        dcblocker_.process(out);
        mainRingBuffer_->put(out);
    } else {
        dcblocker_.process(buffer);
        mainRingBuffer_->put(buffer);
    }
}

void dumpAvailableEngineInterfaces()
{
    SLresult result;
    RING_DBG("Engine Interfaces");
    SLuint32 numSupportedInterfaces;
    result = slQueryNumSupportedEngineInterfaces(&numSupportedInterfaces);
    assert(SL_RESULT_SUCCESS == result);
    result = slQueryNumSupportedEngineInterfaces(NULL);
    assert(SL_RESULT_PARAMETER_INVALID == result);

    RING_DBG("Engine number of supported interfaces %u", numSupportedInterfaces);
    for(SLuint32 i=0; i< numSupportedInterfaces; i++){
        SLInterfaceID  pInterfaceId;
        slQuerySupportedEngineInterfaces(i, &pInterfaceId);
        const char* nm = "unknown iid";

        if (pInterfaceId==SL_IID_NULL) nm="null";
        else if (pInterfaceId==SL_IID_OBJECT) nm="object";
        else if (pInterfaceId==SL_IID_AUDIOIODEVICECAPABILITIES) nm="audiodevicecapabilities";
        else if (pInterfaceId==SL_IID_LED) nm="led";
        else if (pInterfaceId==SL_IID_VIBRA) nm="vibra";
        else if (pInterfaceId==SL_IID_METADATAEXTRACTION) nm="metadataextraction";
        else if (pInterfaceId==SL_IID_METADATATRAVERSAL) nm="metadatatraversal";
        else if (pInterfaceId==SL_IID_DYNAMICSOURCE) nm="dynamicsource";
        else if (pInterfaceId==SL_IID_OUTPUTMIX) nm="outputmix";
        else if (pInterfaceId==SL_IID_PLAY) nm="play";
        else if (pInterfaceId==SL_IID_PREFETCHSTATUS) nm="prefetchstatus";
        else if (pInterfaceId==SL_IID_PLAYBACKRATE) nm="playbackrate";
        else if (pInterfaceId==SL_IID_SEEK) nm="seek";
        else if (pInterfaceId==SL_IID_RECORD) nm="record";
        else if (pInterfaceId==SL_IID_EQUALIZER) nm="equalizer";
        else if (pInterfaceId==SL_IID_VOLUME) nm="volume";
        else if (pInterfaceId==SL_IID_DEVICEVOLUME) nm="devicevolume";
        else if (pInterfaceId==SL_IID_BUFFERQUEUE) nm="bufferqueue";
        else if (pInterfaceId==SL_IID_PRESETREVERB) nm="presetreverb";
        else if (pInterfaceId==SL_IID_ENVIRONMENTALREVERB) nm="environmentalreverb";
        else if (pInterfaceId==SL_IID_EFFECTSEND) nm="effectsend";
        else if (pInterfaceId==SL_IID_3DGROUPING) nm="3dgrouping";
        else if (pInterfaceId==SL_IID_3DCOMMIT) nm="3dcommit";
        else if (pInterfaceId==SL_IID_3DLOCATION) nm="3dlocation";
        else if (pInterfaceId==SL_IID_3DDOPPLER) nm="3ddoppler";
        else if (pInterfaceId==SL_IID_3DSOURCE) nm="3dsource";
        else if (pInterfaceId==SL_IID_3DMACROSCOPIC) nm="3dmacroscopic";
        else if (pInterfaceId==SL_IID_MUTESOLO) nm="mutesolo";
        else if (pInterfaceId==SL_IID_DYNAMICINTERFACEMANAGEMENT) nm="dynamicinterfacemanagement";
        else if (pInterfaceId==SL_IID_MIDIMESSAGE) nm="midimessage";
        else if (pInterfaceId==SL_IID_MIDIMUTESOLO) nm="midimutesolo";
        else if (pInterfaceId==SL_IID_MIDITEMPO) nm="miditempo";
        else if (pInterfaceId==SL_IID_MIDITIME) nm="miditime";
        else if (pInterfaceId==SL_IID_AUDIODECODERCAPABILITIES) nm="audiodecodercapabilities";
        else if (pInterfaceId==SL_IID_AUDIOENCODERCAPABILITIES) nm="audioencodercapabilities";
        else if (pInterfaceId==SL_IID_AUDIOENCODER) nm="audioencoder";
        else if (pInterfaceId==SL_IID_BASSBOOST) nm="bassboost";
        else if (pInterfaceId==SL_IID_PITCH) nm="pitch";
        else if (pInterfaceId==SL_IID_RATEPITCH) nm="ratepitch";
        else if (pInterfaceId==SL_IID_VIRTUALIZER) nm="virtualizer";
        else if (pInterfaceId==SL_IID_VISUALIZATION) nm="visualization";
        else if (pInterfaceId==SL_IID_ENGINE) nm="engine";
        else if (pInterfaceId==SL_IID_ENGINECAPABILITIES) nm="enginecapabilities";
        else if (pInterfaceId==SL_IID_THREADSYNC) nm="theadsync";
        else if (pInterfaceId==SL_IID_ANDROIDEFFECT) nm="androideffect";
        else if (pInterfaceId==SL_IID_ANDROIDEFFECTSEND) nm="androideffectsend";
        else if (pInterfaceId==SL_IID_ANDROIDEFFECTCAPABILITIES) nm="androideffectcapabilities";
        else if (pInterfaceId==SL_IID_ANDROIDCONFIGURATION) nm="androidconfiguration";
        else if (pInterfaceId==SL_IID_ANDROIDSIMPLEBUFFERQUEUE) nm="simplebuferqueue";
        //else if (pInterfaceId==//SL_IID_ANDROIDBUFFERQUEUESOURCE) nm="bufferqueuesource";
        RING_DBG("%s,",nm);
    }
}

} // namespace ring
