/**
 *  Copyright (C) 2004 Savoir-Faire Linux inc.
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
#include "global.h"

#define ALSA_DEVICE	"plughw:0,0"

AudioDriversALSA::AudioDriversALSA(DeviceMode mode) : AudioDrivers () {
	audio_hdl = (snd_pcm_t *) NULL;
	initDevice(mode);

}

AudioDriversALSA::~AudioDriversALSA (void) {
	/* Close the audio handle */
	if (audio_hdl != NULL) snd_pcm_close (audio_hdl);
}


int
AudioDriversALSA::initDevice (DeviceMode mode) {
	int	 err;
		
	if (devstate == DeviceOpened) {
		printf ("ERROR: ALSA Device Already Open !\n");
		return -1;
	}
	
	// Open the audio device
	// Flags : blocking (else have to OR omode with SND_PCM_NONBLOCK).
	switch (mode) {
	case ReadOnly:
		/* Only read sound from the device */
		err = snd_pcm_open (&audio_hdl, ALSA_DEVICE,SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);
		break;

	case WriteOnly:
		/* Only write sound to the device */
		err = snd_pcm_open (&audio_hdl, ALSA_DEVICE,SND_PCM_STREAM_PLAYBACK,SND_PCM_NONBLOCK);	
		break;
	default:
		break;
	}

	if (err < 0) {
		printf ("ERROR: ALSA/snd_pcm_open: Cannot open audio device (%s)\n",
						snd_strerror (err));
		return -1;
	}
	////////////////////////////////////////////////////////////////////////////
	// BEGIN DEVICE SETUP
	////////////////////////////////////////////////////////////////////////////
	// Allocate space for device configuration
	snd_pcm_hw_params_t	*hw_params;

	err = snd_pcm_hw_params_malloc (&hw_params);
	if (err < 0) {
		printf ("Cannot allocate hardware parameter structure (%s)\n",
				 snd_strerror (err));
		return -1;
	}

	// Init hwparams with full configuration space 
	if ((err = snd_pcm_hw_params_any (audio_hdl, hw_params)) < 0) {
		printf ("Cannot initialize hardware parameter structure (%s)\n",
				 snd_strerror (err));
		return -1;
	}

	err = snd_pcm_hw_params_set_access (audio_hdl, hw_params,
					SND_PCM_ACCESS_RW_INTERLEAVED);
	if (err < 0) {
		printf ("Cannot set access type (%s)\n", snd_strerror (err));
	}
	
	// Set sample formats (Signed, 16Bits, little endian)
	err = snd_pcm_hw_params_set_format (audio_hdl, hw_params,
					SND_PCM_FORMAT_S16_LE);
	if (err < 0) {
		printf ("Cannot set sample format (%s)\n", snd_strerror (err));
		return -1;
	} 
	
	unsigned int rate = SAMPLING_RATE;
	unsigned int exact_rate;

	exact_rate = rate;
	// Set sampling rate (8kHz)
	err = snd_pcm_hw_params_set_rate_near (audio_hdl, hw_params, 
			&exact_rate, 0);
	if (err < 0) {
		printf ("Cannot set sample rate (%s)\n", snd_strerror (err));
		return -1;
	}
	if (exact_rate != rate) {
      fprintf(stderr, "The rate %d Hz is not supported by your hardware.\n ==> Using %d Hz instead.\n", rate, exact_rate);
    }
	
	// Set number of channels - Mono(1) or Stereo(2) 
	err = snd_pcm_hw_params_set_channels (audio_hdl, hw_params, MONO);
	if (err < 0) {
		printf ("Cannot set channel count (%s)\n", snd_strerror (err));
		return -1;
	}
	
	// Apply previously setup parameters
	err = snd_pcm_hw_params (audio_hdl, hw_params);
	if (err < 0) {
		printf ("Cannot set parameters (%s)\n", snd_strerror (err));
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
		printf ("ALSA: writeBuffer(): Device Not Open\n");
		return -1;
	}

#if 1
	int rc;
	size_t count = audio_buf.getSize()/2;
	short* buf = (short *)audio_buf.getData();
	while (count > 0) {
		rc = snd_pcm_writei(audio_hdl, buf, count);
		if (rc == -EPIPE) {
			snd_pcm_prepare(audio_hdl);
		} else if (rc == -EAGAIN) {
			continue;
		} else if (rc < 0) {
			printf ("ALSA: write(): %s\n", strerror(errno));
			break; 
		}
		printf("rc = %d\n",rc);
		buf += rc;
		count -= rc;
	}	
	return rc;
#endif
}

unsigned int
AudioDriversALSA::readableBytes (void) {
	audio_buf_info info;
#if 0
	struct timeval timeout;
	fd_set read_fds;

	if (devstate != DeviceOpened) {
		return 0;
	}
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	FD_ZERO (&read_fds);
	FD_SET (audio_fd, &read_fds);
	if (select (audio_fd + 1, &read_fds, NULL, NULL, &timeout) == -1) {
		return 0;
	}
	if (!FD_ISSET ( audio_fd, &read_fds)) {
		return 0;
	}
	if (ioctl (audio_fd, SNDCTL_DSP_GETISPACE, &info) == -1) {
		printf ("ERROR: readableBytes %s\n", strerror(errno));		
		return 0;
	}
#endif
	return info.bytes;
}


int
AudioDriversALSA::readBuffer (int bytes) {
/*	if (devstate != DeviceOpened) {
		printf ("Device Not Open\n");
		return false;
	}

	audio_buf.resize (bytes);
	size_t count = bytes;

	void *buf;
	buf = audio_buf.getData ();
	size_t rc = read (audio_fd, buf, count);
	if (rc != count) {
		printf ("warning: asked microphone for %d got %d\n", count, rc);
	}*/
	return true;
}

int
AudioDriversALSA::readBuffer (void *ptr, int bytes) {
	if( devstate != DeviceOpened ) {
		printf ("ALSA: readBuffer(): Device Not Open\n");
		return -1;
	}

#if 1	
	ssize_t count = bytes/2;
	ssize_t rc;
	
	do {
		rc = snd_pcm_readi(audio_hdl, (short*)ptr, count);
	} while (rc == -EAGAIN);
	if (rc == -EBADFD) printf ("Read: PCM is not in the right state\n");
	if (rc == -ESTRPIPE) printf ("Read: a suspend event occurred\n");
	if (rc == -EPIPE) {
			printf ("Read: -EPIPE %d\n", rc);
			snd_pcm_prepare(audio_hdl);
	}
	if (rc > 0 && rc != count) {
		printf("Read: warning: asked microphone for %d frames but got %d\n",
			count, rc);
	}
	
	return rc;
#endif
}

int
AudioDriversALSA::resetDevice (void) {
/*	printf ("ALSA: Resetting device.\n");
	snd_pcm_drop(audio_hdl);
	snd_pcm_drain(audio_hdl);*/
	return 0;
}

// EOF
