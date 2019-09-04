#pragma once
// System includes
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
// OpenCV headers
#include <opencv2/core.hpp>
// LibFilters headers
// For AVFRAME
#include "libav_deps.h"
// Jami Library headers
#include "dring/videomanager_interface.h"
#include "media/video/video_scaler.h"
#include "video/video_base.h"
// Filters
#include "multipleobjecttracking.h"
// Reactive Streams
#include "reactive_streams.h"
// Exchange video frame
#include "media/filters/ExVideoFrame.h"


namespace jami {
class FrameCopy {
public:
  // This frame is a resized version of the original in RGB format
  VideoFrame resizedFrameRGB;
  // This frame is used to draw predictions into in RGB format
  VideoFrame predictionsFrameBGR;
  // An instance of the scaler
  video::VideoScaler scaler;

  /**
   * @brief createCopyFrames
   * Takes the input frame and copies/scales the content in the
   * frames contained by FrameCopy
   * @param frame
   */
  void createCopyFrames(const VideoFrame &frame,
                        const int resizedImageWidth,
                        const int resizedImageHeight);

  /**
   * @brief copyFrameContent
   * Copies the data from the input frame iFrame to the output frame oFrame
   * Performs the scaling and format conversion
   * @param iFrame
   * @param oFrame
   */
  void copyFrameContent(const VideoFrame &iFrame, VideoFrame &oFrame);
};

class MediaProcessor : public Subscriber<std::shared_ptr<ExVideoFrame>>{
public:
  MediaProcessor();
  virtual ~MediaProcessor() override;

  /**
   * @brief onSubscribe
   * @param subscription
   */
  virtual void onSubscribe(std::shared_ptr<Subscription<std::shared_ptr<ExVideoFrame>>>&& subscription) override;

  /**
   * @brief onNewFrame
   * Takes a frame and updates its content
   * @param frame
   */
  void onNext(std::shared_ptr<ExVideoFrame> frame) override;

  /**
   * @brief onComplete
   */
  virtual void onComplete() override;

  /**
   * @brief stop
   * Notifies the processing thread to stop processing
   */
  void stop();

  /**
   * @brief feedInput
   * Takes a frame and feeds it to the model storage for predictions
   * @param frame
   */
  void feedInput(const VideoFrame &frame);

  /**
   * @brief computePredictions
   * Uses the model to compute the predictions and store them in
   * computedPredictions
   */
  void computePredictions();
  /**
   * @brief printPredictions
   * Prints the predictions names, probabilities
   * and bounding boxes (top left point, bottom right point)
   */
  void printPredictions();

  /**
   * @brief drawPredictionsOnFrame
   * Takes a BGR frame (to conform with OpenCV format), a tuple of predictions
   * and draws the predictions on the frame It draws the bounding box and writes
   * text for each prediction
   * @param frame
   * @param predictions: tuple(BoundinxBox, probability, labelIndex)
   * @param nbPredictions: maximum number of predictions to draw
   * @param threshold: Only display predictions with a probability > threshold
   */
  void drawPredictionsOnFrame(
      const VideoFrame &frame,
      const std::vector<std::tuple<std::array<float, 4>, float, int>>
          &predictions,
      const unsigned int nbPredictions = 5, const float threshold = 0.4f);

private:
  std::chrono::steady_clock::duration delta = std::chrono::nanoseconds::zero();
  int counter = 0;
  // Status variables of the processing
  bool firstRun{true};
  bool running{true};
  bool newFrame{false};
  bool processing{false};

  FrameCopy fcopy;
  std::thread processFrameThread;

  std::mutex inputLock;
  std::condition_variable inputCv;

  // Model being used
  MultipleObjectTracking mot;

  // Output predictions
  std::vector<std::tuple<std::array<float, 4>, float, int>> computedPredictions;

  // Colors
  cv::Scalar colors[10] = {
      // Red
      cv::Scalar{75, 25, 230},
      // Orange
      cv::Scalar{48, 130, 245},
      // Yellow
      cv::Scalar{25, 255, 255},
      // Lime
      cv::Scalar{25, 245, 210},
      // Green
      cv::Scalar{75, 180, 60},
      // Cyan
      cv::Scalar{240, 240, 70},
      // Blue
      cv::Scalar{200, 130, 0},
      // Purple
      cv::Scalar{180, 30, 145},
      // Magenta
      cv::Scalar{230, 50, 240},
      // Apricot
      cv::Scalar{180, 215, 255},
  };

  //Subscription
  std::shared_ptr<Subscription<std::shared_ptr<ExVideoFrame>>> subscription = nullptr;
};

} // namespace jami
