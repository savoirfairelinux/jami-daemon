/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
 *  Author: Eloi Bail <eloi.bail@savoirfairelinux.com>
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
#include "webrtcaudioprocessing.h"
#include "ring_plugin.h"
#include "echocanceller.h"
#include "manager.h"
#include "audio/audiobuffer.h"
#include "audio/ringbufferpool.h"
#include "audio/resampler.h"

#include "logger.h"

#include <stdexcept>
#include <iostream>
#include <memory>
#include <array>
#include <stdio.h>

/** Normal volume (100%, 0 dB) */
#define PA_VOLUME_NORM ((uint32_t) 0x10000U)



WebrtcAudioProcessing::WebrtcAudioProcessing() : EchoCanceller(ECHO_CANCELLER_WEBRTC)
                                                 ,fDebug_("debug_echoCanceller.log")
{
    initWebRtcAudioProcessing();
}

WebrtcAudioProcessing::~WebrtcAudioProcessing()
{}

EchoCanceller*
WebrtcAudioProcessing::clone()
{
    return new WebrtcAudioProcessing;
}


bool
WebrtcAudioProcessing::initWebRtcAudioProcessing()
{
    /* copyright pulseaudio */
    RING_DBG("initWebRtcAudioProcessing ");
    auto& mainBuffer = ring::Manager::instance().getRingBufferPool();
    ecParams.audioFormat = std::make_shared<ring::AudioFormat>(mainBuffer.getInternalAudioFormat());
    webrtc::AudioProcessing *apm = NULL;
    bool hpf, ns, agc, dgc, mobile, cn;
    int rm = -1;

    hpf = DEFAULT_HIGH_PASS_FILTER;
    ns = DEFAULT_NOISE_SUPPRESSION;
    agc = DEFAULT_ANALOG_GAIN_CONTROL;
    dgc = agc ? false : DEFAULT_DIGITAL_GAIN_CONTROL;
    if (agc && dgc) {
        RING_ERR("You must pick only one between analog and digital gain control");
        goto fail;
    }

    mobile = DEFAULT_MOBILE;

    if (mobile) {
        if (ecParams.hasDriftCompensation) {
            RING_ERR("Can't use drift_compensation in mobile mode");
            goto fail;
        }

        cn = DEFAULT_COMFORT_NOISE;
    } else {
        /*
        if (pa_modargs_get_value(ma, "comfort_noise", NULL) || pa_modargs_get_value(ma, "routing_mode", NULL)) {
            pa_log("The routing_mode and comfort_noise options are only valid with mobile=true");
            goto fail;
        }*/
    }

    apm = webrtc::AudioProcessing::Create(0);

    apm->set_sample_rate_hz(ecParams.audioFormat->sample_rate);

    apm->set_num_channels(ecParams.audioFormat->nb_channels, ecParams.audioFormat->nb_channels);
    apm->set_num_reverse_channels(ecParams.audioFormat->nb_channels);


    if (hpf) {
        RING_DBG("Setting high pass filter");
        apm->high_pass_filter()->Enable(true);
    }

    if (!mobile) {
        if (ecParams.hasDriftCompensation) {
            RING_DBG("Setting Drift Compensation");
            apm->echo_cancellation()->set_device_sample_rate_hz(ecParams.audioFormat->sample_rate);
            apm->echo_cancellation()->enable_drift_compensation(true);
        } else {
            apm->echo_cancellation()->enable_drift_compensation(false);
        }

        RING_DBG("Enabling echo cancelling");
        apm->echo_cancellation()->Enable(true);
    } else {
        apm->echo_control_mobile()->set_routing_mode(static_cast<webrtc::EchoControlMobile::RoutingMode>(rm));
        apm->echo_control_mobile()->enable_comfort_noise(cn);
        apm->echo_control_mobile()->Enable(true);
    }

    if (ns) {
        RING_DBG("Setting noise suppression");
        apm->noise_suppression()->set_level(webrtc::NoiseSuppression::kHigh);
        apm->noise_suppression()->Enable(true);
    }

    if (agc || dgc) {
        if (mobile && rm <= webrtc::EchoControlMobile::kEarpiece) {
            /* Maybe this should be a knob, but we've got a lot of knobs already */
            apm->gain_control()->set_mode(webrtc::GainControl::kFixedDigital);
            ecParams.hasAGC = false;
        } else if (dgc) {
            RING_DBG("Setting DGC");
            apm->gain_control()->set_mode(webrtc::GainControl::kAdaptiveDigital);
            ecParams.hasAGC = false;
        } else {
            RING_DBG("Setting AGC");
            apm->gain_control()->set_mode(webrtc::GainControl::kAdaptiveAnalog);
            if ((apm->gain_control()->set_analog_level_limits(0, PA_VOLUME_NORM-1)) != apm->kNoError) {
                RING_ERR("Failed to initialise AGC");
                goto fail;
            }
            ecParams.hasAGC = true;
        }

        apm->gain_control()->Enable(true);
    }

    apm->voice_detection()->Enable(true);

    webRtcEchoParams_.apm = apm;

    /*ec->params.priv.webrtc.sample_spec = *out_ss;
    ec->params.priv.webrtc.blocksize = (uint64_t)pa_bytes_per_second(out_ss) * BLOCK_SIZE_US / PA_USEC_PER_SEC;*/
    //*nframes = ec->params.priv.webrtc.blocksize / pa_frame_size(out_ss);

    return true;

fail:

    RING_DBG("Failure in echo processer init");
    if (apm)
        webrtc::AudioProcessing::Destroy(apm);

    return false;

}

void
WebrtcAudioProcessing::setPlaybackSamples(const int16_t* in_samples, int16_t* out_samples, long* captureVolume)
{
    webrtc::AudioProcessing *apm = (webrtc::AudioProcessing*) webRtcEchoParams_.apm;
    webrtc::AudioFrame out_frame;
    /*
    const pa_sample_spec *ss = &ec->params.priv.webrtc.sample_spec;
    pa_cvolume v;
    */

    out_frame._audioChannel = ecParams.audioFormat->nb_channels;
    out_frame._frequencyInHz = ecParams.audioFormat->sample_rate;
    out_frame._payloadDataLengthInSamples =  ecParams.audioFormat->sample_rate / 100;
    memcpy(out_frame._payloadData, in_samples,  ecParams.blocksize_playback * sizeof(int16_t));

    fDebug_ << "PLAYBACK: sample_rate:" << ecParams.audioFormat->sample_rate << " _payloadDataLengthInSamples:" << out_frame._payloadDataLengthInSamples
        << " blocksize_playback:" << ecParams.blocksize_playback <<  "\n";

    if (ecParams.hasAGC) {
        //TODO:
        //pa_cvolume_init(&v);
        //pa_echo_canceller_get_capture_volume(ec, &v);
        //apm->gain_control()->set_stream_analog_level(*captureVolume);
        //printf("volume avant process : %d \n", captureVolume);
    }

    apm->set_stream_delay_ms(0);
    WebRtc_Word16* dataPtr = out_frame._payloadData;
#if 0
    fDebug_ << "---------BEFORE-------------- \n";
    for (unsigned j = 0; j < ecParams.blocksize_playback; j++)
        fDebug_ << *(dataPtr++) << " ";
    fDebug_ << " \n ----------------------------- \n";
#endif

#if 1
    int err = apm->ProcessStream(&out_frame);
    if (err != apm->kNoError)
    {
        switch(err) {
            case apm->kUnspecifiedError:
                RING_DBG("## kUnspecifiedError");
                break;
            case apm->kCreationFailedError:
                RING_DBG("## kCreationFailedError");
                break;
            case apm->kUnsupportedComponentError:
                RING_DBG("## kUnsupportedComponentError");
                break;
            case apm->kUnsupportedFunctionError:
                RING_DBG("## kUnsupportedFunctionError");
                break;
            case apm->kNullPointerError:
                RING_DBG("## kNullPointerError");
                break;
            case apm->kBadParameterError:
                RING_DBG("## kBadParameterError");
                break;
            case apm->kBadSampleRateError:
                RING_DBG("## kBadSampleRateError");
                break;
            case apm->kBadDataLengthError:
                RING_DBG("## kBadDataLengthError");
                break;
            case apm->kBadNumberChannelsError:
                RING_DBG("## kBadNumberChannelsError");
                break;
            case apm->kFileError:
                RING_DBG("## kFileError");
                break;
            case apm->kStreamParameterNotSetError:
                RING_DBG("## kStreamParameterNotSetError");
                break;
            case apm->kNotEnabledError:
                RING_DBG("## kNotEnabledError");
                break;
            default:
                RING_DBG("## unknown error %d", err);
                break;
        }


    }
#endif

#if 0
    dataPtr = out_frame._payloadData;
    fDebug_ << "---------AFTER----------------- \n";
    for (unsigned j = 0; j < ecParams.blocksize_playback; j++)
        fDebug_ << *(dataPtr++) << " ";
    fDebug_ << " \n ----------------------------- \n";
#endif


    if (ecParams.hasAGC) {
        //TODO
        //pa_cvolume_set(&v, ss->channels, apm->gain_control()->stream_analog_level());
        //pa_echo_canceller_set_capture_volume(ec, &v);
        //*captureVolume = apm->gain_control()->stream_analog_level();
        //printf("volume apres process : %d \n", captureVolume);
    }

    memcpy(out_samples, out_frame._payloadData, ecParams.blocksize_playback * sizeof(int16_t));
}

void
WebrtcAudioProcessing::setCapturedSamples(const int16_t* in_samples)
{
    webrtc::AudioProcessing *apm = (webrtc::AudioProcessing*) webRtcEchoParams_.apm;
    webrtc::AudioFrame play_frame;

    play_frame._audioChannel = ecParams.audioFormat->nb_channels;
    play_frame._frequencyInHz = ecParams.audioFormat->sample_rate;

    //play_frame._payloadDataLengthInSamples =  ecParams.audioFormat->getBytesPerFrame();
    play_frame._payloadDataLengthInSamples =  ecParams.audioFormat->sample_rate / 100;
    memcpy(play_frame._payloadData, in_samples, ecParams.blocksize_capture * sizeof(int16_t));

    fDebug_ << "CAPTURE : sample_rate:" << ecParams.audioFormat->sample_rate << " _payloadDataLengthInSamples:" << play_frame._payloadDataLengthInSamples
        << " blocksize_capture:" << ecParams.blocksize_capture <<  "\n";

    int err = apm->AnalyzeReverseStream(&play_frame);
    if (err != apm->kNoError)
    {
        switch(err) {
            case apm->kUnspecifiedError:
                RING_DBG("## kUnspecifiedError");
                break;
            case apm->kCreationFailedError:
                RING_DBG("## kCreationFailedError");
                break;
            case apm->kUnsupportedComponentError:
                RING_DBG("## kUnsupportedComponentError");
                break;
            case apm->kUnsupportedFunctionError:
                RING_DBG("## kUnsupportedFunctionError");
                break;
            case apm->kNullPointerError:
                RING_DBG("## kNullPointerError");
                break;
            case apm->kBadParameterError:
                RING_DBG("## kBadParameterError");
                break;
            case apm->kBadSampleRateError:
                RING_DBG("## kBadSampleRateError");
                break;
            case apm->kBadDataLengthError:
                RING_DBG("## kBadDataLengthError");
                break;
            case apm->kBadNumberChannelsError:
                RING_DBG("## kBadNumberChannelsError");
                break;
            case apm->kFileError:
                RING_DBG("## kFileError");
                break;
            case apm->kStreamParameterNotSetError:
                RING_DBG("## kStreamParameterNotSetError");
                break;
            case apm->kNotEnabledError:
                RING_DBG("## kNotEnabledError");
                break;
            default:
                RING_DBG("## unknown error %d", err);
                break;
        }


    }
}

void
WebrtcAudioProcessing::setDrift(const float val)
{
    if (ecParams.hasDriftCompensation) {
        //RING_DBG("drift = %f", val);
        //fDebug_ << val << "\n";
        webRtcEchoParams_.apm->echo_cancellation()->set_stream_drift_samples(val * ecParams.audioFormat->getBytesPerFrame());
        //webRtcEchoParams_.apm->echo_cancellation()->set_stream_drift_samples(val);
    }
}
