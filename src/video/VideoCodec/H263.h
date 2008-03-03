/**
 *  H263 Class
 * 
 * This class is a VideoCodec child class. Since the VideoCodec class is virtual and abstract
 * an H263 videoCodec will assumme encoding and decoding video data.
 * This class acts like an interface with the encoding and decoding methods of the h263
 *  libavcodec files
 * 
 */

#ifndef H263_H
#define H263_H
#include "VideoCodec.h"



class H263 : public VideoCodec {
public:

	/**
     * Default Constructor
     * 
     */
    H263();
	/**
     * Default Destructor
     * 
     */
    ~H263();
/**
     * Function to decode video information
     * @param in_buf the input buffer
     * @param width of the video frame
     * @param height of the video frame
     * @param out_buf the output buffer
     * 
     */
   int videoDecode(uint8_t *in_buf, int width, int height, uint8_t* out_buf );
	/**
     * Function to encode video information
     * @param width of the video frame
     * @param height of the video frame
     * @param buf the buffer to encode
     * @param size buffer size
     * 
     */
    int videoEncode(int width, int height, uint8_t* buf, unsigned int size);
};




#endif //H263_H

