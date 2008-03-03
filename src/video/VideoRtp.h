/**
 *  VideoRTP Class
 * 
 * This class is responsible of creating the thread for getting and setting
 * video data in a conversation and add an other thread in case Local SFLPhone
 * is a Conference server
 */

#ifndef VIDEORTP_H
#define VIDEORTP_H

#include "VideoRtpRTX.h"
#include <cc++/thread.h>
#include <samplerate.h>
class VideoRtpRTX;
/**
 * @author Jean-Francois Blanchard-Dionne 
 */
class VideoRtp {
public:
	/**
	 * Destructor
	 */ 	
    ~VideoRtp();
	/**
	 * Default Constructor
	 */ 	
    VideoRtp();

	/**
	 * Function to create a new Vrtxthread
	 * @param conf is 0 to create a a conference video session thread
	 * note : must have an initial normal thread going on
	 * @return 0 if success , -1 if failure
	 */ 
    int createNewVideoSession(bool conf);
	/**
	 * Function to close a Vrtxthread
	 * @param conf is 0 to create a a conference video session thread
	 * note : must have an initial normal thread going on
	 *@return 0 if success , -1 if failure
	 */ 
    int closeVideoSession(bool conf);

private:
    VideoRtpRTX* vRTXThread;
    VideoRtpRTX* vRTXThreadConf;
    ost::Mutex vThreadMutex;
    
};

#endif //VIDEORTP_H
