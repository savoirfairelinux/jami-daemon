/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
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

#include "audio/ringbufferpool.h"
#include "audio/ringbuffer.h"
#include "audio/dcblocker.h"
#include "libav_utils.h"
#include "manager.h"
#include "logger.h"
#include "array_size.h"

#include <SLES/OpenSLES_AndroidConfiguration.h>

#include <thread>
#include <chrono>
#include <cstdio>
#include <cassert>
#include <unistd.h>

namespace jami {

// Constructor
OpenSLLayer::OpenSLLayer(const AudioPreference& pref)
    : AudioLayer(pref)
{
}

// Destructor
OpenSLLayer::~OpenSLLayer()
{
    shutdownAudioEngine();
}

void
OpenSLLayer::startStream(AudioDeviceType stream)
{
    using namespace std::placeholders;
    if (!engineObject_)
        initAudioEngine();

    std::lock_guard<std::mutex> lock(mutex_);
    JAMI_WARN("Start OpenSL audio layer");

    if (stream == AudioDeviceType::PLAYBACK) {
        if (not player_) {
            try {
                player_.reset(new opensl::AudioPlayer(hardwareFormat_,
                                                      hardwareBuffSize_,
                                                      engineInterface_,
                                                      SL_ANDROID_STREAM_VOICE));
                player_->setBufQueue(&playBufQueue_, &freePlayBufQueue_);
                player_->registerCallback(std::bind(&OpenSLLayer::engineServicePlay, this));
                player_->start();
                playbackChanged(true);
            } catch (const std::exception& e) {
                JAMI_ERR("Error initializing audio playback: %s", e.what());
            }
            if (recorder_)
                startAudioCapture();
        }
    } else if (stream == AudioDeviceType::RINGTONE) {
        if (not ringtone_) {
            try {
                ringtone_.reset(new opensl::AudioPlayer(hardwareFormat_,
                                                        hardwareBuffSize_,
                                                        engineInterface_,
                                                        SL_ANDROID_STREAM_VOICE));
                ringtone_->setBufQueue(&ringBufQueue_, &freeRingBufQueue_);
                ringtone_->registerCallback(std::bind(&OpenSLLayer::engineServiceRing, this));
                ringtone_->start();
            } catch (const std::exception& e) {
                JAMI_ERR("Error initializing ringtone playback: %s", e.what());
            }
        }
    } else if (stream == AudioDeviceType::CAPTURE) {
        if (not recorder_) {
            std::lock_guard<std::mutex> lck(recMtx);
            try {
                recorder_.reset(new opensl::AudioRecorder(hardwareFormat_, hardwareBuffSize_, engineInterface_));
                recorder_->setBufQueues(&freeRecBufQueue_, &recBufQueue_);
                recorder_->registerCallback(std::bind(&OpenSLLayer::engineServiceRec, this));
                setHasNativeAEC(recorder_->hasNativeAEC());
            } catch (const std::exception& e) {
                JAMI_ERR("Error initializing audio capture: %s", e.what());
            }
            if (player_)
                startAudioCapture();
        }
    }
    JAMI_WARN("OpenSL audio layer started");
    status_ = Status::Started;
}

void
OpenSLLayer::stopStream(AudioDeviceType stream)
{
    std::lock_guard<std::mutex> lock(mutex_);
    JAMI_WARN("Stopping OpenSL audio layer for type %u", (unsigned) stream);

    if (stream == AudioDeviceType::PLAYBACK) {
        if (player_) {
            playbackChanged(false);
            player_->stop();
            player_.reset();
        }
    } else if (stream == AudioDeviceType::RINGTONE) {
        if (ringtone_) {
            ringtone_->stop();
            ringtone_.reset();
        }
    } else if (stream == AudioDeviceType::CAPTURE) {
        stopAudioCapture();
    }

    if (not player_ and not ringtone_ and not recorder_)
        status_ = Status::Idle;

    flush();
}

std::vector<sample_buf>
allocateSampleBufs(unsigned count, size_t sizeInByte)
{
    std::vector<sample_buf> bufs;
    if (!count || !sizeInByte)
        return bufs;
    bufs.reserve(count);
    size_t allocSize = (sizeInByte + 3) & ~3; // padding to 4 bytes aligned
    for (unsigned i = 0; i < count; i++)
        bufs.emplace_back(allocSize, sizeInByte);
    return bufs;
}

void
OpenSLLayer::initAudioEngine()
{
    JAMI_WARN("OpenSL init started");
    std::vector<int32_t> hw_infos;
    hw_infos.reserve(4);
    emitSignal<DRing::ConfigurationSignal::GetHardwareAudioFormat>(&hw_infos);
    hardwareFormat_ = AudioFormat(hw_infos[0], 1); // Mono on Android
    hardwareBuffSize_ = hw_infos[1];
    hardwareFormatAvailable(hardwareFormat_, hardwareBuffSize_);

    std::lock_guard<std::mutex> lock(mutex_);
    SLASSERT(slCreateEngine(&engineObject_, 0, nullptr, 0, nullptr, nullptr));
    SLASSERT((*engineObject_)->Realize(engineObject_, SL_BOOLEAN_FALSE));
    SLASSERT((*engineObject_)->GetInterface(engineObject_, SL_IID_ENGINE, &engineInterface_));

    uint32_t bufSize = hardwareBuffSize_ * hardwareFormat_.getBytesPerFrame();
    bufs_ = allocateSampleBufs(BUF_COUNT * 3, bufSize);
    for (int i = 0; i < BUF_COUNT; i++)
        freePlayBufQueue_.push(&bufs_[i]);
    for (int i = BUF_COUNT; i < 2 * BUF_COUNT; i++)
        freeRingBufQueue_.push(&bufs_[i]);
    for (int i = 2 * BUF_COUNT; i < 3 * BUF_COUNT; i++)
        freeRecBufQueue_.push(&bufs_[i]);
    JAMI_WARN("OpenSL init ended");
}

void
OpenSLLayer::shutdownAudioEngine()
{
    JAMI_DBG("Stopping OpenSL");
    stopAudioCapture();

    if (player_) {
        player_->stop();
        player_.reset();
    }
    if (ringtone_) {
        ringtone_->stop();
        ringtone_.reset();
    }

    // destroy engine object, and invalidate all associated interfaces
    JAMI_DBG("Shutdown audio engine");
    if (engineObject_ != nullptr) {
        (*engineObject_)->Destroy(engineObject_);
        engineObject_ = nullptr;
        engineInterface_ = nullptr;
    }

    freeRecBufQueue_.clear();
    recBufQueue_.clear();
    freePlayBufQueue_.clear();
    playBufQueue_.clear();
    freeRingBufQueue_.clear();
    ringBufQueue_.clear();

    startedCv_.notify_all();
    bufs_.clear();
}

uint32_t
OpenSLLayer::dbgEngineGetBufCount()
{
    uint32_t count_player = player_->dbgGetDevBufCount();
    count_player += freePlayBufQueue_.size();
    count_player += playBufQueue_.size();

    uint32_t count_ringtone = ringtone_->dbgGetDevBufCount();
    count_ringtone += freeRingBufQueue_.size();
    count_ringtone += ringBufQueue_.size();

    JAMI_ERR("Buf Disrtibutions: PlayerDev=%d, PlayQ=%d, FreePlayQ=%d",
             player_->dbgGetDevBufCount(),
             playBufQueue_.size(),
             freePlayBufQueue_.size());
    JAMI_ERR("Buf Disrtibutions: RingDev=%d, RingQ=%d, FreeRingQ=%d",
             ringtone_->dbgGetDevBufCount(),
             ringBufQueue_.size(),
             freeRingBufQueue_.size());

    if (count_player != BUF_COUNT) {
        JAMI_ERR("====Lost Bufs among the queue(supposed = %d, found = %d)",
                 BUF_COUNT,
                 count_player);
    }
    return count_player;
}

void
OpenSLLayer::engineServicePlay()
{
    sample_buf* buf;
    while (player_ and freePlayBufQueue_.front(&buf)) {
        if (auto dat = getToPlay(hardwareFormat_, hardwareBuffSize_)) {
            buf->size_ = dat->pointer()->nb_samples * dat->pointer()->channels
                         * sizeof(AudioSample);
            if (buf->size_ > buf->cap_) {
                JAMI_ERR("buf->size_(%zu) > buf->cap_(%zu)", buf->size_, buf->cap_);
                break;
            }
            if (not dat->pointer()->data[0] or not buf->buf_) {
                JAMI_ERR("null bufer %p -> %p %d", dat->pointer()->data[0], buf->buf_, dat->pointer()->nb_samples);
                break;
            }
            //JAMI_ERR("std::copy_n %p -> %p %zu", dat->pointer()->data[0], buf->buf_, dat->pointer()->nb_samples);
            std::copy_n((const AudioSample*) dat->pointer()->data[0],
                        dat->pointer()->nb_samples,
                        (AudioSample*) buf->buf_);
            if (!playBufQueue_.push(buf)) {
                JAMI_WARN("playThread player_ PLAY_KICKSTART_BUFFER_COUNT 1");
                break;
            } else
                freePlayBufQueue_.pop();
        } else {
            break;
        }
    }
}

void
OpenSLLayer::engineServiceRing()
{
    sample_buf* buf;
    while (ringtone_ and freeRingBufQueue_.front(&buf)) {
        freeRingBufQueue_.pop();
        if (auto dat = getToRing(hardwareFormat_, hardwareBuffSize_)) {
            buf->size_ = dat->pointer()->nb_samples * dat->pointer()->channels
                         * sizeof(AudioSample);
            if (buf->size_ > buf->cap_) {
                JAMI_ERR("buf->size_(%zu) > buf->cap_(%zu)", buf->size_, buf->cap_);
                break;
            }
            if (not dat->pointer()->data[0] or not buf->buf_) {
                JAMI_ERR("null bufer %p -> %p %d", dat->pointer()->data[0], buf->buf_, dat->pointer()->nb_samples);
                break;
            }
            std::copy_n((const AudioSample*) dat->pointer()->data[0],
                        dat->pointer()->nb_samples,
                        (AudioSample*) buf->buf_);
            if (!ringBufQueue_.push(buf)) {
                JAMI_WARN("playThread ringtone_ PLAY_KICKSTART_BUFFER_COUNT 1");
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
OpenSLLayer::engineServiceRec()
{
    recCv.notify_one();
    return;
}

void
OpenSLLayer::startAudioCapture()
{
    if (not recorder_)
        return;
    JAMI_DBG("Start audio capture");

    if (recThread.joinable())
        return;
    recThread = std::thread([&]() {
        std::unique_lock<std::mutex> lck(recMtx);
        if (recorder_)
            recorder_->start();
        recordChanged(true);
        while (recorder_) {
            recCv.wait_for(lck, std::chrono::seconds(1));
            if (not recorder_)
                break;
            sample_buf* buf;
            while (recBufQueue_.front(&buf)) {
                recBufQueue_.pop();
                if (buf->size_ > 0) {
                    auto nb_samples = buf->size_ / hardwareFormat_.getBytesPerFrame();
                    auto out = std::make_shared<AudioFrame>(hardwareFormat_, nb_samples);
                    if (isCaptureMuted_)
                        libav_utils::fillWithSilence(out->pointer());
                    else
                        std::copy_n((const AudioSample*) buf->buf_,
                                    nb_samples,
                                    (AudioSample*) out->pointer()->data[0]);
                    putRecorded(std::move(out));
                }
                buf->size_ = 0;
                freeRecBufQueue_.push(buf);
            }
        }
        recordChanged(false);
    });

    JAMI_DBG("Audio capture started");
}

void
OpenSLLayer::stopAudioCapture()
{
    JAMI_DBG("Stop audio capture");

    {
        std::lock_guard<std::mutex> lck(recMtx);
        if (recorder_) {
            recorder_->stop();
            recorder_.reset();
        }
    }
    if (recThread.joinable()) {
        recCv.notify_all();
        recThread.join();
    }

    JAMI_DBG("Audio capture stopped");
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
    SLAudioIODeviceCapabilitiesItf deviceCapabilities {nullptr};
    res = (*engineObject_)
              ->GetInterface(engineObject_,
                             SL_IID_AUDIOIODEVICECAPABILITIES,
                             (void*) &deviceCapabilities);
    if (res != SL_RESULT_SUCCESS)
        return captureDeviceList;

    numInputs = MAX_NUMBER_INPUT_DEVICES;
    res = (*deviceCapabilities)
              ->GetAvailableAudioInputs(deviceCapabilities, &numInputs, InputDeviceIDs);
    if (res != SL_RESULT_SUCCESS)
        return captureDeviceList;

    // Search for either earpiece microphone or headset microphone input
    // device - with a preference for the latter
    for (int i = 0; i < numInputs; i++) {
        SLAudioInputDescriptor audioInputDescriptor_;
        res = (*deviceCapabilities)
                  ->QueryAudioInputCapabilities(deviceCapabilities,
                                                InputDeviceIDs[i],
                                                &audioInputDescriptor_);
        if (res != SL_RESULT_SUCCESS)
            return captureDeviceList;

        if (audioInputDescriptor_.deviceConnection == SL_DEVCONNECTION_ATTACHED_WIRED
            and audioInputDescriptor_.deviceScope == SL_DEVSCOPE_USER
            and audioInputDescriptor_.deviceLocation == SL_DEVLOCATION_HEADSET) {
            JAMI_DBG("SL_DEVCONNECTION_ATTACHED_WIRED : mic_deviceID: %d", InputDeviceIDs[i]);
            mic_deviceID = InputDeviceIDs[i];
            mic_available = SL_BOOLEAN_TRUE;
            break;
        } else if (audioInputDescriptor_.deviceConnection == SL_DEVCONNECTION_INTEGRATED
                   and audioInputDescriptor_.deviceScope == SL_DEVSCOPE_USER
                   and audioInputDescriptor_.deviceLocation == SL_DEVLOCATION_HANDSET) {
            JAMI_DBG("SL_DEVCONNECTION_INTEGRATED : mic_deviceID: %d", InputDeviceIDs[i]);
            mic_deviceID = InputDeviceIDs[i];
            mic_available = SL_BOOLEAN_TRUE;
            break;
        }
    }

    if (!mic_available)
        JAMI_ERR("No mic available");

    return captureDeviceList;
}

std::vector<std::string>
OpenSLLayer::getPlaybackDeviceList() const
{
    std::vector<std::string> playbackDeviceList;
    return playbackDeviceList;
}

void
OpenSLLayer::updatePreference(AudioPreference& /*preference*/,
                              int /*index*/,
                              AudioDeviceType /*type*/)
{}

void
dumpAvailableEngineInterfaces()
{
    SLresult result;
    JAMI_DBG("Engine Interfaces");
    SLuint32 numSupportedInterfaces;
    result = slQueryNumSupportedEngineInterfaces(&numSupportedInterfaces);
    assert(SL_RESULT_SUCCESS == result);
    result = slQueryNumSupportedEngineInterfaces(NULL);
    assert(SL_RESULT_PARAMETER_INVALID == result);

    JAMI_DBG("Engine number of supported interfaces %u", numSupportedInterfaces);
    for (SLuint32 i = 0; i < numSupportedInterfaces; i++) {
        SLInterfaceID pInterfaceId;
        slQuerySupportedEngineInterfaces(i, &pInterfaceId);
        const char* nm = "unknown iid";

        if (pInterfaceId == SL_IID_NULL)
            nm = "null";
        else if (pInterfaceId == SL_IID_OBJECT)
            nm = "object";
        else if (pInterfaceId == SL_IID_AUDIOIODEVICECAPABILITIES)
            nm = "audiodevicecapabilities";
        else if (pInterfaceId == SL_IID_LED)
            nm = "led";
        else if (pInterfaceId == SL_IID_VIBRA)
            nm = "vibra";
        else if (pInterfaceId == SL_IID_METADATAEXTRACTION)
            nm = "metadataextraction";
        else if (pInterfaceId == SL_IID_METADATATRAVERSAL)
            nm = "metadatatraversal";
        else if (pInterfaceId == SL_IID_DYNAMICSOURCE)
            nm = "dynamicsource";
        else if (pInterfaceId == SL_IID_OUTPUTMIX)
            nm = "outputmix";
        else if (pInterfaceId == SL_IID_PLAY)
            nm = "play";
        else if (pInterfaceId == SL_IID_PREFETCHSTATUS)
            nm = "prefetchstatus";
        else if (pInterfaceId == SL_IID_PLAYBACKRATE)
            nm = "playbackrate";
        else if (pInterfaceId == SL_IID_SEEK)
            nm = "seek";
        else if (pInterfaceId == SL_IID_RECORD)
            nm = "record";
        else if (pInterfaceId == SL_IID_EQUALIZER)
            nm = "equalizer";
        else if (pInterfaceId == SL_IID_VOLUME)
            nm = "volume";
        else if (pInterfaceId == SL_IID_DEVICEVOLUME)
            nm = "devicevolume";
        else if (pInterfaceId == SL_IID_BUFFERQUEUE)
            nm = "bufferqueue";
        else if (pInterfaceId == SL_IID_PRESETREVERB)
            nm = "presetreverb";
        else if (pInterfaceId == SL_IID_ENVIRONMENTALREVERB)
            nm = "environmentalreverb";
        else if (pInterfaceId == SL_IID_EFFECTSEND)
            nm = "effectsend";
        else if (pInterfaceId == SL_IID_3DGROUPING)
            nm = "3dgrouping";
        else if (pInterfaceId == SL_IID_3DCOMMIT)
            nm = "3dcommit";
        else if (pInterfaceId == SL_IID_3DLOCATION)
            nm = "3dlocation";
        else if (pInterfaceId == SL_IID_3DDOPPLER)
            nm = "3ddoppler";
        else if (pInterfaceId == SL_IID_3DSOURCE)
            nm = "3dsource";
        else if (pInterfaceId == SL_IID_3DMACROSCOPIC)
            nm = "3dmacroscopic";
        else if (pInterfaceId == SL_IID_MUTESOLO)
            nm = "mutesolo";
        else if (pInterfaceId == SL_IID_DYNAMICINTERFACEMANAGEMENT)
            nm = "dynamicinterfacemanagement";
        else if (pInterfaceId == SL_IID_MIDIMESSAGE)
            nm = "midimessage";
        else if (pInterfaceId == SL_IID_MIDIMUTESOLO)
            nm = "midimutesolo";
        else if (pInterfaceId == SL_IID_MIDITEMPO)
            nm = "miditempo";
        else if (pInterfaceId == SL_IID_MIDITIME)
            nm = "miditime";
        else if (pInterfaceId == SL_IID_AUDIODECODERCAPABILITIES)
            nm = "audiodecodercapabilities";
        else if (pInterfaceId == SL_IID_AUDIOENCODERCAPABILITIES)
            nm = "audioencodercapabilities";
        else if (pInterfaceId == SL_IID_AUDIOENCODER)
            nm = "audioencoder";
        else if (pInterfaceId == SL_IID_BASSBOOST)
            nm = "bassboost";
        else if (pInterfaceId == SL_IID_PITCH)
            nm = "pitch";
        else if (pInterfaceId == SL_IID_RATEPITCH)
            nm = "ratepitch";
        else if (pInterfaceId == SL_IID_VIRTUALIZER)
            nm = "virtualizer";
        else if (pInterfaceId == SL_IID_VISUALIZATION)
            nm = "visualization";
        else if (pInterfaceId == SL_IID_ENGINE)
            nm = "engine";
        else if (pInterfaceId == SL_IID_ENGINECAPABILITIES)
            nm = "enginecapabilities";
        else if (pInterfaceId == SL_IID_THREADSYNC)
            nm = "theadsync";
        else if (pInterfaceId == SL_IID_ANDROIDEFFECT)
            nm = "androideffect";
        else if (pInterfaceId == SL_IID_ANDROIDEFFECTSEND)
            nm = "androideffectsend";
        else if (pInterfaceId == SL_IID_ANDROIDEFFECTCAPABILITIES)
            nm = "androideffectcapabilities";
        else if (pInterfaceId == SL_IID_ANDROIDCONFIGURATION)
            nm = "androidconfiguration";
        else if (pInterfaceId == SL_IID_ANDROIDSIMPLEBUFFERQUEUE)
            nm = "simplebuferqueue";
        // else if (pInterfaceId==//SL_IID_ANDROIDBUFFERQUEUESOURCE) nm="bufferqueuesource";
        JAMI_DBG("%s,", nm);
    }
}

} // namespace jami
