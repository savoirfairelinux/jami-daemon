#ifndef TESTFRAMEINPUT_H

// System includes
#include <cstdint>
#include <memory>

// LibML headers
#include "framelistener.h"

class TestFrameInput : public FrameListener {
public:
  TestFrameInput();
  virtual ~TestFrameInput() override;

  void onNewFrame(const AVFrame *frame) override;
  void copyOriginalFrame(const AVFrame *frame);
  void transformFrame(std::shared_ptr<SwsContext> context,
                      const AVFrame *inputFrame, AVFrame *outputFrame);
  void saveFrameAsPPM(const AVFrame *frame, const std::string &filename) const;
  std::shared_ptr<AVFrame> createFrame(const unsigned int width,
                                       const unsigned int height,
                                       const AVPixelFormat format);
  std::shared_ptr<SwsContext> createContext(const unsigned int inputdWidth,
                                            const unsigned int inputHeight,
                                            const AVPixelFormat inputFormat,
                                            const unsigned int outputWidth,
                                            const unsigned int outputHeight,
                                            const AVPixelFormat outputFormat);
  std::vector<uint8_t> toInterleavedRGB(const AVFrame *frame) const;
  std::vector<uint8_t> toNonInterleavedRGB(const AVFrame *frame) const;

private:
  bool firstRun = true;
  // This frame is a copy of the original but in RGB format
  std::shared_ptr<AVFrame> originalFrameRGB;
  std::shared_ptr<SwsContext> originalRGBContext;
  // This frame is a resized version of the original in RGB format
  std::shared_ptr<AVFrame> resizedFrameRGB;
  std::shared_ptr<SwsContext> resizeRGBContext;
};

#endif // TESTFRAMEINPUT_H
