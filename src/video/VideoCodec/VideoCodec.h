/**
 *  VideoCodec Class
 * 
 * This is the mother VideoCodec class. It's a virtual abstract class for encoding and 
 * decoding video data.
 */

#ifndef VIDEOCODEC_H
#define VIDEOCODEC_H
#include <ffmpeg/avcodec.h>
#include <ffmpeg/avformat.h>
/**
 * @author Jean-Francois Blanchard-Dionne */
class VideoCodec {
public:
/**
     * Function to decode video information
     * @param in_buf the input buffer
     * @param width of the video frame
     * @param height of the video frame
     * @param out_buf the output buffer
     * 
     */
    virtual int videoDecode(uint8_t *in_buf, int width, int height, uint8_t* out_buf ) =0;
/**
     * Function to encode video information
     * @param width of the video frame
     * @param height of the video frame
     * @param buf the buffer to encode
     * @param size buffer size
     * 
     */
    virtual int videoEncode(int width, int height, uint8_t* buf, unsigned int size) =0;

    void init();
	/**
     * Default Destructor
     * 
     */
    virtual ~VideoCodec() =0;
	/**
     * Default Constructor
     * 
     */
    VideoCodec();
private:
	/**
     * Libavcodec Codec type
     */
    AVCodec* Codec;
    /**
     * Libavcodec Codec context
     */
    AVCodecContext* codecCtx;
     /**
     * Libavcodec packet
     */
    AVPacket pkt;
     /**
     * Libavcodec frame
     */
    AVFrame* frame;
};
#endif //VIDEOCODEC_H

