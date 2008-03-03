/**
 *  VideoTrpTTX Class
 * 
 * This class is a thread of execution for sending and receiving video data.
 * It has to interact with local capture (V4l) the mixer and the RTPsessions.
 * 
 */

#ifndef VIDEORTPRTX_H
#define VIDEORTPRTX_H

#include "VideoCodec/VideoCodec.h"
#include "VideoRtp.h"
#include <cc++/thread.h>
#include <ccrtp/rtp.h>
class SIPCall;
/**
 * @author Jean-Francois Blanchard-Dionne 
 */
class VideoRtpRTX : public ost::Thread, public ost::TimerPort {
public:

	/**
	 * Destructor
	 */ 	
    ~VideoRtpRTX();
	/**
	 * Default Constructor
	 */ 
    VideoRtpRTX();
	/**
	 * Main function to init RTPSession, send and receive data
	 */ 
    void run();
	/**
	 * Function to init buffer size
	 */ 
    void initBuffers();
	/**
	 * Function to create RTP Session to send Video Packets
	 */ 
    void initVideoRtpSession();
private:

	/**
	 * Get the data from V4l, send it to the mixer, encode and send to RTP
	 * @param timestamp : puts the current time
	 */ 		 	
	void sendSession(int timestamp);

	/**
	 * Receive RTP packet, decode it, send it to mixer
	 */		 	
	void receiveSession();

	/**
	 * Load  a codec  
	 * @param id : The ID of the codec you want to load
	 * @param type : 0 decode codec, 1 encode codec
	 */
	void loadCodec(enum CodecID id,int type);

	/**
	 * unloadCodec 
	 * @param id : The ID of the codec you want to unload
	 * @param type : 0 decode codec, 1 encode codec
	 */
	void unloadCodec(enum CodecID id,int type);

private:

	
    ost::Mutex          threadMutex;
    SIPCall* 			vidCall;
    /** RTP Session to send */
    ost::RTPSession* 	videoSessionSend;
    /** RTP Session to receive */
    ost::RTPSession* 	videoSessionReceive;
    /** System Semaphore */
    ost::Semaphore 		start;
    /** Codec for encoding */
    VideoCodec* 		encodeCodec;
    /** Codec for decoding */
    VideoCodec* 		decodeCodec;

	/** buffer for received Data */
	char* receiveDataDecoded;

	/** Buffer for Data to send */
	char* sendDataEncoded;
};
#endif //VIDEORTPRTX_H
