#ifndef FRAMERESIZER_H
#define FRAMERESIZER_H
#include "framelistener.h"
#include <chrono>
#include <memory>

class FrameResizer : public FrameListener {
public:
  FrameResizer();
  virtual ~FrameResizer() override;

  void onNewFrame(const jami::VideoFrame &frame) override;
  void onNewFrame(jami::VideoFrame &frame) override;
  void transformFrameToRGB(const AVFrame *frame);
  void createTransformedFrame(unsigned int width, unsigned int height);
  void resizeWithTensorflow(uint8_t *, unsigned char *, int, int, int, int, int,
                            int);
  void saveFrameAsPPM(const AVFrame *frame, const std::string &filename) const;
  std::unique_ptr<SwsContext, void (*)(SwsContext *)> resizeContext;
  std::unique_ptr<AVFrame, void (*)(AVFrame *)> originalFrameRGB;
  std::unique_ptr<uint_fast8_t, void (*)(uint_fast8_t *)>
      originalFrameRGBBuffer;
  unsigned int nbFrames = 0;
  std::chrono::steady_clock::duration delta;

private:
  int wantedWidth = 300;
  int wantedHeight = 300;
};

#endif // FRAMERESIZER_H
