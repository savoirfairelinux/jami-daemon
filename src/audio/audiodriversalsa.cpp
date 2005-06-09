/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author:  Jerome Oufella <jerome.oufella@savoirfairelinux.com> 
 *                                                                              
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
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
 */

//#if defined(AUDIO_ALSA)

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/soundcard.h>

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "audiodriversalsa.h"
#include "../global.h"

#define ALSA_DEVICE	"plughw:0,0"

AudioDriversALSA::AudioDriversALSA(DeviceMode mode, Error *error) : 
			AudioDrivers () {
	this->error = error;
	audio_hdl = (snd_pcm_t *) NULL;
	initDevice(mode);
}

AudioDriversALSA::~AudioDriversALSA (void) {
	/* Close the audio handle */
	this->closeDevice();
}

void
AudioDriversALSA::closeDevice (void) {
	if (audio_hdl != NULL) {
		snd_pcm_close (audio_hdl);
		audio_hdl = (snd_pcm_t *) NULL;
	}
}

int
AudioDriversALSA::initDevice (DeviceMode mode) {
	int	err;
		
	if (devstate == DeviceOpened) {
		error->errorName(DEVICE_ALREADY_OPEN, NULL);
		return -1;
	}
	
	// Open the audio device
	switch (mode) {
	case ReadOnly:
		/* Only read sound from the device */
		err = snd_pcm_open (&audio_hdl, ALSA_DEVICE,SND_PCM_STREAM_CAPTURE, 
				SND_PCM_NONBLOCK);
		break;

	case WriteOnly:
		/* Only write sound to the device */
		err = snd_pcm_open (&audio_hdl, ALSA_DEVICE,SND_PCM_STREAM_PLAYBACK,
				SND_PCM_NONBLOCK);	
		break;
	default:
		break;
	}

	if (err < 0) {
		_debug ("ERROR: ALSA/snd_pcm_open: Cannot open audio device (%s)\n",
						snd_strerror (err));
		error->errorName(OPEN_FAILED_DEVICE, NULL);
		return -1;
	}
	////////////////////////////////////////////////////////////////////////////
	// BEGIN DEVICE SETUP
	////////////////////////////////////////////////////////////////////////////
	// Allocate space for device configuration
	snd_pcm_hw_params_t	*hw_params;

	err = snd_pcm_hw_params_malloc (&hw_params);
	if (err < 0) {
		_debug ("Cannot allocate hardware parameter structure (%s)\n",
				 snd_strerror (err));
		error->errorName(PARAMETER_STRUCT_ERROR_ALSA, (char*)snd_strerror(err));
		return -1;
	}

	// Init hwparams with full configuration space 
	if ((err = snd_pcm_hw_params_any (audio_hdl, hw_params)) < 0) {
		_debug ("Cannot initialize hardware parameter structure (%s)\n",
				 snd_strerror (err));
		error->errorName(PARAMETER_STRUCT_ERROR_ALSA, (char*)snd_strerror(err));
		return -1;
	}

	err = snd_pcm_hw_params_set_access (audio_hdl, hw_params,
					SND_PCM_ACCESS_RW_INTERLEAVED);
	if (err < 0) {
		_debug ("Cannot set access type (%s)\n", snd_strerror (err));
		error->errorName(ACCESS_TYPE_ERROR_ALSA, (char*)snd_strerror(err));
		return -1;
	}
	
	// Set sample formats (Signed, 16Bits, little endian)
	err = snd_pcm_hw_params_set_format (audio_hdl, hw_params,
					SND_PCM_FORMAT_S16_LE);
	if (err < 0) {
		_debug ("Cannot set sample format (%s)\n", snd_strerror (err));
		error->errorName(SAMPLE_FORMAT_ERROR_ALSA, (char*)snd_strerror(err));
		return -1;
	} 
	
	unsigned int rate = SAMPLING_RATE;
	unsigned int exact_rate;

	exact_rate = rate;
	// Set sampling rate (8kHz)
	err = snd_pcm_hw_params_set_rate_near (audio_hdl, hw_params, 
			&exact_rate, 0);
	if (err < 0) {
		_debug ("Cannot set sample rate (%s)\n", snd_strerror (err));
		error->errorName(SAMPLE_RATE_ERROR_ALSA, (char*)snd_strerror(err));
		return -1;
	}
	if (exact_rate != rate) {
      _debug("The rate %d Hz is not supported by your hardware.\n ==> Using %d Hz instead.\n", rate, exact_rate);
    }
	
	// Set number of channels - Mono(1) or Stereo(2) 
	err = snd_pcm_hw_params_set_channels (audio_hdl, hw_params, MONO);
	if (err < 0) {
		_debug ("Cannot set channel count (%s)\n", snd_strerror (err));
		error->errorName(CHANNEL_ERROR_ALSA, (char*)snd_strerror(err));
		return -1;
	}

	// Apply previously setup parameters
	err = snd_pcm_hw_params (audio_hdl, hw_params);
	if (err < 0) {
		_debug ("Cannot set parameters (%s)\n", snd_strerror (err));
		error->errorName(PARAM_SETUP_ALSA, (char*)snd_strerror(err));
		return -1;
	}
		
	// Free temp variable used for configuration.
	snd_pcm_hw_params_free (hw_params);

	////////////////////////////////////////////////////////////////////////////
	// END DEVICE SETUP
	////////////////////////////////////////////////////////////////////////////

	// Success
	devstate = DeviceOpened;
	return 0;
}

int
AudioDriversALSA::writeBuffer (void) {
	if (devstate != DeviceOpened) {
		error->errorName(DEVICE_NOT_OPEN, NULL);
		return -1;
	}
	
	int rc;
	size_t count = audio_buf.getSize()/2;
	short* buf = (short *)audio_buf.getData();
	while (count > 0) {
		rc = snd_pcm_writei(audio_hdl, buf, count);
		snd_pcm_wait(audio_hdl, 1);
		if (rc == -EPIPE) {
			snd_pcm_prepare(audio_hdl);
		} else if (rc == -EAGAIN) {
			continue;
		} else if (rc < 0) {
			break; 
		}
		buf += rc;
		count -= rc;
	}	
	return rc;
}

int
AudioDriversALSA::readBuffer (void *ptr, int bytes) {
	if( devstate != DeviceOpened ) {
		error->errorName(DEVICE_NOT_OPEN, NULL);
		return -1;
	}
		 
	ssize_t count = bytes;
	ssize_t rc;
	do {
		rc = snd_pcm_readi(audio_hdl, (short*)ptr, count);
	} while (rc == -EAGAIN);
	if (rc == -EPIPE) {
			snd_pcm_prepare(audio_hdl);
			bzero(ptr, bytes);
			rc = 320;
	}
	if (rc != 320)
		rc = rc * 2;
	return rc;
}

int
AudioDriversALSA::resetDevice (void) {
	int err;

	_debug("Resetting...\n");
	if ((err = snd_pcm_drop(audio_hdl)) < 0) {
		_debug ("ALSA: drop() error: %s\n", snd_strerror (err));				
		error->errorName(DROP_ERROR_ALSA, (char*)snd_strerror(err));
		return -1;
	}

	if ((err = snd_pcm_prepare(audio_hdl)) < 0) {
		_debug ("ALSA: prepare() error: %s\n", snd_strerror (err));			
		error->errorName(PREPARE_ERROR_ALSA, (char*)snd_strerror(err));
		return -1;
	}
	return 0;
}

//#endif // defined(AUDIO_ALSA)
// EOF
