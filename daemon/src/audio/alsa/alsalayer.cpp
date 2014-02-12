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

class AlsaThread {
    public:
        AlsaThread(AlsaLayer *alsa);
        ~AlsaThread();
        void initAudioLayer();
        void start();
        bool isRunning() const;

    private:
        void run();
        static void *runCallback(void *context);

        NON_COPYABLE(AlsaThread);
        pthread_t thread_;
        AlsaLayer* alsa_;
        bool running_;
};

AlsaThread::AlsaThread(AlsaLayer *alsa)
    : thread_(0), alsa_(alsa), running_(false)
{}

bool AlsaThread::isRunning() const
{
    return running_;
}

AlsaThread::~AlsaThread()
{
    running_ = false;

    if (thread_)
        pthread_join(thread_, NULL);
}

void AlsaThread::start()
{
    running_ = true;
    pthread_create(&thread_, NULL, &runCallback, this);
}

void *
AlsaThread::runCallback(void *data)
{
    AlsaThread *context = static_cast<AlsaThread*>(data);
    context->run();
    return NULL;
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
        usleep(20000); // 20 ms
    }
}

AlsaLayer::AlsaLayer(const AudioPreference &pref)
    : AudioLayer(pref)
    , indexIn_(pref.getAlsaCardin())
    , indexOut_(pref.getAlsaCardout())
    , indexRing_(pref.getAlsaCardring())
    , watchdogTotalCount_(0)
    , watchdogTotalErr_(0)
    , playbackHandle_(NULL)
    , ringtoneHandle_(NULL)
    , captureHandle_(NULL)
    , audioPlugin_(pref.getAlsaPlugin())
    , is_playback_prepared_(false)
    , is_capture_prepared_(false)
    , is_playback_running_(false)
    , is_capture_running_(false)
    , is_playback_open_(false)
    , is_capture_open_(false)
    , audioThread_(NULL)
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
    TRY(snd_pcm_hw_params_set_rate_near(HW, &audioFormat_.sample_rate, NULL), "sample rate");
    TRY(snd_pcm_hw_params_set_channels(HW, 1), "channel count");

    snd_pcm_hw_params_get_buffer_size_min(hwparams, &buffer_size_min);
    snd_pcm_hw_params_get_buffer_size_max(hwparams, &buffer_size_max);
    snd_pcm_hw_params_get_period_size_min(hwparams, &period_size_min, NULL);
    snd_pcm_hw_params_get_period_size_max(hwparams, &period_size_max, NULL);
    DEBUG("Buffer size range from %lu to %lu", buffer_size_min, buffer_size_max);
    DEBUG("Period size range from %lu to %lu", period_size_min, period_size_max);
    buffer_size = buffer_size > buffer_size_max ? buffer_size_max : buffer_size;
    buffer_size = buffer_size < buffer_size_min ? buffer_size_min : buffer_size;
    period_size = period_size > period_size_max ? period_size_max : period_size;
    period_size = period_size < period_size_min ? period_size_min : period_size;

    TRY(snd_pcm_hw_params_set_buffer_size_near(HW, &buffer_size), "Unable to set buffer size for playback");
    TRY(snd_pcm_hw_params_set_period_size_near(HW, &period_size, NULL), "Unable to set period size for playback");
    TRY(snd_pcm_hw_params_set_periods_near(HW, &periods, NULL), "Unable to set number of periods for playback");
    TRY(snd_pcm_hw_params(HW), "hwparams");

    snd_pcm_hw_params_get_buffer_size(hwparams, &buffer_size);
    snd_pcm_hw_params_get_period_size(hwparams, &period_size, NULL);
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
AlsaLayer::write(void* buffer, int length, snd_pcm_t * handle)
{
    // Skip empty buffers
    if (!length)
        return;

    snd_pcm_uframes_t frames = snd_pcm_bytes_to_frames(handle, length);
    watchdogTotalCount_++;

    int err = snd_pcm_writei(handle, buffer, frames);

    if (err < 0)
        snd_pcm_recover(handle, err, 0);

    if (err >= 0)
        return;

    watchdogTotalErr_++;

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

            ALSA_CALL(snd_pcm_writei(handle, buffer, frames), "XRUN handling failed");
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

    // Detect when something is going wrong. This can be caused by alsa bugs or
    // faulty encoder on the other side
    // TODO do something useful instead of just warning and flushing buffers
    if (watchdogTotalErr_ > 0 && watchdogTotalCount_ / watchdogTotalErr_ >= 4 and watchdogTotalCount_ > 50) {
        ERROR("%d errors out of %d frames", watchdogTotalErr_, watchdogTotalCount_);
        flushUrgent();
        flushMain();
    }
}

int
AlsaLayer::read(void* buffer, int toCopy)
{
    if (snd_pcm_state(captureHandle_) == SND_PCM_STATE_XRUN) {
        prepareCaptureStream();
        startCaptureStream();
    }

    snd_pcm_uframes_t frames = snd_pcm_bytes_to_frames(captureHandle_, toCopy);

    int err = snd_pcm_readi(captureHandle_, buffer, frames);

    if (err >= 0)
        return snd_pcm_frames_to_bytes(captureHandle_, frames);

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

namespace {
bool safeUpdate(snd_pcm_t *handle, int &samples)
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

std::vector<std::string>
getValues(const std::vector<HwIDPair> &deviceMap)
{
    std::vector<std::string> audioDeviceList;

    for (const auto & dev : deviceMap)
        audioDeviceList.push_back(dev.second);

    return audioDeviceList;
}
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
AlsaLayer::getAudioDeviceIndex(const std::string &description) const
{
    std::vector<HwIDPair> captureDevice(getAudioDeviceIndexMap(true));
    std::vector<HwIDPair> playbackDevice(getAudioDeviceIndexMap(false));

    std::vector<HwIDPair> audioDeviceIndexMap;
    audioDeviceIndexMap.insert(audioDeviceIndexMap.end(), captureDevice.begin(), captureDevice.end());
    audioDeviceIndexMap.insert(audioDeviceIndexMap.end(), playbackDevice.begin(), playbackDevice.end());

    for (const auto & dev : audioDeviceIndexMap)
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
    unsigned int mainBufferSampleRate = Manager::instance().getMainBuffer().getInternalSamplingRate();
    bool resample = audioFormat_.sample_rate != mainBufferSampleRate;

    int toGetSamples = snd_pcm_avail_update(captureHandle_);

    if (toGetSamples < 0)
        ERROR("Audio: Mic error: %s", snd_strerror(toGetSamples));

    if (toGetSamples <= 0)
        return;

    const int framesPerBufferAlsa = 2048;
    toGetSamples = std::min(framesPerBufferAlsa, toGetSamples);

    AudioBuffer in(toGetSamples, audioFormat_);

    // TODO: handle ALSA multichannel capture
    const int toGetBytes = in.frames() * sizeof(SFLAudioSample);
    SFLAudioSample * const in_ptr = in.getChannel(0)->data();

    if (read(in_ptr, toGetBytes) != toGetBytes) {
        ERROR("ALSA MIC : Couldn't read!");
        return;
    }

    in.applyGain(isCaptureMuted_ ? 0.0 : captureGain_);

    if (resample) {
        int outSamples = toGetSamples * (static_cast<double>(audioFormat_.sample_rate) / mainBufferSampleRate);
        AudioBuffer rsmpl_out(outSamples, AudioFormat(mainBufferSampleRate, 1));
        resampler_.resample(in, rsmpl_out);
        dcblocker_.process(rsmpl_out);
        Manager::instance().getMainBuffer().putData(rsmpl_out, MainBuffer::DEFAULT_ID);
    } else {
        dcblocker_.process(in);
        Manager::instance().getMainBuffer().putData(in, MainBuffer::DEFAULT_ID);
    }
}

void AlsaLayer::playback(int maxSamples)
{
    size_t bytesToGet = Manager::instance().getMainBuffer().availableForGet(MainBuffer::DEFAULT_ID);

    const size_t bytesToPut = maxSamples * sizeof(SFLAudioSample);

    // no audio available, play tone or silence
    if (bytesToGet <= 0) {
        // FIXME: not thread safe! we only lock the mutex when we get the
        // pointer, we have no guarantee that it will stay safe to use
        AudioLoop *tone = Manager::instance().getTelephoneTone();
        AudioLoop *file_tone = Manager::instance().getTelephoneFile();

        AudioBuffer out(maxSamples, audioFormat_);

        if (tone)
            tone->getNext(out, playbackGain_);
        else if (file_tone && !ringtoneHandle_)
            file_tone->getNext(out, playbackGain_);

        write(out.getChannel(0)->data(), bytesToPut, playbackHandle_);
    } else {
        // play the regular sound samples

        const size_t mainBufferSampleRate = Manager::instance().getMainBuffer().getInternalSamplingRate();
        const bool resample = audioFormat_.sample_rate != mainBufferSampleRate;

        double resampleFactor = 1.0;
        size_t maxNbBytesToGet = bytesToPut;

        if (resample) {
            resampleFactor = static_cast<double>(audioFormat_.sample_rate) / mainBufferSampleRate;
            maxNbBytesToGet = bytesToGet / resampleFactor;
        }

        bytesToGet = std::min(maxNbBytesToGet, bytesToGet);

        const size_t samplesToGet = bytesToGet / sizeof(SFLAudioSample);
        AudioBuffer out(samplesToGet, AudioFormat(mainBufferSampleRate, 1));

        Manager::instance().getMainBuffer().getData(out, MainBuffer::DEFAULT_ID);
        out.applyGain(isPlaybackMuted_ ? 0.0 : playbackGain_);

        if (resample) {
            const size_t outSamples = samplesToGet * resampleFactor;
            const size_t outBytes = outSamples * sizeof(SFLAudioSample);
            AudioBuffer rsmpl_out(outSamples, audioFormat_);
            resampler_.resample(out, rsmpl_out);
            write(rsmpl_out.getChannel(0)->data(), outBytes, playbackHandle_);
        } else {
            write(out.getChannel(0)->data(), bytesToGet, playbackHandle_);
        }
    }
}

void AlsaLayer::audioCallback()
{
    if (!playbackHandle_ or !captureHandle_)
        return;

    notifyIncomingCall();

    snd_pcm_wait(playbackHandle_, 20);

    int playbackAvailSmpl = 0;

    if (not safeUpdate(playbackHandle_, playbackAvailSmpl))
        return;

    unsigned samplesToGet = urgentRingBuffer_.availableForGet(MainBuffer::DEFAULT_ID);

    if (samplesToGet > 0) {
        // Urgent data (dtmf, incoming call signal) come first.
        samplesToGet = std::min(samplesToGet, (unsigned)playbackAvailSmpl);
        AudioBuffer out(samplesToGet, AudioFormat::MONO);
        urgentRingBuffer_.get(out, MainBuffer::DEFAULT_ID);
        out.applyGain(isPlaybackMuted_ ? 0.0 : playbackGain_);

        write(out.getChannel(0)->data(), samplesToGet * sizeof(SFLAudioSample), playbackHandle_);
        // Consume the regular one as well (same amount of bytes)
        Manager::instance().getMainBuffer().discard(samplesToGet, MainBuffer::DEFAULT_ID);
    } else {
        // regular audio data
        playback(playbackAvailSmpl);
    }

    if (ringtoneHandle_) {
        AudioLoop *file_tone = Manager::instance().getTelephoneFile();
        int ringtoneAvailSmpl = 0;

        if (not safeUpdate(ringtoneHandle_, ringtoneAvailSmpl))
            return;

        int ringtoneAvailBytes = ringtoneAvailSmpl * sizeof(SFLAudioSample);

        AudioBuffer out(ringtoneAvailSmpl, AudioFormat::MONO);

        if (file_tone) {
            DEBUG("playback gain %d", playbackGain_);
            file_tone->getNext(out, playbackGain_);
        }

        write(out.getChannel(0)->data(), ringtoneAvailBytes, ringtoneHandle_);
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
