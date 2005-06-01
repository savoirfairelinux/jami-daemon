/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com> 
 *
 * 	Portions Copyright (c) 2000 Billy Biggs <bbiggs@div8.net>
 *  Portions Copyright (c) 2004 Wirlab <kphone@wirlab.net>
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

#if defined(AUDIO_OSS)

#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/soundcard.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/unistd.h>

#include <stdio.h>

#include "audiodriversoss.h"
#include "../global.h"


AudioDriversOSS::AudioDriversOSS (DeviceMode mode, Error *error) : 
			AudioDrivers () {
	this->error = error;
	audio_fd = -1;
	initDevice(mode);
}

AudioDriversOSS::~AudioDriversOSS (void) {
	if (audio_fd > 0) {
		this->closeDevice();
	}
}

int
AudioDriversOSS::resetDevice (void) {
	_debug ("Resetting...\n");
	if (ioctl(audio_fd, SNDCTL_DSP_RESET) < 0) {
		perror("ioctl");
		return -1;
	}
	return 0;
}

int
AudioDriversOSS::initDevice (DeviceMode mode) {
	int oflag;
	switch (mode) {
	case ReadOnly:
		oflag = O_RDONLY;
		break;
	case WriteOnly:
		oflag = O_WRONLY;
		break;
	default:
		oflag = O_RDWR;
		break;
	}
	
	if (devstate == DeviceOpened) {
		error->errorName(DEVICE_ALREADY_OPEN, NULL);
		return -1;
	}

	// Open device in non-blocking mode
	audio_fd = open (AUDIO_DEVICE, oflag | O_NONBLOCK );
	if (audio_fd == -1) {
		error->errorName(OPEN_FAILED_DEVICE, NULL);	
		return -1;
	}  
 
	// Remove O_NONBLOCK
	int flags = fcntl(audio_fd, F_GETFL) & ~O_NONBLOCK;
	fcntl (audio_fd, F_SETFL, flags);
    
	// Fragments : No limit (0x7FFF), 
	int frag = ( ( 0x7FFF << 16 ) | 7 );
	if (ioctl(audio_fd, SNDCTL_DSP_SETFRAGMENT, &frag)) {
		_debug ("ERROR: SETFRAG %s\n", strerror(errno));
		error->errorName(FRAGMENT_ERROR_OSS, strerror(errno));
		return -1;
	}

	// Setup sample format 16 bit signed little endian
	int format;
	format = AFMT_S16_LE;

	if (ioctl(audio_fd, SNDCTL_DSP_SETFMT, &format) == -1) {
		_debug("ERROR: SETFMT  %s\n", strerror(errno));
		error->errorName(SAMPLE_FORMAT_ERROR_OSS, strerror(errno));
		return -1;
	}
	if (format != AFMT_S16_LE) {
		_debug ("ERROR: Format not supported\n");
		return -1;
	}

	// Setup number of channels
	int channels = MONO;
	if (ioctl(audio_fd, SNDCTL_DSP_CHANNELS, &channels) == -1) {
		_debug ("ERROR: DSP_STEREO %s\n", strerror(errno));
		error->errorName(CHANNEL_ERROR_OSS, strerror(errno));
		return -1;
	}
	if (channels != MONO) {
		_debug ("ERROR: Unsupported Number of Channels\n");
		return -1;
	}

	// Setup sampling rate 8KHz
	int rate = SAMPLING_RATE;
	if (ioctl(audio_fd, SNDCTL_DSP_SPEED, &rate ) == -1 ) {
		_debug ("ERROR: DSP_SPEED  %s\n", strerror(errno));
		error->errorName(SAMPLE_RATE_ERROR_OSS, strerror(errno));
		return -1;
	}

	if (rate != SAMPLING_RATE) {
		_debug ("WARNING: driver rounded %d Hz request to %d Hz, off by %f%%\n"
				, 8000, rate, 100*((rate-8000)/8000.0));
	}

	// Buffering parameters
	audio_buf_info info;
	if (mode == WriteOnly) {
		if (ioctl(audio_fd, SNDCTL_DSP_GETOSPACE, &info) == -1) {
			_debug ("ERROR: GETISPACE %s\n", strerror(errno));
			error->errorName(GETISPACE_ERROR_OSS, strerror(errno));
			return -1;
		} 
	} else {
		if (ioctl(audio_fd, SNDCTL_DSP_GETISPACE, &info ) == -1) {
			_debug ("ERROR: GETOSPACE %s\n", strerror(errno));
			error->errorName(GETOSPACE_ERROR_OSS, strerror(errno));
			return -1;
		}
	}
//	audio_buf.resize (info.fragsize * sizeof(short));
	devstate = DeviceOpened;

	return 0;
}

int
AudioDriversOSS::closeDevice (void) {
	close (audio_fd);
	audio_fd = -1;
	return 1;
}

bool
AudioDriversOSS::openDevice (int exist_fd) {
	audio_fd = exist_fd;
	if (audio_fd == -1) {
		_debug ("ERROR: Open Failed\n");
		return false;
	}

	audio_buf_info info;
	if (ioctl(audio_fd, SNDCTL_DSP_GETISPACE, &info) == -1) {
		_debug ("ERROR: GETISPACE  %s\n", strerror(errno));
		return false;
	}
//	audio_buf.resize (info.fragsize * sizeof(short));


	devstate = DeviceOpened;
	return true;
}


int
AudioDriversOSS::readBuffer (void *ptr, int bytes) {
	if( devstate != DeviceOpened ) {
		return false;
	}
	ssize_t count = bytes;
	ssize_t rc;
 
	rc = read (audio_fd, ptr, count);
	if (rc < 0) {
		_debug ("rc < 0 read(): %s\n", strerror(errno));
	}
	
	else if (rc != count) {
		_debug ("WARNING: asked microphone for %d got %d\n", count, rc);
	}

	return rc;
}


int
AudioDriversOSS::writeBuffer (void) {
	if (devstate != DeviceOpened ) {
		error->errorName(DEVICE_NOT_OPEN, NULL);
		return -1;
	}
	
	size_t count = audio_buf.getSize();
	short *buf = (short*)audio_buf.getData();

	audio_buf_info info;
	if (ioctl(audio_fd, SNDCTL_DSP_GETOSPACE, &info) == 0 ) {
		if (info.fragstotal - info.fragments > 15) {
			// drop the fragment if the buffer starts to fill up
			return 1;
		}
	}
	// Loop into write() while buffer not complete.
	for (;;) {
		int a;
		if ((a = write(audio_fd, buf, count)) < 0) {
			_debug ("write(): %s\n", strerror(errno));
			break;
		}
		if (a > 0) { 
			return a;
			break;
		}
	}
	return 1; 
}


unsigned int 
AudioDriversOSS::readableBytes(void) {
	audio_buf_info info;
	struct timeval timeout;
	fd_set read_fds;

	if (devstate != DeviceOpened) {
		return 0;
	}
	
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	FD_ZERO( &read_fds );
	FD_SET( audio_fd, &read_fds );
	
	if (select (audio_fd + 1, &read_fds, NULL, NULL, &timeout) == -1) {
		return 0;
	}
	if (!FD_ISSET (audio_fd, &read_fds)) {
		return 0;
	}
	if (ioctl (audio_fd, SNDCTL_DSP_GETISPACE, &info) == -1) {
		_debug ("ERROR: readableBytes %s\n", strerror(errno));
		return 0;
	}

	return info.bytes;
}

#endif // defined(AUDIO_OSS)

