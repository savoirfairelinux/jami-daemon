/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Андрей Лухнов <aol.nnov@gmail.com>
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

#include "alsalayer.h"
#include "logger.h"
#include "manager.h"
#include "noncopyable.h"
#include "client/configurationmanager.h"

#include <thread>
#include <atomic>

class AlsaThread {
    public:
        AlsaThread(AlsaLayer *alsa);
        ~AlsaThread();
        void initAudioLayer();
        void start();
        bool isRunning() const;

    private:
        void run();

        NON_COPYABLE(AlsaThread);
        std::thread thread_;
        AlsaLayer* alsa_;
        std::atomic<bool> running_;
};

AlsaThread::AlsaThread(AlsaLayer *alsa)
    : thread_(), alsa_(alsa), running_(false)
{}

bool AlsaThread::isRunning() const
{
    return running_;
}

AlsaThread::~AlsaThread()
{
    running_ = false;
    if (thread_.joinable())
        thread_.join();
}

void AlsaThread::start()
{
    running_ = true;
    thread_ = std::thread(&AlsaThread::run, this);
}

void AlsaThread::initAudioLayer(void)
{
    std::string pcmp;
    std::string pcmr;
    std::string pcmc;

    if (alsa_->audioPlugin_ == PCM_DMIX_DSNOOP) {
        pcmp = alsa_->buildDeviceTopo(PCM_DMIX, alsa_->indexOut_);
        pcmr = alsa_->buildDeviceTopo(PCM_DMIX, alsa_->indexRing_);
        pcmc = alsa_->buildDeviceTopo(PCM_DSNOOP, alsa_->indexIn_);
    } else {
        pcmp = alsa_->buildDeviceTopo(alsa_->audioPlugin_, alsa_->indexOut_);
        pcmr = alsa_->buildDeviceTopo(alsa_->audioPlugin_, alsa_->indexRing_);
        pcmc = alsa_->buildDeviceTopo(alsa_->audioPlugin_, alsa_->indexIn_);
    }

    if (not alsa_->is_capture_open_) {
        alsa_->is_capture_open_ = alsa_->openDevice(&alsa_->captureHandle_, pcmc, SND_PCM_STREAM_CAPTURE);

        if (not alsa_->is_capture_open_)
            Manager::instance().getClient()->getConfigurationManager()->errorAlert(ALSA_CAPTURE_DEVICE);
    }

    if (not alsa_->is_playback_open_) {
        alsa_->is_playback_open_ = alsa_->openDevice(&alsa_->playbackHandle_, pcmp, SND_PCM_STREAM_PLAYBACK);

        if (not alsa_->is_playback_open_)
            Manager::instance().getClient()->getConfigurationManager()->errorAlert(ALSA_PLAYBACK_DEVICE);

        if (alsa_->getIndexPlayback() != alsa_->getIndexRingtone())
            if (!alsa_->openDevice(&alsa_->ringtoneHandle_, pcmr, SND_PCM_STREAM_PLAYBACK))
                Manager::instance().getClient()->getConfigurationManager()->errorAlert(ALSA_PLAYBACK_DEVICE);
    }

    alsa_->hardwareFormatAvailable(alsa_->getFormat());

    alsa_->prepareCaptureStream();
    alsa_->preparePlaybackStream();

    alsa_->startCaptureStream();
    alsa_->startPlaybackStream();

    alsa_->flushMain();
    alsa_->flushUrgent();
}

/**
 * Reimplementation of run()
 */
void AlsaThread::run()
{
    initAudioLayer();
    alsa_->isStarted_ = true;

    while (alsa_->isStarted_ and running_) {
        alsa_->audioCallback();
    }
}

AlsaLayer::AlsaLayer(const AudioPreference &pref)
    : AudioLayer(pref)
    , indexIn_(pref.getAlsaCardin())
    , indexOut_(pref.getAlsaCardout())
    , indexRing_(pref.getAlsaCardring())
    , playbackHandle_(nullptr)
    , ringtoneHandle_(nullptr)
    , captureHandle_(nullptr)
    , audioPlugin_(pref.getAlsaPlugin())
    , playbackBuff_(0, audioFormat_)
    , captureBuff_(0, audioFormat_)
    , playbackIBuff_(1024)
    , captureIBuff_(1024)
    , is_playback_prepared_(false)
    , is_capture_prepared_(false)
    , is_playback_running_(false)
    , is_capture_running_(false)
    , is_playback_open_(false)
    , is_capture_open_(false)
    , audioThread_(nullptr)
{}

AlsaLayer::~AlsaLayer()
{
    isStarted_ = false;
    delete audioThread_;

    /* Then close the audio devices */
    closeCaptureStream();
    closePlaybackStream();
}

// Retry approach taken from pa_linux_alsa.c, part of PortAudio
bool AlsaLayer::openDevice(snd_pcm_t **pcm, const std::string &dev, snd_pcm_stream_t stream)
{
    DEBUG("Alsa: Opening %s",  dev.c_str());

    static const int MAX_RETRIES = 100;
    int err = snd_pcm_open(pcm, dev.c_str(), stream, 0);

    // Retry if busy, since dmix plugin may not have released the device yet
    for (int tries = 0; tries < MAX_RETRIES and err == -EBUSY; ++tries) {
        const struct timespec req = {0, 100000000L};
        nanosleep(&req, 0);
        err = snd_pcm_open(pcm, dev.c_str(), stream, 0);
    }

    if (err < 0) {
        ERROR("Alsa: couldn't open device %s : %s",  dev.c_str(),
              snd_strerror(err));
        return false;
    }

    if (!alsa_set_params(*pcm)) {
        snd_pcm_close(*pcm);
        return false;
    }

    return true;
}

void
AlsaLayer::startStream()
{
    dcblocker_.reset();

    if (is_playback_running_ and is_capture_running_)
        return;

    if (audioThread_ == NULL) {
        audioThread_ = new AlsaThread(this);
        audioThread_->start();
    } else if (!audioThread_->isRunning()) {
        audioThread_->start();
    }
}

void
AlsaLayer::stopStream()
{
    isStarted_ = false;

    delete audioThread_;
    audioThread_ = NULL;

    closeCaptureStream();
    closePlaybackStream();

    playbackHandle_ = NULL;
    captureHandle_ = NULL;
    ringtoneHandle_ = NULL;

    /* Flush the ring buffers */
    flushUrgent();
    flushMain();
}

/*
 * GCC extension : statement expression
 *
 * ALSA_CALL(function_call, error_string) will:
 * 		call the function
 * 		display an error if the function failed
 * 		return the function return value
 */
#define ALSA_CALL(call, error) ({ \
			int err_code = call; \
			if (err_code < 0) \
				ERROR(error ": %s", snd_strerror(err_code)); \
			err_code; \
		})

void AlsaLayer::stopCaptureStream()
{
    if (captureHandle_ && ALSA_CALL(snd_pcm_drop(captureHandle_), "couldn't stop capture") >= 0) {
        is_capture_running_ = false;
        is_capture_prepared_ = false;
    }
}

void AlsaLayer::closeCaptureStream()
{
    if (is_capture_prepared_ and is_capture_running_)
        stopCaptureStream();

    if (is_capture_open_ && ALSA_CALL(snd_pcm_close(captureHandle_), "Couldn't close capture") >= 0)
        is_capture_open_ = false;
}

void AlsaLayer::startCaptureStream()
{
    if (captureHandle_ and not is_capture_running_)
        if (ALSA_CALL(snd_pcm_start(captureHandle_), "Couldn't start capture") >= 0)
            is_capture_running_ = true;
}

void AlsaLayer::stopPlaybackStream()
{
    if (ringtoneHandle_ and is_playback_running_)
        ALSA_CALL(snd_pcm_drop(ringtoneHandle_), "Couldn't stop ringtone");

    if (playbackHandle_ and is_playback_running_) {
        if (ALSA_CALL(snd_pcm_drop(playbackHandle_), "Couldn't stop playback") >= 0) {
            is_playback_running_ = false;
            is_playback_prepared_ = false;
        }
    }
}


void AlsaLayer::closePlaybackStream()
{
    if (is_playback_prepared_ and is_playback_running_)
        stopPlaybackStream();

    if (is_playback_open_) {
        if (ringtoneHandle_)
            ALSA_CALL(snd_pcm_close(ringtoneHandle_), "Couldn't stop ringtone");

        if (ALSA_CALL(snd_pcm_close(playbackHandle_), "Coulnd't close playback") >= 0)
            is_playback_open_ = false;
    }

}

void AlsaLayer::startPlaybackStream()
{
    is_playback_running_ = true;
}

void AlsaLayer::prepareCaptureStream()
{
    if (is_capture_open_ and not is_capture_prepared_)
        if (ALSA_CALL(snd_pcm_prepare(captureHandle_), "Couldn't prepare capture") >= 0)
            is_capture_prepared_ = true;
}

void AlsaLayer::preparePlaybackStream()
{
    is_playback_prepared_ = true;
}

bool AlsaLayer::alsa_set_params(snd_pcm_t *pcm_handle)
{
#define TRY(call, error) do { \
		if (ALSA_CALL(call, error) < 0) \
			return false; \
	} while(0)

    snd_pcm_hw_params_t *hwparams;
    snd_pcm_hw_params_alloca(&hwparams);

    const unsigned SFL_ALSA_PERIOD_SIZE = 160;
    const unsigned SFL_ALSA_NB_PERIOD = 8;
    const unsigned SFL_ALSA_BUFFER_SIZE = SFL_ALSA_PERIOD_SIZE * SFL_ALSA_NB_PERIOD;

    snd_pcm_uframes_t period_size = SFL_ALSA_PERIOD_SIZE;
    snd_pcm_uframes_t buffer_size = SFL_ALSA_BUFFER_SIZE;
    unsigned int periods = SFL_ALSA_NB_PERIOD;

    snd_pcm_uframes_t  period_size_min = 0;
    snd_pcm_uframes_t  period_size_max = 0;
    snd_pcm_uframes_t  buffer_size_min = 0;
    snd_pcm_uframes_t  buffer_size_max = 0;

#define HW pcm_handle, hwparams /* hardware parameters */
    TRY(snd_pcm_hw_params_any(HW), "hwparams init");

    TRY(snd_pcm_hw_params_set_access(HW, SND_PCM_ACCESS_RW_INTERLEAVED), "access type");
    TRY(snd_pcm_hw_params_set_format(HW, SND_PCM_FORMAT_S16_LE), "sample format");

    TRY(snd_pcm_hw_params_set_rate_resample(HW, 0), "hardware sample rate"); /* prevent software resampling */
    TRY(snd_pcm_hw_params_set_rate_near(HW, &audioFormat_.sample_rate, nullptr), "sample rate");

    // TODO: use snd_pcm_query_chmaps or similar to get hardware channel num
    audioFormat_.nb_channels = 2;
    TRY(snd_pcm_hw_params_set_channels_near(HW, &audioFormat_.nb_channels), "channel count");

    snd_pcm_hw_params_get_buffer_size_min(hwparams, &buffer_size_min);
    snd_pcm_hw_params_get_buffer_size_max(hwparams, &buffer_size_max);
    snd_pcm_hw_params_get_period_size_min(hwparams, &period_size_min, nullptr);
    snd_pcm_hw_params_get_period_size_max(hwparams, &period_size_max, nullptr);
    DEBUG("Buffer size range from %lu to %lu", buffer_size_min, buffer_size_max);
    DEBUG("Period size range from %lu to %lu", period_size_min, period_size_max);
    buffer_size = buffer_size > buffer_size_max ? buffer_size_max : buffer_size;
    buffer_size = buffer_size < buffer_size_min ? buffer_size_min : buffer_size;
    period_size = period_size > period_size_max ? period_size_max : period_size;
    period_size = period_size < period_size_min ? period_size_min : period_size;

    TRY(snd_pcm_hw_params_set_buffer_size_near(HW, &buffer_size), "Unable to set buffer size for playback");
    TRY(snd_pcm_hw_params_set_period_size_near(HW, &period_size, nullptr), "Unable to set period size for playback");
    TRY(snd_pcm_hw_params_set_periods_near(HW, &periods, nullptr), "Unable to set number of periods for playback");
    TRY(snd_pcm_hw_params(HW), "hwparams");

    snd_pcm_hw_params_get_buffer_size(hwparams, &buffer_size);
    snd_pcm_hw_params_get_period_size(hwparams, &period_size, nullptr);
    snd_pcm_hw_params_get_rate(hwparams, &audioFormat_.sample_rate, nullptr);
    snd_pcm_hw_params_get_channels(hwparams, &audioFormat_.nb_channels);
    DEBUG("Was set period_size = %lu", period_size);
    DEBUG("Was set buffer_size = %lu", buffer_size);

    if (2 * period_size > buffer_size) {
        ERROR("buffer to small, could not use");
        return false;
    }

#undef HW

    DEBUG("%s using format %s",
          (snd_pcm_stream(pcm_handle) == SND_PCM_STREAM_PLAYBACK) ? "playback" : "capture",
          audioFormat_.toString().c_str() );

    snd_pcm_sw_params_t *swparams = NULL;
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
AlsaLayer::write(SFLAudioSample* buffer, int frames, snd_pcm_t * handle)
{
    // Skip empty buffers
    if (!frames)
        return;

    int err = snd_pcm_writei(handle, (const void*)buffer, frames);

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
                    preparePlaybackStream();
                    startPlaybackStream();
                }

            ALSA_CALL(snd_pcm_writei(handle, (const void*)buffer, frames), "XRUN handling failed");
            break;
        }

        case -EBADFD: {
            snd_pcm_status_t* status;
            snd_pcm_status_alloca(&status);

            if (ALSA_CALL(snd_pcm_status(handle, status), "Cannot get playback handle status") >= 0) {
                if (snd_pcm_status_get_state(status) == SND_PCM_STATE_SETUP) {
                    ERROR("Writing in state SND_PCM_STATE_SETUP, should be "
                          "SND_PCM_STATE_PREPARED or SND_PCM_STATE_RUNNING");
                    int error = snd_pcm_prepare(handle);

                    if (error < 0) {
                        ERROR("Failed to prepare handle: %s", snd_strerror(error));
                        stopPlaybackStream();
                    }
                }
            }

            break;
        }

        default:
            ERROR("Unknown write error, dropping frames: %s", snd_strerror(err));
            stopPlaybackStream();
            break;
    }
}

int
AlsaLayer::read(SFLAudioSample* buffer, int frames)
{
    if (snd_pcm_state(captureHandle_) == SND_PCM_STATE_XRUN) {
        prepareCaptureStream();
        startCaptureStream();
    }

    int err = snd_pcm_readi(captureHandle_, (void*)buffer, frames);

    if (err >= 0)
        return err;

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

            ERROR("XRUN capture ignored (%s)", snd_strerror(err));
            break;
        }

        case -EPERM:
            ERROR("Can't capture, EPERM (%s)", snd_strerror(err));
            prepareCaptureStream();
            startCaptureStream();
            break;
    }

    return 0;
}

std::string
AlsaLayer::buildDeviceTopo(const std::string &plugin, int card)
{
    std::stringstream ss;
    std::string pcm(plugin);

    if (pcm == PCM_DEFAULT)
        return pcm;

    ss << ":" << card;

    return pcm + ss.str();
}

static bool
safeUpdate(snd_pcm_t *handle, int &samples)
{
    samples = snd_pcm_avail_update(handle);

    if (samples < 0) {
        samples = snd_pcm_recover(handle, samples, 0);

        if (samples < 0) {
            ERROR("Got unrecoverable error from snd_pcm_avail_update: %s", snd_strerror(samples));
            return false;
        }
    }

    return true;
}

static std::vector<std::string>
getValues(const std::vector<HwIDPair> &deviceMap)
{
    std::vector<std::string> audioDeviceList;

    for (const auto & dev : deviceMap)
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
    snd_ctl_card_info_t *info;
    snd_pcm_info_t* pcminfo;
    snd_ctl_card_info_alloca(&info);
    snd_pcm_info_alloca(&pcminfo);

    int numCard = -1;

    std::vector<HwIDPair> audioDevice;

    if (snd_card_next(&numCard) < 0 || numCard < 0)
        return audioDevice;

    do {
        std::stringstream ss;
        ss << numCard;
        std::string name = "hw:" + ss.str();

        if (snd_ctl_open(&handle, name.c_str(), 0) == 0) {
            if (snd_ctl_card_info(handle, info) == 0) {
                snd_pcm_info_set_device(pcminfo, 0);
                snd_pcm_info_set_stream(pcminfo, getCapture ? SND_PCM_STREAM_CAPTURE : SND_PCM_STREAM_PLAYBACK);

                int err;

                if ((err = snd_ctl_pcm_info(handle, pcminfo)) < 0) {
                    WARN("Cannot get info for %s %s: %s", getCapture ?
                         "capture device" : "playback device", name.c_str(),
                         snd_strerror(err));
                } else {
                    DEBUG("card %i : %s [%s]",
                          numCard,
                          snd_ctl_card_info_get_id(info),
                          snd_ctl_card_info_get_name(info));
                    std::string description = snd_ctl_card_info_get_name(info);
                    description.append(" - ");
                    description.append(snd_pcm_info_get_name(pcminfo));

                    // The number of the sound card is associated with a string description
                    audioDevice.push_back(HwIDPair(numCard, description));
                }
            }

            snd_ctl_close(handle);
        }
    } while (snd_card_next(&numCard) >= 0 && numCard >= 0);


    return audioDevice;
}


bool
AlsaLayer::soundCardIndexExists(int card, DeviceType stream)
{
    snd_pcm_info_t *pcminfo;
    snd_pcm_info_alloca(&pcminfo);
    std::string name("hw:");
    std::stringstream ss;
    ss << card;
    name.append(ss.str());

    snd_ctl_t* handle;

    if (snd_ctl_open(&handle, name.c_str(), 0) != 0)
        return false;

    snd_pcm_info_set_stream(pcminfo, stream == DeviceType::PLAYBACK ?  SND_PCM_STREAM_PLAYBACK : SND_PCM_STREAM_CAPTURE);
    bool ret = snd_ctl_pcm_info(handle, pcminfo) >= 0;
    snd_ctl_close(handle);
    return ret;
}

int
AlsaLayer::getAudioDeviceIndex(const std::string &description, DeviceType type) const
{
    std::vector<HwIDPair> devices = getAudioDeviceIndexMap(type == DeviceType::CAPTURE);

    for (const auto & dev : devices)
        if (dev.second == description)
            return dev.first;

    // else return the default one
    return 0;
}

std::string
AlsaLayer::getAudioDeviceName(int index, DeviceType type) const
{
    // a bit ugly and wrong.. i do not know how to implement it better in alsalayer.
    // in addition, for now it is used in pulselayer only due to alsa and pulse layers api differences.
    // but after some tweaking in alsalayer, it could be used in it too.
    switch (type) {
        case DeviceType::PLAYBACK:
        case DeviceType::RINGTONE:
            return getPlaybackDeviceList().at(index);

        case DeviceType::CAPTURE:
            return getCaptureDeviceList().at(index);
        default:
            // Should never happen
            ERROR("Unexpected type");
            return "";
    }
}

void AlsaLayer::capture()
{
    AudioFormat mainBufferFormat = Manager::instance().getMainBuffer().getInternalAudioFormat();

    int toGetFrames = snd_pcm_avail_update(captureHandle_);

    if (toGetFrames < 0)
        ERROR("Audio: Mic error: %s", snd_strerror(toGetFrames));

    if (toGetFrames <= 0)
        return;

    const int framesPerBufferAlsa = 2048;
    toGetFrames = std::min(framesPerBufferAlsa, toGetFrames);
    captureIBuff_.resize(toGetFrames * audioFormat_.nb_channels);

    if (read(captureIBuff_.data(), toGetFrames) != toGetFrames) {
        ERROR("ALSA MIC : Couldn't read!");
        return;
    }

    captureBuff_.deinterleave(captureIBuff_, audioFormat_);
    captureBuff_.applyGain(isCaptureMuted_ ? 0.0 : captureGain_);

    if (audioFormat_.nb_channels != mainBufferFormat.nb_channels) {
        captureBuff_.setChannelNum(mainBufferFormat.nb_channels, true);
    }
    if (audioFormat_.sample_rate != mainBufferFormat.sample_rate) {
        int outFrames = toGetFrames * (static_cast<double>(audioFormat_.sample_rate) / mainBufferFormat.sample_rate);
        AudioBuffer rsmpl_in(outFrames, mainBufferFormat);
        resampler_.resample(captureBuff_, rsmpl_in);
        dcblocker_.process(rsmpl_in);
        Manager::instance().getMainBuffer().putData(rsmpl_in, MainBuffer::DEFAULT_ID);
    } else {
        dcblocker_.process(captureBuff_);
        Manager::instance().getMainBuffer().putData(captureBuff_, MainBuffer::DEFAULT_ID);
    }
}

void AlsaLayer::playback(int maxFrames)
{
    unsigned framesToGet = Manager::instance().getMainBuffer().availableForGet(MainBuffer::DEFAULT_ID);

    // no audio available, play tone or silence
    if (framesToGet <= 0) {
        // FIXME: not thread safe! we only lock the mutex when we get the
        // pointer, we have no guarantee that it will stay safe to use
        AudioLoop *tone = Manager::instance().getTelephoneTone();
        AudioLoop *file_tone = Manager::instance().getTelephoneFile();

        playbackBuff_.setFormat(audioFormat_);
        playbackBuff_.resize(maxFrames);

        if (tone)
            tone->getNext(playbackBuff_, playbackGain_);
        else if (file_tone && !ringtoneHandle_)
            file_tone->getNext(playbackBuff_, playbackGain_);
        else
            playbackBuff_.reset();

        playbackBuff_.interleave(playbackIBuff_);
        write(playbackIBuff_.data(), playbackBuff_.frames(), playbackHandle_);
    } else {
        // play the regular sound samples
        const AudioFormat mainBufferFormat = Manager::instance().getMainBuffer().getInternalAudioFormat();
        const bool resample = audioFormat_.sample_rate != mainBufferFormat.sample_rate;

        double resampleFactor = 1.0;
        unsigned maxNbFramesToGet = maxFrames;

        if (resample) {
            resampleFactor = static_cast<double>(audioFormat_.sample_rate) / mainBufferFormat.sample_rate;
            maxNbFramesToGet = maxFrames / resampleFactor;
        }

        framesToGet = std::min(framesToGet, maxNbFramesToGet);

        playbackBuff_.setFormat(mainBufferFormat);
        playbackBuff_.resize(framesToGet);

        Manager::instance().getMainBuffer().getData(playbackBuff_, MainBuffer::DEFAULT_ID);
        playbackBuff_.applyGain(isPlaybackMuted_ ? 0.0 : playbackGain_);

        if (audioFormat_.nb_channels != mainBufferFormat.nb_channels) {
            playbackBuff_.setChannelNum(audioFormat_.nb_channels, true);
        }
        if (resample) {
            const size_t outFrames = framesToGet * resampleFactor;
            AudioBuffer rsmpl_out(outFrames, audioFormat_);
            resampler_.resample(playbackBuff_, rsmpl_out);
            rsmpl_out.interleave(playbackIBuff_);
            write(playbackIBuff_.data(), outFrames, playbackHandle_);
        } else {
            playbackBuff_.interleave(playbackIBuff_);
            write(playbackIBuff_.data(), framesToGet, playbackHandle_);
        }
    }
}

void AlsaLayer::audioCallback()
{
    if (!playbackHandle_ or !captureHandle_)
        return;

    notifyIncomingCall();

    snd_pcm_wait(playbackHandle_, 20);

    int playbackAvailFrames = 0;

    if (not safeUpdate(playbackHandle_, playbackAvailFrames))
        return;

    unsigned framesToGet = urgentRingBuffer_.availableForGet(MainBuffer::DEFAULT_ID);

    if (framesToGet > 0) {
        // Urgent data (dtmf, incoming call signal) come first.
        framesToGet = std::min(framesToGet, (unsigned)playbackAvailFrames);
        playbackBuff_.setFormat(audioFormat_);
        playbackBuff_.resize(framesToGet);
        urgentRingBuffer_.get(playbackBuff_, MainBuffer::DEFAULT_ID);
        playbackBuff_.applyGain(isPlaybackMuted_ ? 0.0 : playbackGain_);
        playbackBuff_.interleave(playbackIBuff_);
        write(playbackIBuff_.data(), framesToGet, playbackHandle_);
        // Consume the regular one as well (same amount of frames)
        Manager::instance().getMainBuffer().discard(framesToGet, MainBuffer::DEFAULT_ID);
    } else {
        // regular audio data
        playback(playbackAvailFrames);
    }

    if (ringtoneHandle_) {
        AudioLoop *file_tone = Manager::instance().getTelephoneFile();
        int ringtoneAvailFrames = 0;

        if (not safeUpdate(ringtoneHandle_, ringtoneAvailFrames))
            return;

        playbackBuff_.setFormat(audioFormat_);
        playbackBuff_.resize(ringtoneAvailFrames);

        if (file_tone) {
            DEBUG("playback gain %d", playbackGain_);
            file_tone->getNext(playbackBuff_, playbackGain_);
        }

        playbackBuff_.interleave(playbackIBuff_);
        write(playbackIBuff_.data(), ringtoneAvailFrames, ringtoneHandle_);
    }

    // Additionally handle the mic's audio stream
    if (is_capture_running_)
        capture();
}

void AlsaLayer::updatePreference(AudioPreference &preference, int index, DeviceType type)
{
    switch (type) {
        case DeviceType::PLAYBACK:
            preference.setAlsaCardout(index);
            break;

        case DeviceType::CAPTURE:
            preference.setAlsaCardin(index);
            break;

        case DeviceType::RINGTONE:
            preference.setAlsaCardring(index);
            break;

        default:
            break;
    }
}
