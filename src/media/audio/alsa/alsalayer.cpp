/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Андрей Лухнов <aol.nnov@gmail.com>
 *  Author: Mohamed Fenjiro <mohamed.fenjiro@savoirfairelinux.com>
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

#include "alsalayer.h"
#include "logger.h"
#include "manager.h"
#include "noncopyable.h"
#include "client/ring_signal.h"
#include "audio/ringbufferpool.h"
#include "audio/ringbuffer.h"
#include "audio/audioloop.h"
#include "libav_utils.h"

#include <fmt/core.h>

#include <thread>
#include <atomic>
#include <chrono>

namespace jami {

AlsaLayer::AlsaLayer(const AudioPreference& pref)
    : AudioLayer(pref)
    , indexIn_(pref.getAlsaCardin())
    , indexOut_(pref.getAlsaCardout())
    , indexRing_(pref.getAlsaCardRingtone())
    , audioPlugin_(pref.getAlsaPlugin())
    , playbackBuff_(0, audioFormat_)
    , captureBuff_(0, audioFormat_)
{
    setHasNativeAEC(false);
    setHasNativeNS(false);
}

AlsaLayer::~AlsaLayer()
{
    status_ = Status::Idle;
    stopThread();

    /* Then close the audio devices */
    closeCaptureStream();
    closePlaybackStream();
    closeRingtoneStream();
}

/**
 * Reimplementation of run()
 */
void
AlsaLayer::run()
{
    if (playbackHandle_)
        playbackChanged(true);
    if (captureHandle_)
        recordChanged(true);

    while (status_ == Status::Started and running_) {
        playback();
        ringtone();
        capture();
    }

    playbackChanged(false);
    recordChanged(false);
}

// Retry approach taken from pa_linux_alsa.c, part of PortAudio
bool
AlsaLayer::openDevice(snd_pcm_t** pcm,
                      const std::string& dev,
                      snd_pcm_stream_t stream,
                      AudioFormat& format)
{
    JAMI_DBG("Alsa: Opening %s device '%s'",
             (stream == SND_PCM_STREAM_CAPTURE) ? "capture" : "playback",
             dev.c_str());

    static const int MAX_RETRIES = 10; // times of 100ms
    int err, tries = 0;
    do {
        err = snd_pcm_open(pcm, dev.c_str(), stream, 0);
        // Retry if busy, since dmix plugin may not have released the device yet
        if (err == -EBUSY) {
            // We're called in audioThread_ context, so if exit is requested
            // force return now
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    } while (err == -EBUSY and ++tries <= MAX_RETRIES);

    if (err < 0) {
        JAMI_ERR("Alsa: couldn't open %s device %s : %s",
                 (stream == SND_PCM_STREAM_CAPTURE)    ? "capture"
                 : (stream == SND_PCM_STREAM_PLAYBACK) ? "playback"
                                                       : "ringtone",
                 dev.c_str(),
                 snd_strerror(err));
        return false;
    }

    if (!alsa_set_params(*pcm, format)) {
        snd_pcm_close(*pcm);
        return false;
    }

    return true;
}

void
AlsaLayer::startStream(AudioDeviceType type)
{
    std::unique_lock<std::mutex> lk(mutex_);
    status_ = Status::Starting;
    stopThread();

    bool dsnop = audioPlugin_ == PCM_DMIX_DSNOOP;

    if (type == AudioDeviceType::PLAYBACK and not is_playback_open_) {
        is_playback_open_ = openDevice(&playbackHandle_,
                                       buildDeviceTopo(dsnop ? PCM_DMIX : audioPlugin_, indexOut_),
                                       SND_PCM_STREAM_PLAYBACK,
                                       audioFormat_);
        if (not is_playback_open_)
            emitSignal<libjami::ConfigurationSignal::Error>(ALSA_PLAYBACK_DEVICE);

        hardwareFormatAvailable(getFormat());
        startPlaybackStream();
    }

    if (type == AudioDeviceType::RINGTONE and getIndexPlayback() != getIndexRingtone()
        and not ringtoneHandle_) {
        if (!openDevice(&ringtoneHandle_,
                        buildDeviceTopo(dsnop ? PCM_DMIX : audioPlugin_, indexRing_),
                        SND_PCM_STREAM_PLAYBACK,
                        audioFormat_))
            emitSignal<libjami::ConfigurationSignal::Error>(ALSA_PLAYBACK_DEVICE);
    }

    if (type == AudioDeviceType::CAPTURE and not is_capture_open_) {
        is_capture_open_ = openDevice(&captureHandle_,
                                      buildDeviceTopo(dsnop ? PCM_DSNOOP : audioPlugin_, indexIn_),
                                      SND_PCM_STREAM_CAPTURE,
                                      audioInputFormat_);

        if (not is_capture_open_)
            emitSignal<libjami::ConfigurationSignal::Error>(ALSA_CAPTURE_DEVICE);
        prepareCaptureStream();
        startCaptureStream();
    }

    status_ = Status::Started;
    startThread();
}

void
AlsaLayer::stopStream(AudioDeviceType stream)
{
    std::unique_lock<std::mutex> lk(mutex_);
    stopThread();

    if (stream == AudioDeviceType::CAPTURE && is_capture_open_) {
        closeCaptureStream();
    }

    if (stream == AudioDeviceType::PLAYBACK && is_playback_open_) {
        closePlaybackStream();
        flushUrgent();
        flushMain();
    }

    if (stream == AudioDeviceType::RINGTONE and ringtoneHandle_) {
        closeRingtoneStream();
    }

    if (is_capture_open_ or is_playback_open_ or ringtoneHandle_) {
        startThread();
    } else {
        status_ = Status::Idle;
    }
}

void
AlsaLayer::startThread()
{
    running_ = true;
    audioThread_ = std::thread(&AlsaLayer::run, this);
}

void
AlsaLayer::stopThread()
{
    running_ = false;
    if (audioThread_.joinable())
        audioThread_.join();
}

/*
 * GCC extension : statement expression
 *
 * ALSA_CALL(function_call, error_string) will:
 *  call the function
 *  display an error if the function failed
 *  return the function return value
 */
#define ALSA_CALL(call, error) \
    ({ \
        int err_code = call; \
        if (err_code < 0) \
            JAMI_ERR(error ": %s", snd_strerror(err_code)); \
        err_code; \
    })

void
AlsaLayer::stopCaptureStream()
{
    if (captureHandle_ && ALSA_CALL(snd_pcm_drop(captureHandle_), "couldn't stop capture") >= 0) {
        is_capture_running_ = false;
        is_capture_prepared_ = false;
    }
}

void
AlsaLayer::closeCaptureStream()
{
    if (is_capture_prepared_ and is_capture_running_)
        stopCaptureStream();

    JAMI_DBG("Alsa: Closing capture stream");
    if (is_capture_open_
        && ALSA_CALL(snd_pcm_close(captureHandle_), "Couldn't close capture") >= 0) {
        is_capture_open_ = false;
        captureHandle_ = nullptr;
    }
}

void
AlsaLayer::startCaptureStream()
{
    if (captureHandle_ and not is_capture_running_)
        if (ALSA_CALL(snd_pcm_start(captureHandle_), "Couldn't start capture") >= 0)
            is_capture_running_ = true;
}

void
AlsaLayer::stopPlaybackStream()
{
    if (playbackHandle_ and is_playback_running_) {
        if (ALSA_CALL(snd_pcm_drop(playbackHandle_), "Couldn't stop playback") >= 0) {
            is_playback_running_ = false;
        }
    }
}

void
AlsaLayer::closePlaybackStream()
{
    if (is_playback_running_)
        stopPlaybackStream();

    if (is_playback_open_) {
        JAMI_DBG("Alsa: Closing playback stream");
        if (ALSA_CALL(snd_pcm_close(playbackHandle_), "Coulnd't close playback") >= 0)
            is_playback_open_ = false;
        playbackHandle_ = nullptr;
    }
}

void
AlsaLayer::closeRingtoneStream()
{
    if (ringtoneHandle_) {
        ALSA_CALL(snd_pcm_drop(ringtoneHandle_), "Couldn't stop ringtone");
        ALSA_CALL(snd_pcm_close(ringtoneHandle_), "Couldn't close ringtone");
        ringtoneHandle_ = nullptr;
    }
}

void
AlsaLayer::startPlaybackStream()
{
    is_playback_running_ = true;
}

void
AlsaLayer::prepareCaptureStream()
{
    if (is_capture_open_ and not is_capture_prepared_)
        if (ALSA_CALL(snd_pcm_prepare(captureHandle_), "Couldn't prepare capture") >= 0)
            is_capture_prepared_ = true;
}

bool
AlsaLayer::alsa_set_params(snd_pcm_t* pcm_handle, AudioFormat& format)
{
#define TRY(call, error) \
    do { \
        if (ALSA_CALL(call, error) < 0) \
            return false; \
    } while (0)

    snd_pcm_hw_params_t* hwparams;
    snd_pcm_hw_params_alloca(&hwparams);

    const unsigned RING_ALSA_PERIOD_SIZE = 160;
    const unsigned RING_ALSA_NB_PERIOD = 8;
    const unsigned RING_ALSA_BUFFER_SIZE = RING_ALSA_PERIOD_SIZE * RING_ALSA_NB_PERIOD;

    snd_pcm_uframes_t period_size = RING_ALSA_PERIOD_SIZE;
    snd_pcm_uframes_t buffer_size = RING_ALSA_BUFFER_SIZE;
    unsigned int periods = RING_ALSA_NB_PERIOD;

    snd_pcm_uframes_t period_size_min = 0;
    snd_pcm_uframes_t period_size_max = 0;
    snd_pcm_uframes_t buffer_size_min = 0;
    snd_pcm_uframes_t buffer_size_max = 0;

#define HW pcm_handle, hwparams /* hardware parameters */
    TRY(snd_pcm_hw_params_any(HW), "hwparams init");

    TRY(snd_pcm_hw_params_set_access(HW, SND_PCM_ACCESS_RW_INTERLEAVED), "access type");
    TRY(snd_pcm_hw_params_set_format(HW, SND_PCM_FORMAT_S16_LE), "sample format");

    TRY(snd_pcm_hw_params_set_rate_resample(HW, 0),
        "hardware sample rate"); /* prevent software resampling */
    TRY(snd_pcm_hw_params_set_rate_near(HW, &format.sample_rate, nullptr), "sample rate");

    // TODO: use snd_pcm_query_chmaps or similar to get hardware channel num
    audioFormat_.nb_channels = 2;
    format.nb_channels = 2;
    TRY(snd_pcm_hw_params_set_channels_near(HW, &format.nb_channels), "channel count");

    snd_pcm_hw_params_get_buffer_size_min(hwparams, &buffer_size_min);
    snd_pcm_hw_params_get_buffer_size_max(hwparams, &buffer_size_max);
    snd_pcm_hw_params_get_period_size_min(hwparams, &period_size_min, nullptr);
    snd_pcm_hw_params_get_period_size_max(hwparams, &period_size_max, nullptr);
    JAMI_DBG("Buffer size range from %lu to %lu", buffer_size_min, buffer_size_max);
    JAMI_DBG("Period size range from %lu to %lu", period_size_min, period_size_max);
    buffer_size = buffer_size > buffer_size_max ? buffer_size_max : buffer_size;
    buffer_size = buffer_size < buffer_size_min ? buffer_size_min : buffer_size;
    period_size = period_size > period_size_max ? period_size_max : period_size;
    period_size = period_size < period_size_min ? period_size_min : period_size;

    TRY(snd_pcm_hw_params_set_buffer_size_near(HW, &buffer_size),
        "Unable to set buffer size for playback");
    TRY(snd_pcm_hw_params_set_period_size_near(HW, &period_size, nullptr),
        "Unable to set period size for playback");
    TRY(snd_pcm_hw_params_set_periods_near(HW, &periods, nullptr),
        "Unable to set number of periods for playback");
    TRY(snd_pcm_hw_params(HW), "hwparams");

    snd_pcm_hw_params_get_buffer_size(hwparams, &buffer_size);
    snd_pcm_hw_params_get_period_size(hwparams, &period_size, nullptr);
    snd_pcm_hw_params_get_rate(hwparams, &format.sample_rate, nullptr);
    snd_pcm_hw_params_get_channels(hwparams, &format.nb_channels);
    JAMI_DBG("Was set period_size = %lu", period_size);
    JAMI_DBG("Was set buffer_size = %lu", buffer_size);

    if (2 * period_size > buffer_size) {
        JAMI_ERR("buffer to small, could not use");
        return false;
    }

#undef HW

    JAMI_DBG("%s using format %s",
             (snd_pcm_stream(pcm_handle) == SND_PCM_STREAM_PLAYBACK) ? "playback" : "capture",
             format.toString().c_str());

    snd_pcm_sw_params_t* swparams = nullptr;
    snd_pcm_sw_params_alloca(&swparams);

#define SW pcm_handle, swparams /* software parameters */
    snd_pcm_sw_params_current(SW);
    TRY(snd_pcm_sw_params_set_start_threshold(SW, period_size * 2), "start threshold");
    TRY(snd_pcm_sw_params(SW), "sw parameters");
#undef SW

    return true;

#undef TRY
}

// TODO first frame causes broken pipe (underrun) because not enough data is sent
// we should wait until the handle is ready
void
AlsaLayer::write(const AudioFrame& buffer, snd_pcm_t* handle)
{
    int err = snd_pcm_writei(handle,
                             (const void*) buffer.pointer()->data[0],
                             buffer.pointer()->nb_samples);

    if (err < 0)
        snd_pcm_recover(handle, err, 0);

    if (err >= 0)
        return;

    switch (err) {
    case -EPIPE:
    case -ESTRPIPE:
    case -EIO: {
        snd_pcm_status_t* status;
        snd_pcm_status_alloca(&status);

        if (ALSA_CALL(snd_pcm_status(handle, status), "Cannot get playback handle status") >= 0)
            if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN) {
                stopPlaybackStream();
                startPlaybackStream();
            }

        ALSA_CALL(snd_pcm_writei(handle,
                                 (const void*) buffer.pointer()->data[0],
                                 buffer.pointer()->nb_samples),
                  "XRUN handling failed");
        break;
    }

    case -EBADFD: {
        snd_pcm_status_t* status;
        snd_pcm_status_alloca(&status);

        if (ALSA_CALL(snd_pcm_status(handle, status), "Cannot get playback handle status") >= 0) {
            if (snd_pcm_status_get_state(status) == SND_PCM_STATE_SETUP) {
                JAMI_ERR("Writing in state SND_PCM_STATE_SETUP, should be "
                         "SND_PCM_STATE_PREPARED or SND_PCM_STATE_RUNNING");
                int error = snd_pcm_prepare(handle);

                if (error < 0) {
                    JAMI_ERR("Failed to prepare handle: %s", snd_strerror(error));
                    stopPlaybackStream();
                }
            }
        }

        break;
    }

    default:
        JAMI_ERR("Unknown write error, dropping frames: %s", snd_strerror(err));
        stopPlaybackStream();
        break;
    }
}

std::unique_ptr<AudioFrame>
AlsaLayer::read(unsigned frames)
{
    if (snd_pcm_state(captureHandle_) == SND_PCM_STATE_XRUN) {
        prepareCaptureStream();
        startCaptureStream();
    }

    auto ret = std::make_unique<AudioFrame>(audioInputFormat_, frames);
    int err = snd_pcm_readi(captureHandle_, ret->pointer()->data[0], frames);

    if (err >= 0) {
        ret->pointer()->nb_samples = err;
        return ret;
    }

    switch (err) {
    case -EPIPE:
    case -ESTRPIPE:
    case -EIO: {
        snd_pcm_status_t* status;
        snd_pcm_status_alloca(&status);

        if (ALSA_CALL(snd_pcm_status(captureHandle_, status), "Get status failed") >= 0)
            if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN) {
                stopCaptureStream();
                prepareCaptureStream();
                startCaptureStream();
            }

        JAMI_ERR("XRUN capture ignored (%s)", snd_strerror(err));
        break;
    }

    case -EPERM:
        JAMI_ERR("Can't capture, EPERM (%s)", snd_strerror(err));
        prepareCaptureStream();
        startCaptureStream();
        break;
    }

    return {};
}

std::string
AlsaLayer::buildDeviceTopo(const std::string& plugin, int card)
{
    if (plugin == PCM_DEFAULT)
        return plugin;

    return fmt::format("{}:{}", plugin, card);
}

static bool
safeUpdate(snd_pcm_t* handle, long& samples)
{
    samples = snd_pcm_avail_update(handle);

    if (samples < 0) {
        samples = snd_pcm_recover(handle, samples, 0);

        if (samples < 0) {
            JAMI_ERR("Got unrecoverable error from snd_pcm_avail_update: %s", snd_strerror(samples));
            return false;
        }
    }

    return true;
}

static std::vector<std::string>
getValues(const std::vector<HwIDPair>& deviceMap)
{
    std::vector<std::string> audioDeviceList;
    audioDeviceList.reserve(deviceMap.size());

    for (const auto& dev : deviceMap)
        audioDeviceList.push_back(dev.second);

    return audioDeviceList;
}

std::vector<std::string>
AlsaLayer::getCaptureDeviceList() const
{
    return getValues(getAudioDeviceIndexMap(true));
}

std::vector<std::string>
AlsaLayer::getPlaybackDeviceList() const
{
    return getValues(getAudioDeviceIndexMap(false));
}

std::vector<HwIDPair>
AlsaLayer::getAudioDeviceIndexMap(bool getCapture) const
{
    snd_ctl_t* handle;
    snd_ctl_card_info_t* info;
    snd_pcm_info_t* pcminfo;
    snd_ctl_card_info_alloca(&info);
    snd_pcm_info_alloca(&pcminfo);

    int numCard = -1;

    std::vector<HwIDPair> audioDevice;

    if (snd_card_next(&numCard) < 0 || numCard < 0)
        return audioDevice;

    do {
        std::string name = fmt::format("hw:{}", numCard);

        if (snd_ctl_open(&handle, name.c_str(), 0) == 0) {
            if (snd_ctl_card_info(handle, info) == 0) {
                snd_pcm_info_set_device(pcminfo, 0);
                snd_pcm_info_set_stream(pcminfo,
                                        getCapture ? SND_PCM_STREAM_CAPTURE
                                                   : SND_PCM_STREAM_PLAYBACK);

                int err;
                if ((err = snd_ctl_pcm_info(handle, pcminfo)) < 0) {
                    JAMI_WARN("Cannot get info for %s %s: %s",
                              getCapture ? "capture device" : "playback device",
                              name.c_str(),
                              snd_strerror(err));
                } else {
                    JAMI_DBG("card %i : %s [%s]",
                             numCard,
                             snd_ctl_card_info_get_id(info),
                             snd_ctl_card_info_get_name(info));
                    std::string description = snd_ctl_card_info_get_name(info);
                    description.append(" - ");
                    description.append(snd_pcm_info_get_name(pcminfo));

                    // The number of the sound card is associated with a string description
                    audioDevice.emplace_back(numCard, std::move(description));
                }
            }

            snd_ctl_close(handle);
        }
    } while (snd_card_next(&numCard) >= 0 && numCard >= 0);

    return audioDevice;
}

bool
AlsaLayer::soundCardIndexExists(int card, AudioDeviceType stream)
{
    std::string name = fmt::format("hw:{}", card);

    snd_ctl_t* handle;
    if (snd_ctl_open(&handle, name.c_str(), 0) != 0)
        return false;

    snd_pcm_info_t* pcminfo;
    snd_pcm_info_alloca(&pcminfo);
    snd_pcm_info_set_stream(pcminfo,
                            stream == AudioDeviceType::PLAYBACK ? SND_PCM_STREAM_PLAYBACK
                                                                : SND_PCM_STREAM_CAPTURE);
    bool ret = snd_ctl_pcm_info(handle, pcminfo) >= 0;
    snd_ctl_close(handle);
    return ret;
}

int
AlsaLayer::getAudioDeviceIndex(const std::string& description, AudioDeviceType type) const
{
    std::vector<HwIDPair> devices = getAudioDeviceIndexMap(type == AudioDeviceType::CAPTURE);

    for (const auto& dev : devices)
        if (dev.second == description)
            return dev.first;

    // else return the default one
    return 0;
}

std::string
AlsaLayer::getAudioDeviceName(int index, AudioDeviceType type) const
{
    // a bit ugly and wrong.. i do not know how to implement it better in alsalayer.
    // in addition, for now it is used in pulselayer only due to alsa and pulse layers api
    // differences. but after some tweaking in alsalayer, it could be used in it too.
    switch (type) {
    case AudioDeviceType::PLAYBACK:
    case AudioDeviceType::RINGTONE:
        return getPlaybackDeviceList().at(index);

    case AudioDeviceType::CAPTURE:
        return getCaptureDeviceList().at(index);
    default:
        // Should never happen
        JAMI_ERR("Unexpected type");
        return "";
    }
}

void
AlsaLayer::capture()
{
    if (!captureHandle_ or !is_capture_running_)
        return;

    snd_pcm_wait(captureHandle_, 10);

    int toGetFrames = snd_pcm_avail_update(captureHandle_);
    if (toGetFrames < 0)
        JAMI_ERR("Audio: Mic error: %s", snd_strerror(toGetFrames));
    if (toGetFrames <= 0)
        return;

    const int framesPerBufferAlsa = 2048;
    toGetFrames = std::min(framesPerBufferAlsa, toGetFrames);
    if (auto r = read(toGetFrames)) {
        putRecorded(std::move(r));
    } else
        JAMI_ERR("ALSA MIC : Couldn't read!");
}

void
AlsaLayer::playback()
{
    if (!playbackHandle_)
        return;

    snd_pcm_wait(playbackHandle_, 20);

    long maxFrames = 0;
    if (not safeUpdate(playbackHandle_, maxFrames))
        return;

    if (auto toPlay = getToPlay(audioFormat_, maxFrames)) {
        write(*toPlay, playbackHandle_);
    }
}

void
AlsaLayer::ringtone()
{
    if (!ringtoneHandle_)
        return;

    long ringtoneAvailFrames = 0;
    if (not safeUpdate(ringtoneHandle_, ringtoneAvailFrames))
        return;

    if (auto toRing = getToRing(audioFormat_, ringtoneAvailFrames)) {
        write(*toRing, ringtoneHandle_);
    }
}

void
AlsaLayer::updatePreference(AudioPreference& preference, int index, AudioDeviceType type)
{
    switch (type) {
    case AudioDeviceType::PLAYBACK:
        preference.setAlsaCardout(index);
        break;

    case AudioDeviceType::CAPTURE:
        preference.setAlsaCardin(index);
        break;

    case AudioDeviceType::RINGTONE:
        preference.setAlsaCardRingtone(index);
        break;

    default:
        break;
    }
}

} // namespace jami
