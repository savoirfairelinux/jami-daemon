/**
 *  Copyright (C) 2004 Savoir-Faire Linux inc.
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
#include "global.h"

#define MONO	1

AudioDriversOSS::AudioDriversOSS (void) : AudioDrivers () {
	audio_fd = -1;
	initDevice(AudioDrivers::ReadWrite);
}

AudioDriversOSS::~AudioDriversOSS (void) {
	if (audio_fd > 0) {
		this->closeDevice();
	}
}

int
AudioDriversOSS::resetDevice (void) {
	printf ("Resetting...");
	if (ioctl(audio_fd, SNDCTL_DSP_RESET) < 0) {
		perror("ioctl");
		return -1;
	}
	printf ("done\n");
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
		return -1;
	}

	// Open device in non-blocking mode
	audio_fd = open (AUDIO_DEVICE, oflag | O_NONBLOCK );
	if (audio_fd == -1) {
		printf ("ERROR: Open Failed\n");
		return -1;
	}

	// Remove O_NONBLOCK
	int flags = fcntl(audio_fd, F_GETFL) & ~O_NONBLOCK;
	fcntl (audio_fd, F_SETFL, flags);

	// Fragments : No limit (0x7FFF), 
	int frag = ( ( 0x7FFF << 16 ) | 7 );
	if (ioctl(audio_fd, SNDCTL_DSP_SETFRAGMENT, &frag)) {
		printf ("ERROR: SETFRAG %s\n", strerror(errno));
		return -1;
	}

	// Setup sample format 16 bit signed little endian
	int format;
	format = AFMT_S16_LE;

	if (ioctl(audio_fd, SNDCTL_DSP_SETFMT, &format) == -1) {
		printf("ERROR: SETFMT  %s\n", strerror(errno));
		return -1;
	}
	if (format != AFMT_S16_LE) {
		printf ("ERROR: Format not supported\n");
		return -1;
	}

	// Setup number of channels
	int channels = MONO;
	if (ioctl(audio_fd, SNDCTL_DSP_CHANNELS, &channels) == -1) {
		printf ("ERROR: DSP_STEREO %s\n", strerror(errno));
		return false;
	}
	if (channels != MONO) {
		printf ("ERROR: Unsupported Number of Channels\n");
		return false;
	}

	// Setup sampling rate
	int rate = SAMPLING_RATE;
	if (ioctl(audio_fd, SNDCTL_DSP_SPEED, &rate ) == -1 ) {
		printf ("ERROR: DSP_SPEED  %s\n", strerror(errno));
		return false;
	}

	if (rate != SAMPLING_RATE) {
		printf ("WARNING: driver rounded %d Hz request to %d Hz, off by %f%%\n"
				, 8000, rate, 100*((rate-8000)/8000.0));
	}

	// Buffering parameters
	audio_buf_info info;
	if (mode == WriteOnly) {
		if (ioctl(audio_fd, SNDCTL_DSP_GETOSPACE, &info) == -1) {
			printf ("ERROR: GETISPACE %s\n", strerror(errno));
			return false;
		} 
	} else {
		if (ioctl(audio_fd, SNDCTL_DSP_GETISPACE, &info ) == -1) {
			printf ("ERROR: GETOSPACE %s\n", strerror(errno));
			return false;
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
		printf ("ERROR: Open Failed\n");
		return false;
	}

	audio_buf_info info;
	if (ioctl(audio_fd, SNDCTL_DSP_GETISPACE, &info) == -1) {
		printf ("ERROR: GETISPACE  %s\n", strerror(errno));
		return false;
	}
//	audio_buf.resize (info.fragsize * sizeof(short));


	devstate = DeviceOpened;
	return true;
}

#if 0
#ifndef min
#define min(a,b)	(a<b?a:b)
#endif
int
AudioDriversOSS::readBuffer (void *buf, int read_bytes) {
    int 			read_len,
					available;
    audio_buf_info	info;

	/* Figure out how many bytes we can read before blocking... */
    ioctl(audio_fd, SNDCTL_DSP_GETISPACE, &info);
    available = min(info.bytes, read_bytes);

	printf("info=%d, read_bytes=%d, available=%d\n", info.bytes, read_bytes,
			available);
    read_len  = read (audio_fd, (char *)buf, available);
    if (read_len < 0) {
        perror("audio_read");
        return 0;
    }

    return read_len;
}
#endif

int
AudioDriversOSS::readBuffer (void *ptr, int bytes) {
	if( devstate != DeviceOpened ) {
		printf ("Device Not Open\n");
		return false;
	}

	//audio_buf.resize(bytes);
	ssize_t count = bytes;

//	unsigned char *buf;
//	buf = audio_buf.getData();
	ssize_t rc;

	rc = read (audio_fd, ptr, count);
	if (rc < 0) {
		printf ("rc < 0 read(): %s\n", strerror(errno));
	}
	
	else if (rc != count) {
		printf ("WARNING: asked microphone for %d got %d\n", count, rc);
	}

	return rc;
}

int
AudioDriversOSS::readBuffer (int bytes) {
	if( devstate != DeviceOpened ) {
		printf ("Device Not Open\n");
		return false;
	}

	audio_buf.resize(bytes);
	ssize_t count = bytes;

	short *buf;
	buf = (short*)audio_buf.getData();
	ssize_t rc;

	rc = read (audio_fd, buf, count);
	if (rc < 0) {
		printf ("rc < 0 read(): %s\n", strerror(errno));
	}
	
	else if (rc != count) {
		printf ("WARNING: asked microphone for %d got %d\n", count, rc);
	}

	return rc;
}

int
AudioDriversOSS::writeBuffer (void *ptr, int len) {
	if (devstate != DeviceOpened ) {
		printf ("Device Not Opened\n");
		return -1;
	}
	
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
		if ((a = write(audio_fd, ptr, len)) < 0) {
			printf ("write(): %s\n", strerror(errno));
			break;
		}
		if (a > 0) { 
			return a;
			break;
		}
	}
	return 1; 
}

int
AudioDriversOSS::writeBuffer (void) {
	if (devstate != DeviceOpened ) {
		printf ("Device Not Opened\n");
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
			printf ("write(): %s\n", strerror(errno));
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
		printf ("ERROR: readableBytes %s\n", strerror(errno));
		return 0;
	}

	return info.bytes;
}


