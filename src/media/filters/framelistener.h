#ifndef FRAMELISTENER_H
#define FRAMELISTENER_H
// For AVFRAME
#include "libav_deps.h"
#include "video/video_base.h"
namespace jami {
class FrameListener {
public:
    virtual void onNewFrame(const VideoFrame &frame) = 0;
    virtual void onNewFrame(VideoFrame &frame) = 0;
    virtual ~FrameListener()  = default;
};
}
#endif // FRAMELISTENER_H
