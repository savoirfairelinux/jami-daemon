#include "audiobuffer.h"


AudioBuffer::AudioBuffer (void) {
	data = new unsigned char[BUF_SIZE];
	size = BUF_SIZE;
	realsize = size;
}

AudioBuffer::~AudioBuffer (void)
{
	delete[] data;
}

void AudioBuffer::resize (size_t newsize)
{
	if (newsize > realsize) {
		delete[] data;
		data = new unsigned char[newsize];
		size = newsize;
		realsize = newsize;
	} else {
		size = newsize;
	}
}
