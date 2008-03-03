/**
 *  H263p Class
 * 
 * This class is a VideoCodec child class. Since the VideoCodec class is virtual and abstract
 * an H263p videoCodec will assumme encoding and decoding video data.
 * This class acts like an interface with the encoding and decoding methods of the h263p
 *  libavcodec files
 * 
 */

#ifndef H263P_H
#define H263P_H
#include "VideoCodec.h"

/**
 * @author Jean-Francois Blanchard-Dionne */
class H263P : public VideoCodec {
public:
	/**
     * Default Constructor
     * 
     */

    H263P();
	/**
     * Default Destructor
     * 
     */
    ~H263P();
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
#endif //H263P_H
