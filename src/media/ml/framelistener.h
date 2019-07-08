#ifndef FRAMELISTENER_H
#define FRAMELISTENER_H
// For AVFRAME
#include "libav_deps.h"

class FrameListener {
public:
  virtual void onNewFrame(const AVFrame *frame) = 0;
  virtual ~FrameListener();
};
#endif // FRAMELISTENER_H
