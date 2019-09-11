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
  cv::Mat resizedFrameRGB;
  // This frame is used to draw predictions into in RGB format
  cv::Mat predictionsFrameBGR;
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
  void feedInput(const cv::Mat &frame);

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
      cv::Mat &frame,
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
      cv::Scalar{230, 25, 75},
      // Orange
      cv::Scalar{245, 130, 48},
      // Yellow
      cv::Scalar{255, 255, 25},
      // Lime
      cv::Scalar{210, 245, 25},
      // Green
      cv::Scalar{6075, 180, 75},
      // Cyan
      cv::Scalar{70, 240, 240},
      // Blue
      cv::Scalar{0, 130, 200},
      // Purple
      cv::Scalar{145, 30, 180},
      // Magenta
      cv::Scalar{240, 50, 230},
      // Apricot
      cv::Scalar{255, 215, 180},
  };

  //Subscription
  std::shared_ptr<Subscription<std::shared_ptr<ExVideoFrame>>> subscription = nullptr;
};

} // namespace jami
