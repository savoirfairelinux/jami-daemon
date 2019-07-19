#ifndef TESTFRAMEINPUT_H
#define TESTFRAMEINPUT_H
// System includes
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
// LibML headers
#include "framelistener.h"
// Library headers
#include "multipleobjecttracking.h"

class FrameCopy {
public:
  // This frame is a copy of the original but in RGB format
  std::shared_ptr<AVFrame> originalFrameRGB;
  // This frame is a resized version of the original in RGB format
  std::shared_ptr<AVFrame> resizedFrameRGB;

  std::shared_ptr<AVFrame> predictionsFrame;

  std::shared_ptr<SwsContext> originalRGBContext;
  std::shared_ptr<SwsContext> resizeRGBContext;

  std::shared_ptr<AVFrame> createFrame(const unsigned int width,
                                       const unsigned int height,
                                       const AVPixelFormat format);
  std::shared_ptr<SwsContext> createContext(const unsigned int inputdWidth,
                                            const unsigned int inputHeight,
                                            const AVPixelFormat inputFormat,
                                            const unsigned int outputWidth,
                                            const unsigned int outputHeight,
                                            const AVPixelFormat outputFormat);
  void createCopyFrames(const AVFrame *frame);
  void copyOriginalFrame(const AVFrame *frame);
  void transformFrame(std::shared_ptr<SwsContext> context,
                      const AVFrame *inputFrame, AVFrame *outputFrame);
  void saveFrameAsPPM(const AVFrame *frame, const std::string &filename) const;
  std::vector<uint8_t> toInterleavedRGB(const AVFrame *frame) const;
  std::vector<uint8_t> toNonInterleavedRGB(const AVFrame *frame) const;
};

class TestFrameInput : public FrameListener {
public:
  TestFrameInput();
  virtual ~TestFrameInput() override;

  void onNewFrame(const AVFrame *frame) override;
  void onNewFrame(AVFrame *frame) override;
  void feedInput();
  void computePredictions();
  std::shared_ptr<AVFrame> drawPredictionsOnCopyFrame(
      const AVFrame *frame,
      const std::vector<std::tuple<std::array<float, 4>, float, int>>
          &predictions);

private:
  FrameCopy fcopy;
  // Model being used
  MultipleObjectTracking mot;
  // Only display predictions with a probability threshold
  const float threshold = 0.4f;
  // Status variables of the processing
  bool firstRun = true;
  bool processing = false;
  bool newFrame = false;
  bool stopProcessingThread = false;
  //
  std::mutex frameMutex;
  std::condition_variable data_cond;
  // Processing thread
  std::thread processFrameThread;

  // Output preditctions
  std::vector<std::tuple<std::array<float, 4>, float, int>> computedPredictions;

  // Colors
  unsigned char colors[10][3] = {
      // Red
      {static_cast<unsigned char>(230), static_cast<unsigned char>(25),
       static_cast<unsigned char>(75)},
      // Orange
      {static_cast<unsigned char>(245), static_cast<unsigned char>(130),
       static_cast<unsigned char>(48)},
      // Yellow
      {static_cast<unsigned char>(255), static_cast<unsigned char>(255),
       static_cast<unsigned char>(25)},
      // Lime
      {static_cast<unsigned char>(210), static_cast<unsigned char>(245),
       static_cast<unsigned char>(25)},
      // Green
      {static_cast<unsigned char>(60), static_cast<unsigned char>(180),
       static_cast<unsigned char>(75)},
      // Cyan
      {static_cast<unsigned char>(70), static_cast<unsigned char>(240),
       static_cast<unsigned char>(240)},
      // Blue
      {static_cast<unsigned char>(0), static_cast<unsigned char>(130),
       static_cast<unsigned char>(200)},
      // Purple
      {static_cast<unsigned char>(145), static_cast<unsigned char>(30),
       static_cast<unsigned char>(180)},
      // Magenta
      {static_cast<unsigned char>(240), static_cast<unsigned char>(50),
       static_cast<unsigned char>(230)},
      // Apricot
      {static_cast<unsigned char>(255), static_cast<unsigned char>(215),
       static_cast<unsigned char>(180)},
  };
};

#endif // TESTFRAMEINPUT_H
