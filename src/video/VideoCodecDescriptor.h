/**
 *  VideoCodecDescriptor Class
 * 
 * This class  acts like a container. It generates at startup a map of all
 * codecs, active or not, supported by the application and sets a vector for the order
 * of negotiation of the video codecs
 * Most functions in this class is for managing and accessing these two containers
 * This class is mainly to interact with the GUI about video Codecs matter.
 */


#ifndef VIDEOCODECDESCRIPTOR_H
#define VIDEOCODECDESCRIPTOR_H
#include <map>
#include <vector>
#include "/usr/include/ffmpeg/avcodec.h"
/**
 * @author Jean-francois Blanchard-dionne 
 */
/* A codec is identified by its libavcodec CodecID. A CodecID is associated with a name. */
typedef std::map<CodecID, std::string> VideoCodecMap;
/* The struct to reflect the order the user wants to use the Video codecs */
typedef std::vector<CodecID> VideoCodecOrder;

class VideoCodecDescriptor {
public:

	/**
	 * Destructor
     */
    ~VideoCodecDescriptor();
	/**
	 * Default Constructor
     */
    VideoCodecDescriptor();
    /**
     * Set the default codecs order
     */   
    int setDefaultOrder();
  
  
    void init();
	

    /**
     * Check in the map codec if the specified codec is supported 
     * @param id : libavcodec unique codecID
     * @return true if the codec specified is supported
     * 	     false otherwise
     */
    bool isActive(enum CodecID id);

    /**
     * Remove the codec from the list
     * @param id :  libavcodec CodecID of the codec to erase
     */ 
    int removeCodec(enum CodecID id);

    /**
     * Add a codec in the list.
     * @param id : libavcodec CodecID of the codec to add
     */
    int addCodec(enum CodecID id);
	/**
     * Function to send the map containing the active Codecs.
     * 
     */
    VideoCodecOrder& getActiveCodecs();
	/**
     * Function to set the map
     * @param activeC to set the Codec Map with another map
     * (not really suppose to happen)
     */
    void setActiveCodecs(VideoCodecOrder& activeC);
	/**
     * Function to set the map
     * @param codecMap to set the Codec Map
     * (not really suppose to happen)
     */
    void setCodecMap(VideoCodecMap& codecMap);
	/**
     * Function to get the map
     * @return codecMap to set the Codec Map
     */
   	VideoCodecMap& getCodecMap() { return codecMap; }

private:	
	/**
     * Vector of all the Active codecs
     */
    VideoCodecOrder activeCodecs;
    /**
     * Map of all codecs, active and inactive
     */
    VideoCodecMap codecMap;




};
#endif //VIDEOCODECDESCRIPTOR_H
