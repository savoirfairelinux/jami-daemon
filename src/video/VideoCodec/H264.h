/**
 *  H264 Class
 * 
 * This class is a VideoCodec child class. Since the VideoCodec class is virtual and abstract
 * an H264 videoCodec will assumme encoding and decoding video data.
 * This class acts like an interface with the encoding and decoding methods of the h264
 *  libavcodec files
 * 
 */

#ifndef H264_H
#define H264_H
#include "VideoCodec.h"


/**
 * @author Jean-Francois Blanchard-Dionne 
 */
class H264 : public VideoCodec {
public:
	/**
     * Default Constructor
     * 
     */

    H264();
	/**
     * Default Destructor
     * 
     */
    ~H264();
	/**
     * Function to decode video information
     * @param in_buf the input buffer
     * @param width of the video frame
     * @param height of the video frame
     * @param out_buf the output buffer
     * 
     */
    int videoDecode(uint8_t *in_buf, int width, int height, uint8_t* out_buf  );
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
#endif //H264_H
