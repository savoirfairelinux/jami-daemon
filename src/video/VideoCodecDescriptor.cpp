#include "VideoCodecDescriptor.h"

 	VideoCodecDescriptor::~VideoCodecDescriptor(){}
	
    VideoCodecDescriptor::VideoCodecDescriptor(){}

    
    int VideoCodecDescriptor::setDefaultOrder(){
    
    return 1;
    }
    
    void VideoCodecDescriptor::init(){}
	
   
    bool VideoCodecDescriptor::isActive(enum CodecID id){
    
    return true;
    }

   
    int VideoCodecDescriptor::removeCodec(enum CodecID id){
    
    return 1;
    }

   
    int VideoCodecDescriptor::addCodec(enum CodecID id){
    return 1;}
	
    VideoCodecOrder& VideoCodecDescriptor::getActiveCodecs() { return activeCodecs; }
	
    void VideoCodecDescriptor::setActiveCodecs(VideoCodecOrder& activeC){}
	
    void VideoCodecDescriptor::setCodecMap(VideoCodecMap& codec){}
    
    int setDefaultOrder(){return 0;}
    
