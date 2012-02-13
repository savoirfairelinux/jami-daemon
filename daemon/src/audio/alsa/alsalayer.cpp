/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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
#include "audio/dcblocker.h"
#include "eventthread.h"
#include "audio/samplerateconverter.h"
#include "managerimpl.h"
#include "noncopyable.h"
#include "dbus/configurationmanager.h"

class AlsaThread : public ost::Thread {
    public:
        AlsaThread(AlsaLayer *alsa);

        ~AlsaThread() {
            terminate();
        }

        virtual void run();

    private:
        NON_COPYABLE(AlsaThread);
        AlsaLayer* alsa_;
};

AlsaThread::AlsaThread(AlsaLayer *alsa)
    : Thread(), alsa_(alsa)
{
    setCancel(cancelDeferred);
}

/**
 * Reimplementation of run()
 */
void AlsaThread::run()
{
    while (!testCancel()) {
        alsa_->audioCallback();
        Thread::sleep(20);
    }
}

// Constructor
AlsaLayer::AlsaLayer()
    : indexIn_(audioPref.getCardin())
    , indexOut_(audioPref.getCardout())
    , indexRing_(audioPref.getCardring())
    , playbackHandle_(NULL)
    , ringtoneHandle_(NULL)
    , captureHandle_(NULL)
    , audioPlugin_(audioPref.getPlugin())
    // , IDSoundCards_()
    , is_playback_prepared_(false)
    , is_capture_prepared_(false)
    , is_playback_running_(false)
    , is_capture_running_(false)
    , is_playback_open_(false)
    , is_capture_open_(false)
    , audioThread_(NULL)
{
    setCaptureGain(Manager::instance().audioPreference.getVolumemic());
    setPlaybackGain(Manager::instance().audioPreference.getVolumespkr());
}

// Destructor
AlsaLayer::~AlsaLayer()
{
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
        usleep(10000);
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

    std::string pcmp;
    std::string pcmr;
    std::string pcmc;

    if (audioPlugin_ == PCM_DMIX_DSNOOP) {
        pcmp = buildDeviceTopo(PCM_DMIX, indexOut_);
        pcmr = buildDeviceTopo(PCM_DMIX, indexRing_);
        pcmc = buildDeviceTopo(PCM_DSNOOP, indexIn_);
    } else {
        pcmp = buildDeviceTopo(audioPlugin_, indexOut_);
        pcmr = buildDeviceTopo(audioPlugin_, indexRing_);
        pcmc = buildDeviceTopo(audioPlugin_, indexIn_);
    }

    if (not is_capture_open_) {
        is_capture_open_ = openDevice(&captureHandle_, pcmc, SND_PCM_STREAM_CAPTURE);

        if (not is_capture_open_)
            Manager::instance().getDbusManager()->getConfigurationManager()->errorAlert(ALSA_CAPTURE_DEVICE);
    }

    if (not is_playback_open_) {
        is_playback_open_ = openDevice(&playbackHandle_, pcmp, SND_PCM_STREAM_PLAYBACK);

        if (not is_playback_open_)
            Manager::instance().getDbusManager()->getConfigurationManager()->errorAlert(ALSA_PLAYBACK_DEVICE);

        if (getIndexPlayback() != getIndexRingtone())
            if (!openDevice(&ringtoneHandle_, pcmr, SND_PCM_STREAM_PLAYBACK))
                Manager::instance().getDbusManager()->getConfigurationManager()->errorAlert(ALSA_PLAYBACK_DEVICE);
    }

    prepareCaptureStream();
    preparePlaybackStream();

    startCaptureStream();
    startPlaybackStream();

    flushMain();
    flushUrgent();

    if (audioThread_ == NULL) {
        audioThread_ = new AlsaThread(this);
        audioThread_->start();
    }

    isStarted_ = true;
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

//////////////////////////////////////////////////////////////////////////////////////////////
/////////////////   ALSA PRIVATE FUNCTIONS   ////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

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
				ERROR("ALSA: "error": %s", snd_strerror(err_code)); \
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
    if (playbackHandle_ and not is_playback_running_)
        if (ALSA_CALL(snd_pcm_start(playbackHandle_), "Couldn't start playback") >= 0)
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
    if (is_playback_open_ and not is_playback_prepared_)
        if (ALSA_CALL(snd_pcm_prepare(playbackHandle_), "Couldn't prepare playback") >= 0)
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

    snd_pcm_uframes_t periodSize = 160;
    unsigned int periods = 4;

#define HW pcm_handle, hwparams /* hardware parameters */
    TRY(snd_pcm_hw_params_any(HW), "hwparams init");
    TRY(snd_pcm_hw_params_set_access(HW, SND_PCM_ACCESS_RW_INTERLEAVED), "access type");
    TRY(snd_pcm_hw_params_set_format(HW, SND_PCM_FORMAT_S16_LE), "sample format");
    TRY(snd_pcm_hw_params_set_rate_near(HW, &audioSampleRate_, NULL), "sample rate");
    TRY(snd_pcm_hw_params_set_channels(HW, 1), "channel count");
    TRY(snd_pcm_hw_params_set_period_size_near(HW, &periodSize, NULL), "period time");
    TRY(snd_pcm_hw_params_set_periods_near(HW, &periods, NULL), "periods number");
    TRY(snd_pcm_hw_params(HW), "hwparams");
#undef HW

    DEBUG("ALSA: %s using sampling rate %dHz",
           (snd_pcm_stream(pcm_handle) == SND_PCM_STREAM_PLAYBACK) ? "playback" : "capture",
           audioSampleRate_);

    snd_pcm_sw_params_t *swparams = NULL;
    snd_pcm_sw_params_alloca(&swparams);

#define SW pcm_handle, swparams /* software parameters */
    snd_pcm_sw_params_current(SW);
    TRY(snd_pcm_sw_params_set_start_threshold(SW, periodSize * 2), "start threshold");
    TRY(snd_pcm_sw_params(SW), "sw parameters");
#undef SW

    return true;

#undef TRY
}

//TODO	first frame causes broken pipe (underrun) because not enough data are send --> make the handle wait to be ready
void
AlsaLayer::write(void* buffer, int length, snd_pcm_t * handle)
{
    snd_pcm_uframes_t frames = snd_pcm_bytes_to_frames(handle, length);

    int err = snd_pcm_writei(handle, buffer , frames);

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

            ALSA_CALL(snd_pcm_writei(handle, buffer , frames), "XRUN handling failed");
            break;
        }

        default:
            ERROR("ALSA: unknown write error, dropping frames: %s", snd_strerror(err));
            stopPlaybackStream();
            break;
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

            ERROR("ALSA: XRUN capture ignored (%s)", snd_strerror(err));
            break;
        }

        case EPERM:
            ERROR("ALSA: Can't capture, EPERM (%s)", snd_strerror(err));
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

std::vector<std::string>
AlsaLayer::getAudioDeviceList(AudioStreamDirection dir) const
{
    std::vector<HwIDPair> deviceMap;
    std::vector<std::string> audioDeviceList;

    deviceMap = getAudioDeviceIndexMap(dir);

    for(std::vector<HwIDPair>::const_iterator iter = deviceMap.begin(); iter != deviceMap.end(); iter++) {
         audioDeviceList.push_back(iter->second);
    }

    return audioDeviceList;
}


std::vector<HwIDPair>
AlsaLayer::getAudioDeviceIndexMap(AudioStreamDirection dir) const
{
    snd_ctl_t* handle;
    snd_ctl_card_info_t *info;
    snd_pcm_info_t* pcminfo;
    snd_ctl_card_info_alloca(&info);
    snd_pcm_info_alloca(&pcminfo);

    int numCard = -1 ;

    std::vector<HwIDPair> audioDevice;

    if (snd_card_next(&numCard) < 0 || numCard < 0)
        return audioDevice;

    do {
        std::stringstream ss;
        ss << numCard;
        std::string name = "hw:" + ss.str();

        if (snd_ctl_open(&handle, name.c_str(), 0) == 0) {
            if (snd_ctl_card_info(handle, info) == 0) {
                snd_pcm_info_set_device(pcminfo , 0);
                snd_pcm_info_set_stream(pcminfo, (dir == AUDIO_STREAM_CAPTURE) ? SND_PCM_STREAM_CAPTURE : SND_PCM_STREAM_PLAYBACK);

                if (snd_ctl_pcm_info(handle ,pcminfo) < 0) {
                    DEBUG(" Cannot get info");
                }
                else {
                    DEBUG("card %i : %s [%s]",
                           numCard,
                           snd_ctl_card_info_get_id(info),
                           snd_ctl_card_info_get_name(info));
                    std::string description = snd_ctl_card_info_get_name(info);
                    description.append(" - ");
                    description.append(snd_pcm_info_get_name(pcminfo));

                    // The number of the sound card is associated with a string description
                    audioDevice.push_back(HwIDPair(numCard , description));
                }
            }

            snd_ctl_close(handle);
        }
    } while (snd_card_next(&numCard) >= 0 && numCard >= 0);


    return audioDevice;
}


bool
AlsaLayer::soundCardIndexExists(int card, int stream)
{
    snd_ctl_t* handle;
    snd_pcm_info_t *pcminfo;
    snd_pcm_info_alloca(&pcminfo);
    std::string name("hw:");
    std::stringstream ss;
    ss << card;
    name.append(ss.str());

    if (snd_ctl_open(&handle, name.c_str(), 0) != 0)
        return false;

    snd_pcm_info_set_stream(pcminfo , (stream == SFL_PCM_PLAYBACK) ? SND_PCM_STREAM_PLAYBACK : SND_PCM_STREAM_CAPTURE);
    bool ret = snd_ctl_pcm_info(handle , pcminfo) >= 0;
    snd_ctl_close(handle);
    return ret;
}

int
AlsaLayer::getAudioDeviceIndex(const std::string &description) const
{
    std::vector<HwIDPair> audioDeviceIndexMap;

    std::vector<HwIDPair> captureDevice = getAudioDeviceIndexMap(AUDIO_STREAM_CAPTURE);
    std::vector<HwIDPair> playbackDevice = getAudioDeviceIndexMap(AUDIO_STREAM_PLAYBACK);

    audioDeviceIndexMap.insert(audioDeviceIndexMap.end(), captureDevice.begin(), captureDevice.end());
    audioDeviceIndexMap.insert(audioDeviceIndexMap.end(), playbackDevice.begin(), playbackDevice.end());

    for (std::vector<HwIDPair>::const_iterator iter = audioDeviceIndexMap.begin(); iter != audioDeviceIndexMap.end(); ++iter)
        if (iter->second == description)
            return iter->first;

    // else return the default one
    return 0;
}

namespace {
void adjustVolume(SFLDataFormat *src , int samples, int volumePercentage)
{
    if (volumePercentage != 100)
        for (int i = 0 ; i < samples; i++)
            src[i] = src[i] * volumePercentage * 0.01;
}
}

void AlsaLayer::capture()
{
    unsigned int mainBufferSampleRate = Manager::instance().getMainBuffer()->getInternalSamplingRate();
    bool resample = audioSampleRate_ != mainBufferSampleRate;

    int toGetSamples = snd_pcm_avail_update(captureHandle_);

    if (toGetSamples < 0)
        ERROR("Audio: Mic error: %s", snd_strerror(toGetSamples));

    if (toGetSamples <= 0)
        return;

    const int framesPerBufferAlsa = 2048;

    if (toGetSamples > framesPerBufferAlsa)
        toGetSamples = framesPerBufferAlsa;

    int toGetBytes = toGetSamples * sizeof(SFLDataFormat);
    SFLDataFormat* in = (SFLDataFormat*) malloc(toGetBytes);

    if (read(in, toGetBytes) != toGetBytes) {
        ERROR("ALSA MIC : Couldn't read!");
        goto end;
    }

    adjustVolume(in, toGetSamples, getCaptureGain());

    if (resample) {
        int outSamples = toGetSamples * ((double) audioSampleRate_ / mainBufferSampleRate);
        int outBytes = outSamples * sizeof(SFLDataFormat);
        SFLDataFormat* rsmpl_out = (SFLDataFormat*) malloc(outBytes);
        converter_->resample((SFLDataFormat*) in, rsmpl_out, mainBufferSampleRate, audioSampleRate_, toGetSamples);
        dcblocker_.process(rsmpl_out, rsmpl_out, outSamples);
        Manager::instance().getMainBuffer()->putData(rsmpl_out, outBytes);
        free(rsmpl_out);
    } else {
        dcblocker_.process(in, in, toGetSamples);
        Manager::instance().getMainBuffer()->putData(in, toGetBytes);
    }

end:
    free(in);
}

void AlsaLayer::playback(int maxSamples)
{

    unsigned int mainBufferSampleRate = Manager::instance().getMainBuffer()->getInternalSamplingRate();
    bool resample = audioSampleRate_ != mainBufferSampleRate;

    int toGet = Manager::instance().getMainBuffer()->availForGet();
    int toPut = maxSamples * sizeof(SFLDataFormat);

    if (toGet <= 0) {    	// no audio available, play tone or silence
        AudioLoop *tone = Manager::instance().getTelephoneTone();
        AudioLoop *file_tone = Manager::instance().getTelephoneFile();

        SFLDataFormat *out = (SFLDataFormat *) malloc(toPut);

        if (tone) {
            tone->getNext(out, maxSamples, getPlaybackGain());
        }
        else if (file_tone && !ringtoneHandle_) {
            file_tone->getNext(out, maxSamples, getPlaybackGain());
        }
        else
            memset(out, 0, toPut);

        write(out, toPut, playbackHandle_);
        free(out);
        return;
    }

    // play the regular sound samples

    int maxNbBytesToGet = toPut;
    // Compute maximal value to get from the ring buffer
    double resampleFactor = 1.0;

    if (resample) {
        resampleFactor = (double) audioSampleRate_ / mainBufferSampleRate;
        maxNbBytesToGet = (double) toGet / resampleFactor;
    }

    if (toGet > maxNbBytesToGet)
        toGet = maxNbBytesToGet;

    SFLDataFormat *out = (SFLDataFormat*) malloc(toGet);
    Manager::instance().getMainBuffer()->getData(out, toGet);
    adjustVolume(out, toGet / sizeof(SFLDataFormat), getPlaybackGain());

    if (resample) {
        int inSamples = toGet / sizeof(SFLDataFormat);
        int outSamples = inSamples * resampleFactor;
        SFLDataFormat *rsmpl_out = (SFLDataFormat*) malloc(outSamples * sizeof(SFLDataFormat));
        converter_->resample(out, rsmpl_out, mainBufferSampleRate, audioSampleRate_, inSamples);
        write(rsmpl_out, outSamples * sizeof(SFLDataFormat), playbackHandle_);
        free(rsmpl_out);
    } else {
        write(out, toGet, playbackHandle_);
    }

    free(out);
}

void AlsaLayer::audioCallback()
{
    if (!playbackHandle_ or !captureHandle_)
        return;

    notifyincomingCall();

    snd_pcm_wait(playbackHandle_, 20);

    int playbackAvailSmpl = snd_pcm_avail_update(playbackHandle_);
    int playbackAvailBytes = playbackAvailSmpl * sizeof(SFLDataFormat);

    int toGet = urgentRingBuffer_.AvailForGet();

    if (toGet > 0) {
        // Urgent data (dtmf, incoming call signal) come first.
        if (toGet > playbackAvailBytes)
            toGet = playbackAvailBytes;

        SFLDataFormat *out = (SFLDataFormat*) malloc(toGet);
        urgentRingBuffer_.Get(out, toGet);
        adjustVolume(out, toGet / sizeof(SFLDataFormat), getPlaybackGain());

        write(out, toGet, playbackHandle_);
        free(out);
        // Consume the regular one as well (same amount of bytes)
        Manager::instance().getMainBuffer()->discard(toGet);
    } else {
        // regular audio data
        playback(playbackAvailSmpl);
    }

    if (ringtoneHandle_) {
        AudioLoop *file_tone = Manager::instance().getTelephoneFile();
        int ringtoneAvailSmpl = snd_pcm_avail_update(ringtoneHandle_);
        int ringtoneAvailBytes = ringtoneAvailSmpl*sizeof(SFLDataFormat);

        SFLDataFormat *out = (SFLDataFormat *) malloc(ringtoneAvailBytes);

        if (file_tone) {
            DEBUG("playback gain %d", getPlaybackGain());
            file_tone->getNext(out, ringtoneAvailSmpl, getPlaybackGain());
        }
        else
            memset(out, 0, ringtoneAvailBytes);

        write(out, ringtoneAvailBytes, ringtoneHandle_);
        free(out);
    }

    // Additionally handle the mic's audio stream
    if (is_capture_running_)
        capture();
}
