#ifndef FRAMELISTENER_H
#define FRAMELISTENER_H
// For AVFRAME
#include "libav_deps.h"
#include "video/video_base.h"

class FrameListener {
public:
  virtual void onNewFrame(const jami::VideoFrame &frame) = 0;
  virtual void onNewFrame(jami::VideoFrame &frame) = 0;
  virtual ~FrameListener();
};
#endif // FRAMELISTENER_H
