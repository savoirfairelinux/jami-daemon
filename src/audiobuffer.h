#ifndef __AUDIOBUFFER_H__
#define __AUDIOBUFFER_H__

#include <stddef.h>
#include <stdlib.h>

#define BUF_SIZE	4096

/**
 * Small class for passing around buffers of audio data.
 */
class AudioBuffer {
public:
	/**
	 * Creates an audio buffer of @param length bytes.
	 */
	AudioBuffer (void);

	/**
	 * Deletes the audio buffer, freeing the data.
	 */
	~AudioBuffer (void);

	/**
	 * Returns a pointer to the audio data.
	 */
	unsigned char *getData (void) {
		return data;
	}

	/**
	 * Returns the size of the buffer.
	 */
	size_t getSize (void) {
	   return size; 
	}

	/**
	 * Resizes the buffer to size newlength. Will only allocate new memory
	 * if the size is larger than what has been previously allocated.
	 */
	void resize (size_t newsize);

private:
	unsigned char *data;
	size_t size;
	size_t realsize;
};

#endif // __AUDIOBUFFER_H__
