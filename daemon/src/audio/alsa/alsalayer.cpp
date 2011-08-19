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
#include "audio/audioprocessing.h"
#include "eventthread.h"
#include "audio/samplerateconverter.h"
#include "managerimpl.h"

class AlsaThread : public ost::Thread
{
    public:
        AlsaThread (AlsaLayer *alsa);

        ~AlsaThread () {
            terminate();
        }

        virtual void run (void);

    private:
        AlsaThread (const AlsaThread& at);
        AlsaThread& operator= (const AlsaThread& at);

        AlsaLayer* _alsa;
};

AlsaThread::AlsaThread (AlsaLayer *alsa)
    : Thread(), _alsa (alsa)
{
    setCancel (cancelDeferred);
}

/**
 * Reimplementation of run()
 */
void AlsaThread::run (void)
{
    while (!testCancel()) {
        _alsa->audioCallback();
        Thread::sleep (20);
    }
}

// Constructor
AlsaLayer::AlsaLayer (ManagerImpl* manager)
    : AudioLayer (manager, ALSA)
    , playbackHandle_ (NULL)
    , ringtoneHandle_ (NULL)
    , captureHandle_ (NULL)
    , periodSize_ (160)
    , audioPlugin_ ("default")
    , IDSoundCards_ ()
    , is_playback_prepared_ (false)
    , is_capture_prepared_ (false)
    , is_playback_running_ (false)
    , is_capture_running_ (false)
    , is_playback_open_ (false)
    , is_capture_open_ (false)
    , trigger_request_ (false)
    , audioThread_ (NULL)
    , converter_ (0)

{
    _debug ("Audio: Build ALSA layer");
    _urgentRingBuffer.createReadPointer();

    audioPlugin_ = AudioLayer::_manager->audioPreference.getPlugin();

    AudioLayer::_noisesuppressstate = true;
}

// Destructor
AlsaLayer::~AlsaLayer (void)
{
    _debug ("Audio: Destroy of ALSA layer");
    closeLayer();

    delete converter_;
}

void
AlsaLayer::closeLayer()
{
    _debugAlsa ("Audio: Close ALSA streams");

    try {
        /* Stop the audio thread first */
        if (audioThread_) {
            _debug ("Audio: Stop Audio Thread");
            delete audioThread_;
            audioThread_ = NULL;
        }
    } catch (...) {
        _debugException ("Audio: Exception: when stopping audiortp");
        throw;
    }

    /* Then close the audio devices */
    closeCaptureStream();
    closePlaybackStream();

    captureHandle_ = NULL;
    playbackHandle_ = NULL;
    ringtoneHandle_ = NULL;
}

void
AlsaLayer::openDevice (int indexIn, int indexOut, int indexRing, int sampleRate, int frameSize, int stream , const std::string &plugin)
{
    /* Close the devices before open it */
    if (stream == SFL_PCM_BOTH and is_capture_open_ and is_playback_open_) {
        closeCaptureStream();
        closePlaybackStream();
    } else if ( (stream == SFL_PCM_CAPTURE or stream == SFL_PCM_BOTH) and is_capture_open_)
        closeCaptureStream ();
    else if ( (stream == SFL_PCM_PLAYBACK or stream == SFL_PCM_BOTH) and is_playback_open_)
        closePlaybackStream ();

    _indexIn = indexIn;
    _indexOut = indexOut;
    _indexRing = indexRing;

    _audioSampleRate = sampleRate;
    _frameSize = frameSize;

    audioPlugin_ = plugin;

    _debugAlsa (" Setting AlsaLayer: device     in=%2d, out=%2d, ring=%2d", _indexIn, _indexOut, _indexRing);
    _debugAlsa ("                   : alsa plugin=%s", audioPlugin_.c_str());
    _debugAlsa ("                   : nb channel in=%2d, out=%2d", _inChannel, _outChannel);
    _debugAlsa ("                   : sample rate=%5d, format=%s", _audioSampleRate, SFLDataFormatString);

    audioThread_ = NULL;

    // use 1 sec buffer for resampling
    converter_ = new SamplerateConverter (_audioSampleRate);
    _dcblocker = new DcBlocker;
    _audiofilter = new AudioProcessing (_dcblocker);
}

void
AlsaLayer::startStream (void)
{
    _debug ("Audio: Start stream");

    if (_audiofilter)
        _audiofilter->resetAlgorithm();

    if (is_playback_running_ and is_capture_running_)
        return;

    std::string pcmp;
    std::string pcmr;
    std::string pcmc;

    if (audioPlugin_ == PCM_DMIX_DSNOOP) {
        pcmp = buildDeviceTopo (PCM_DMIX, _indexOut, 0);
        pcmr = buildDeviceTopo (PCM_DMIX, _indexRing, 0);
        pcmc = buildDeviceTopo (PCM_DSNOOP, _indexIn, 0);
    } else {
        pcmp = buildDeviceTopo (audioPlugin_, _indexOut, 0);
        pcmr = buildDeviceTopo (audioPlugin_, _indexRing, 0);
        pcmc = buildDeviceTopo (audioPlugin_, _indexIn, 0);
    }

    _debug ("pcmp: %s, index %d", pcmp.c_str(), _indexOut);
    _debug ("pcmr: %s, index %d", pcmr.c_str(), _indexRing);
    _debug ("pcmc: %s, index %d", pcmc.c_str(), _indexIn);

    if (not is_capture_open_)
        open_device (pcmp, pcmc, pcmr, SFL_PCM_CAPTURE);

    if (not is_playback_open_)
        open_device (pcmp, pcmc, pcmr, SFL_PCM_PLAYBACK);

    prepareCaptureStream ();
    preparePlaybackStream ();

    startCaptureStream ();
    startPlaybackStream ();

    flushMain();
    flushUrgent();

    if (audioThread_ == NULL) {
        try {
            _debug ("Audio: Start Audio Thread");
            audioThread_ = new AlsaThread (this);
            audioThread_->start();
        } catch (...) {
            _debugException ("Fail to start audio thread");
        }
    }

    _isStarted = true;
}

void
AlsaLayer::stopStream (void)
{
    _debug ("Audio: Stop stream");

    _isStarted = false;

    try {
        /* Stop the audio thread first */
        if (audioThread_) {
            _debug ("Audio: Stop audio thread");
            delete audioThread_;
            audioThread_ = NULL;
        }
    } catch (...) {
        _debugException ("Audio: Exception: when stopping audiortp");
        throw;
    }

    closeCaptureStream ();
    closePlaybackStream ();

    playbackHandle_ = NULL;
    captureHandle_ = NULL;
    ringtoneHandle_ = NULL;

    /* Flush the ring buffers */
    flushUrgent ();
    flushMain ();
}

void AlsaLayer::setNoiseSuppressState (bool state)
{
    // if a stream already opened
    AudioLayer::_noisesuppressstate = state;
}

//////////////////////////////////////////////////////////////////////////////////////////////
/////////////////   ALSA PRIVATE FUNCTIONS   ////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

void AlsaLayer::stopCaptureStream (void)
{
    if (captureHandle_) {
        _debug ("Audio: Stop ALSA capture");

        int err;
        if ((err = snd_pcm_drop (captureHandle_)) < 0)
            _debug ("Audio: Error: stopping ALSA capture: %s", snd_strerror (err));
        else {
            is_capture_running_ = false;
            is_capture_prepared_ = false;
        }
    }
}

void AlsaLayer::closeCaptureStream (void)
{
    if (is_capture_prepared_ and is_capture_running_)
        stopCaptureStream ();

    if (is_capture_open_) {
        _debug ("Audio: Close ALSA capture");

        int err;
        if ((err = snd_pcm_close (captureHandle_)) < 0)
            _debug ("Audio: Error: Closing ALSA capture: %s", snd_strerror (err));
        else
            is_capture_open_ = false;
    }
}

void AlsaLayer::startCaptureStream (void)
{
    if (captureHandle_ and not is_capture_running_) {
        _debug ("Audio: Start ALSA capture");

        int err;
        if ((err = snd_pcm_start (captureHandle_)) < 0)
            _debug ("Error starting ALSA capture: %s",  snd_strerror (err));
        else
            is_capture_running_ = true;
    }
}

void AlsaLayer::prepareCaptureStream (void)
{
    int err;

    if (is_capture_open_ and not is_capture_prepared_) {
        _debug ("Audio: Prepare ALSA capture");

        if ((err = snd_pcm_prepare (captureHandle_)) < 0)
            _debug ("Audio: Error: preparing ALSA capture: %s", snd_strerror (err));
        else
            is_capture_prepared_ = true;
    }
}

void AlsaLayer::stopPlaybackStream (void)
{
    if (ringtoneHandle_ and is_playback_running_) {
        _debug ("Audio: Stop ALSA ringtone");

        int err;
        if ((err = snd_pcm_drop (ringtoneHandle_)) < 0)
            _debug ("Audio: Error: Stop ALSA ringtone: %s", snd_strerror (err));
    }

    if (playbackHandle_ and is_playback_running_) {
        _debug ("Audio: Stop ALSA playback");

        int err;
        if ((err = snd_pcm_drop (playbackHandle_)) < 0)
            _debug ("Audio: Error: Stopping ALSA playback: %s", snd_strerror (err));
        else {
            is_playback_running_ = false;
            is_playback_prepared_ = false;
        }
    }
}


void AlsaLayer::closePlaybackStream (void)
{
    if (is_playback_prepared_ and is_playback_running_)
        stopPlaybackStream();

    if (is_playback_open_) {
        _debug ("Audio: Close ALSA playback");

        int err;
        if (ringtoneHandle_ and (err = snd_pcm_close (ringtoneHandle_)) < 0)
                _warn ("Audio: Error: Closing ALSA ringtone: %s", snd_strerror (err));

        if ((err = snd_pcm_close (playbackHandle_)) < 0)
            _warn ("Audio: Error: Closing ALSA playback: %s", snd_strerror (err));
        else
            is_playback_open_ = false;
    }

}

void AlsaLayer::startPlaybackStream (void)
{
    if (playbackHandle_ and not is_playback_running_) {
        _debug ("Audio: Start ALSA playback");

        int err;
        if ((err = snd_pcm_start (playbackHandle_)) < 0)
            _debug ("Audio: Error: Starting ALSA playback: %s", snd_strerror (err));
        else
            is_playback_running_ = true;
    }
}

void AlsaLayer::preparePlaybackStream (void)
{
    if (is_playback_open_ and not is_playback_prepared_) {
        _debug ("Audio: Prepare playback stream");

        int err;
        if ((err = snd_pcm_prepare (playbackHandle_)) < 0)
            _debug ("Audio: Preparing the device: %s", snd_strerror (err));
        else
            is_playback_prepared_ = true;
    }
}

bool AlsaLayer::alsa_set_params (snd_pcm_t *pcm_handle, int type)
{
    snd_pcm_hw_params_t *hwparams = NULL;
    snd_pcm_sw_params_t *swparams = NULL;
    int format;
    int periods = 4;
    int periodsize = 160;

    /* Allocate the snd_pcm_hw_params_t struct */
    snd_pcm_hw_params_malloc (&hwparams);

    periodSize_ = periodsize;
    /* Full configuration space */

    int err;
    if ((err = snd_pcm_hw_params_any (pcm_handle, hwparams)) < 0) {
        _debugAlsa ("Audio: Error: Cannot initialize hardware parameter structure (%s)", snd_strerror (err));
        return false;
    }

    if ((err = snd_pcm_hw_params_set_access (pcm_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        _debugAlsa ("Audio: Error: Cannot set access type (%s)", snd_strerror (err));
        return false;
    }

    /* Set sample format */
    format = SND_PCM_FORMAT_S16_LE;

    if ((err = snd_pcm_hw_params_set_format (pcm_handle, hwparams, (snd_pcm_format_t) format)) < 0) {
        _debugAlsa ("Audio: Error: Cannot set sample format (%s)", snd_strerror (err));
        return false;
    }

    /* Set sample rate. If we can't set to the desired exact value, we set to the nearest acceptable */
    int dir = 0;

    unsigned int exact_ivalue = _audioSampleRate;

    if ((err = snd_pcm_hw_params_set_rate_near (pcm_handle, hwparams, &exact_ivalue, &dir) < 0)) {
        _debugAlsa ("Audio: Error: Cannot set sample rate (%s)", snd_strerror (err));
        return false;
    } else
        _debug ("Audio: Set audio rate to %d", _audioSampleRate);

    if (dir != 0) {
        _debugAlsa ("Audio: Error: (%i) The chosen rate %d Hz is not supported by your hardware.Using %d Hz instead. ", type , _audioSampleRate, exact_ivalue);
        _audioSampleRate = exact_ivalue;
    }

    /* Set the number of channels */
    if ((err = snd_pcm_hw_params_set_channels (pcm_handle, hwparams, 1)) < 0) {
        _debugAlsa ("Audio: Error: Cannot set channel count (%s)", snd_strerror (err));
        return false;
    }

    /* Set the buffer size in frames */
    unsigned long exact_lvalue = periodsize;

    dir = 0;

    if ((err = snd_pcm_hw_params_set_period_size_near (pcm_handle, hwparams, &exact_lvalue, &dir)) < 0) {
        _debugAlsa ("Audio: Error: Cannot set period time (%s)", snd_strerror (err));
        return false;
    }

    if (dir != 0)
        _debugAlsa ("Audio: Warning: (%i) The chosen period size %lu bytes is not supported by your hardware.Using %lu instead. ", type, periodsize, exact_lvalue);

    periodSize_ = exact_lvalue;
    /* Set the number of fragments */
    exact_ivalue = periods;
    dir = 0;

    if ((err = snd_pcm_hw_params_set_periods_near (pcm_handle, hwparams, &exact_ivalue, &dir)) < 0) {
        _debugAlsa ("Audio: Error: Cannot set periods number (%s)", snd_strerror (err));
        return false;
    }

    if (dir != 0)
        _debugAlsa ("Audio: Warning: The chosen period number %i bytes is not supported by your hardware.Using %i instead. ", periods, exact_ivalue);

    periods = exact_ivalue;

    /* Set the hw parameters */

    if ((err = snd_pcm_hw_params (pcm_handle, hwparams)) < 0) {
        _debugAlsa ("Audio: Error: Cannot set hw parameters (%s)", snd_strerror (err));
        return false;
    }

    snd_pcm_hw_params_free (hwparams);

    /* Set the sw parameters */
    snd_pcm_sw_params_malloc (&swparams);
    snd_pcm_sw_params_current (pcm_handle, swparams);

    /* Set the start threshold */

    if ((err = snd_pcm_sw_params_set_start_threshold(pcm_handle, swparams, periodSize_ * 2)) < 0) {
        _debugAlsa ("Audio: Error: Cannot set start threshold (%s)", snd_strerror (err));
        return false;
    }

    if ((err = snd_pcm_sw_params (pcm_handle, swparams)) < 0) {
        _debugAlsa ("Audio: Error: Cannot set sw parameters (%s)", snd_strerror (err));
        return false;
    }

    snd_pcm_sw_params_free (swparams);
    return true;
}


bool
AlsaLayer::open_device (std::string pcm_p, std::string pcm_c, std::string pcm_r, int flag)
{
    if (flag == SFL_PCM_BOTH or flag == SFL_PCM_PLAYBACK) {

        _debug ("Audio: Open playback device (and ringtone)");

        int err;
        if ((err = snd_pcm_open (&playbackHandle_, pcm_p.c_str(), SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
            _warn ("Audio: Error while opening playback device %s",  pcm_p.c_str());
            setErrorMessage (ALSA_PLAYBACK_DEVICE);
            is_playback_open_ = false;
            return false;
        }

        if (!alsa_set_params (playbackHandle_, 1)) {
            _warn ("Audio: Error: Playback failed");
            snd_pcm_close (playbackHandle_);
            is_playback_open_ = false;
            return false;
        }

        if (getIndexOut() != getIndexRing()) {

            if ((err = snd_pcm_open (&ringtoneHandle_, pcm_r.c_str(), SND_PCM_STREAM_PLAYBACK, 0)) < 0)
                _warn ("Audio: Error: Opening ringtone device %s", pcm_r.c_str());

            if (!alsa_set_params (ringtoneHandle_, 1)) {
                _warn ("Audio: Error: Ringtone failed");
                snd_pcm_close (ringtoneHandle_);
            }
        }

        is_playback_open_ = true;
    }

    if (flag == SFL_PCM_BOTH or flag == SFL_PCM_CAPTURE) {

        _debug ("Audio: Open capture device");

        if (snd_pcm_open (&captureHandle_,  pcm_c.c_str(),  SND_PCM_STREAM_CAPTURE, 0) < 0) {
            _warn ("Audio: Error: Opening capture device %s",  pcm_c.c_str());

            setErrorMessage (ALSA_CAPTURE_DEVICE);
            is_capture_open_ = false;
            return false;
        }

        if (!alsa_set_params (captureHandle_, 0)) {
            _warn ("Audio: Error: Capture failed");
            snd_pcm_close (captureHandle_);
            is_capture_open_ = false;
            return false;
        }

        is_capture_open_ = true;
    }

    return true;
}

//TODO	first frame causes broken pipe (underrun) because not enough data are send --> make the handle wait to be ready
int
AlsaLayer::write (void* buffer, int length, snd_pcm_t * handle)
{
    if (trigger_request_) {
        trigger_request_ = false;
        startPlaybackStream ();
    }

    snd_pcm_uframes_t frames = snd_pcm_bytes_to_frames (handle, length);

    int err;
    if ((err = snd_pcm_writei (handle, buffer , frames)) < 0) {
        switch (err) {

            case -EPIPE:
            case -ESTRPIPE:
            case -EIO:
                handle_xrun_playback (handle);

                if (snd_pcm_writei (handle, buffer , frames) < 0)
                    _debugAlsa ("Audio: XRUN handling failed");

                trigger_request_ = true;

                break;

            default:
                _debugAlsa ("Audio: Write error unknown - dropping frames: %s", snd_strerror (err));
                stopPlaybackStream ();
                break;
        }
    }

    return (err > 0) ? err : 0 ;
}

int
AlsaLayer::read (void* buffer, int toCopy)
{
    if (snd_pcm_state (captureHandle_) == SND_PCM_STATE_XRUN) {
        prepareCaptureStream ();
        startCaptureStream ();
    }

    snd_pcm_uframes_t frames = snd_pcm_bytes_to_frames (captureHandle_, toCopy);

    int err;
    if ((err = snd_pcm_readi (captureHandle_, buffer, frames)) < 0) {
        switch (err) {

            case -EPIPE:
            case -ESTRPIPE:
            case -EIO:
                _debugAlsa ("Audio: XRUN capture ignored (%s)", snd_strerror (err));
                handle_xrun_capture();
                break;

            case EPERM:
                _debugAlsa ("Audio: Capture EPERM (%s)", snd_strerror (err));
                prepareCaptureStream ();
                startCaptureStream ();
                break;

            default:
                break;
        }

        return 0;
    }

    return toCopy;
}

void
AlsaLayer::handle_xrun_capture (void)
{
    _debugAlsa ("Audio: Handle xrun capture");

    snd_pcm_status_t* status;
    snd_pcm_status_alloca (&status);

    if (snd_pcm_status(captureHandle_, status) <= 0) {
        if (snd_pcm_status_get_state (status) == SND_PCM_STATE_XRUN) {
            stopCaptureStream ();
            prepareCaptureStream ();
            startCaptureStream ();
        }
    } else
        _debugAlsa ("Audio: Get status failed");
}

void
AlsaLayer::handle_xrun_playback (snd_pcm_t *handle)
{
    _debugAlsa ("Audio: Handle xrun playback");

    snd_pcm_status_t* status;
    snd_pcm_status_alloca (&status);

    int state;
    if ((state = snd_pcm_status (handle, status)) < 0)
        _debugAlsa ("Audio: Error: Cannot get playback handle status (%s)" , snd_strerror (state));
    else {
        int state = snd_pcm_status_get_state (status);

        if (state  == SND_PCM_STATE_XRUN) {
            _debug ("Audio: audio device in state SND_PCM_STATE_XRUN, restart device");
            stopPlaybackStream ();
            preparePlaybackStream ();

            trigger_request_ = true;
        }
    }
}

std::string
AlsaLayer::buildDeviceTopo (const std::string &plugin, int card, int subdevice)
{
    std::stringstream ss, ss1;
    std::string pcm(plugin);

    if (pcm == PCM_DEFAULT)
        return pcm;

    ss << card;

    pcm.append (":");

    pcm.append (ss.str());

    if (subdevice != 0) {
        pcm.append (",");
        ss1 << subdevice;
        pcm.append (ss1.str());
    }

    _debug ("Audio: Device topo: %s", pcm.c_str());

    return pcm;
}

std::vector<std::string>
AlsaLayer::getSoundCardsInfo (int stream)
{
    _debug ("Audio: Get sound cards info: ");

    snd_ctl_t* handle;
    snd_ctl_card_info_t *info;
    snd_pcm_info_t* pcminfo;
    snd_ctl_card_info_alloca (&info);
    snd_pcm_info_alloca (&pcminfo);

    int numCard = -1 ;
    std::string description;

    std::vector<std::string> cards_id;
    if (snd_card_next (&numCard) < 0 || numCard < 0)
        return cards_id;

    while (numCard >= 0) {
        std::stringstream ss;
        ss << numCard;
        std::string name= "hw:";
        name.append (ss.str());

        if (snd_ctl_open (&handle, name.c_str(), 0) == 0) {
            if (snd_ctl_card_info (handle, info) == 0) {
                snd_pcm_info_set_device (pcminfo , 0);
                snd_pcm_info_set_stream (pcminfo, (stream == SFL_PCM_CAPTURE) ? SND_PCM_STREAM_CAPTURE : SND_PCM_STREAM_PLAYBACK);

                if (snd_ctl_pcm_info (handle ,pcminfo) < 0) _debugAlsa (" Cannot get info");
                else {
                    _debugAlsa ("card %i : %s [%s]",
                                numCard,
                                snd_ctl_card_info_get_id (info),
                                snd_ctl_card_info_get_name (info));
                    description = snd_ctl_card_info_get_name (info);
                    description.append (" - ");
                    description.append (snd_pcm_info_get_name (pcminfo));
                    cards_id.push_back (description);
                    // The number of the sound card is associated with a string description
                    IDSoundCards_.push_back(HwIDPair(numCard , description));
                }
            }

            snd_ctl_close (handle);
        }

        if (snd_card_next (&numCard) < 0)
            break;
    }

    return cards_id;
}


bool
AlsaLayer::soundCardIndexExist (int card, int stream)
{
    snd_ctl_t* handle;
    snd_pcm_info_t *pcminfo;
    snd_pcm_info_alloca (&pcminfo);
    std::string name = "hw:";
    std::stringstream ss;
    ss << card ;
    name.append (ss.str());

    if (snd_ctl_open (&handle, name.c_str(), 0) != 0)
		return false;

    snd_pcm_info_set_stream (pcminfo , (stream == SFL_PCM_PLAYBACK) ? SND_PCM_STREAM_PLAYBACK : SND_PCM_STREAM_CAPTURE);
	bool ret = snd_ctl_pcm_info (handle , pcminfo) >= 0;
	snd_ctl_close(handle);
	return ret;
}

int
AlsaLayer::soundCardGetIndex (const std::string &description)
{
    for (std::vector<HwIDPair>::const_iterator iter = IDSoundCards_.begin(); iter != IDSoundCards_.end(); ++iter)
        if (iter->second == description)
            return iter->first;

    // else return the default one
    return 0;
}

namespace
{
    void adjustVolume (SFLDataFormat *src , int samples, int volumePercentage)
    {
        if (volumePercentage != 100)
            for (int i = 0 ; i < samples; i++)
                src[i] = src[i] * volumePercentage * 0.01;
    }
}

void AlsaLayer::audioCallback (void)
{
    unsigned int _mainBufferSampleRate = getMainBuffer()->getInternalSamplingRate();
    bool resample = _audioSampleRate != _mainBufferSampleRate;

    notifyincomingCall();

    unsigned short spkrVolume = _manager->getSpkrVolume();

    AudioLoop *tone = _manager->getTelephoneTone();
    AudioLoop *file_tone = _manager->getTelephoneFile();

    // AvailForGet tell the number of chars inside the buffer
    // framePerBuffer are the number of data for one channel (left)
    int urgentAvailBytes = _urgentRingBuffer.AvailForGet();

    if (!playbackHandle_ or !captureHandle_)
        return;

    snd_pcm_wait (playbackHandle_, 20);

    int playbackAvailSmpl = snd_pcm_avail_update (playbackHandle_);
    int playbackAvailBytes = playbackAvailSmpl * sizeof (SFLDataFormat);

    if (urgentAvailBytes > 0) {
        // Urgent data (dtmf, incoming call signal) come first.
        int toGet = urgentAvailBytes;
        if (toGet > playbackAvailBytes)
			toGet = playbackAvailBytes;
        SFLDataFormat *out = (SFLDataFormat*) malloc (toGet);
		_urgentRingBuffer.Get (out, toGet);
        adjustVolume(out, toGet / sizeof(SFLDataFormat), spkrVolume);

		write (out, toGet, playbackHandle_);
		free (out);
        // Consume the regular one as well (same amount of bytes)
        getMainBuffer()->discard (toGet);
    } else {
    	// regular audio data
        int toGet = getMainBuffer()->availForGet();

        if (toGet <= 0) {
        	// no audio available, play tone or silence
			SFLDataFormat *out = (SFLDataFormat *) malloc (playbackAvailBytes);

			if (tone)
				tone->getNext (out, playbackAvailSmpl, spkrVolume);
			else if (file_tone && !ringtoneHandle_)
				file_tone->getNext (out, playbackAvailSmpl, spkrVolume);
			else
				memset(out, 0, playbackAvailBytes);

            write (out, playbackAvailBytes, playbackHandle_);
			free (out);
        } else {
        	// play the regular sound samples

            int maxNbBytesToGet = playbackAvailBytes;
            // Compute maximal value to get from the ring buffer
            double resampleFactor = 1.0;
            if (resample) {
                resampleFactor = (double) _audioSampleRate / _mainBufferSampleRate;
                maxNbBytesToGet = (double) toGet / resampleFactor;
            }

            if (toGet > maxNbBytesToGet)
            	toGet = maxNbBytesToGet;

			SFLDataFormat *out = (SFLDataFormat*) malloc (toGet);
			getMainBuffer()->getData (out, toGet);
            adjustVolume(out, toGet / sizeof(SFLDataFormat), spkrVolume);

			if (resample) {
				int inSamples = toGet / sizeof(SFLDataFormat);
				int outSamples = inSamples * resampleFactor;
				SFLDataFormat *rsmpl_out = (SFLDataFormat*) malloc (outSamples * sizeof(SFLDataFormat));
				converter_->resample (out, rsmpl_out, _mainBufferSampleRate, _audioSampleRate, inSamples);
				write (rsmpl_out, outSamples * sizeof(SFLDataFormat), playbackHandle_);
				free (rsmpl_out);
			} else {
				write (out, toGet, playbackHandle_);
			}
			free (out);
        }
    }

    if (file_tone and ringtoneHandle_) {
        int ringtoneAvailSmpl = snd_pcm_avail_update (ringtoneHandle_);
        int ringtoneAvailBytes = ringtoneAvailSmpl*sizeof (SFLDataFormat);
        SFLDataFormat *out = (SFLDataFormat *) malloc (ringtoneAvailBytes);
		file_tone->getNext (out, ringtoneAvailSmpl, spkrVolume);
		write (out, ringtoneAvailBytes, ringtoneHandle_);
		free (out);
    } else if (ringtoneHandle_) {
        int ringtoneAvailSmpl = snd_pcm_avail_update (ringtoneHandle_);
        int ringtoneAvailBytes = ringtoneAvailSmpl*sizeof (SFLDataFormat);

        SFLDataFormat *out = (SFLDataFormat *) malloc (ringtoneAvailBytes);
		memset (out, 0, ringtoneAvailBytes);
		write (out, ringtoneAvailBytes, ringtoneHandle_);
		free (out);
    }

    // Additionally handle the mic's audio stream
    if (!is_capture_running_)
        return;

    int toPutSamples = snd_pcm_avail_update (captureHandle_);
    if (toPutSamples <= 0) {
        if (toPutSamples < 0)
        	_error ("Audio: Mic error: %s", snd_strerror (toPutSamples));
        return;
    }
    
    const int framesPerBufferAlsa = 2048;
    if (toPutSamples > framesPerBufferAlsa)
    	toPutSamples = framesPerBufferAlsa;

    int toPutBytes = toPutSamples * sizeof(SFLDataFormat);
    SFLDataFormat* in = (SFLDataFormat*) malloc (toPutBytes);
    int bytes = read (in, toPutBytes);
    if (toPutBytes != bytes) {
    	_error("ALSA MIC : Couldn't read!");
    	free(in);
    	return;
    }
    adjustVolume (in, toPutSamples, _manager->getSpkrVolume());

    if (resample) {
    	int outSamples = toPutSamples * ((double) _audioSampleRate / _mainBufferSampleRate);
    	int outBytes = outSamples * sizeof (SFLDataFormat);
        SFLDataFormat* rsmpl_out = (SFLDataFormat*) malloc (outBytes);
        converter_->resample ( (SFLDataFormat*) in, rsmpl_out, _mainBufferSampleRate, _audioSampleRate, toPutSamples);
        _audiofilter->processAudio (rsmpl_out, outBytes);
        getMainBuffer()->putData (rsmpl_out, outBytes);
        free (rsmpl_out);
    } else {
        SFLDataFormat* filter_out = (SFLDataFormat*) malloc (toPutBytes);
		_audiofilter->processAudio (in, filter_out, toPutBytes);
		getMainBuffer()->putData (filter_out, toPutBytes);
		free (filter_out);
    }

    free (in);
}
