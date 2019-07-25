#include "media_processor.h"
// System includes
#include <cstring>
// OpenCV headers
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
// Logger
#include "logger.h"

using namespace std;

namespace jami {

void FrameCopy::createCopyFrames(const VideoFrame &frame,
                                 const int resizedImageWidth,
                                 const int resizedImageHeight) {
  // Allocate space for the resized frame
  resizedFrameRGB.reserve(AV_PIX_FMT_RGB24, resizedImageWidth,
                          resizedImageHeight);
  // Allocate space for the predictions frame
  predictionsFrameBGR.reserve(AV_PIX_FMT_BGR24, frame.width(), frame.height());
}

void FrameCopy::copyFrameContent(const VideoFrame &iFrame, VideoFrame &oFrame) {
  // Transform the input frame to RGB and performs the scaling
  scaler.scale(iFrame, oFrame);
}

MediaProcessor::MediaProcessor() : mot{SupervisedModel{}} {
  mot.init();
  /**
   * Waits for new frames and then process them
   * Writes the predictions in computedPredictions
   **/
  processFrameThread = std::thread([this] {
    while (running) {
      std::unique_lock<std::mutex> l(inputLock);
      inputCv.wait(l, [this] { return not running or newFrame; });
      if (not running) {
        break;
      }

      feedInput(fcopy.resizedFrameRGB);
      newFrame = false;
      /** Unclock the mutex, this way we let the other thread
       *  copy new data while we are processing the old one
       **/

      l.unlock();

      computePredictions();
    }
  });
}

MediaProcessor::~MediaProcessor() {
  JAMI_DBG("~MediaProcessor");
  stop();
  processFrameThread.join();
}

void MediaProcessor::onNewFrame(VideoFrame &frame) {
  if (firstRun) {
    mot.setExpectedImageDimensions();
    fcopy.createCopyFrames(frame, mot.getImageWidth(), mot.getImageHeight());
    JAMI_DBG() << "FRAME[]: w: " << frame.width() << " , h: " << frame.height()
               << " , format: "
               << av_get_pix_fmt_name(
                      static_cast<AVPixelFormat>(frame.format()));
    firstRun = false;
  }
  std::chrono::steady_clock::time_point tic = std::chrono::steady_clock::now();
  if (!newFrame) {

    std::lock_guard<std::mutex> l(inputLock);
    fcopy.copyFrameContent(frame, fcopy.resizedFrameRGB);
    //      printPredictions();
    newFrame = true;
    inputCv.notify_all();
  }

  fcopy.copyFrameContent(frame, fcopy.predictionsFrameBGR);
  drawPredictionsOnCopyFrame(fcopy.predictionsFrameBGR, computedPredictions);
  frame.copyFrom(fcopy.predictionsFrameBGR);
  std::chrono::steady_clock::time_point tac = std::chrono::steady_clock::now();
  const int totalFrames = 7200;
  delta += tac - tic;
  counter++;

  if (counter == totalFrames) {
    counter = 0;
    auto diff =
        std::chrono::duration_cast<std::chrono::microseconds>(delta).count();
    JAMI_DBG() << "Time per frame: " << diff / totalFrames;
    JAMI_DBG() << "Frame rate: " << 1000000.0f / diff / totalFrames;
    delta = std::chrono::nanoseconds::zero();
  }
}

void MediaProcessor::onNewFrame(const VideoFrame &frame) { (void)frame; }

void MediaProcessor::stop() {
  running = false;
  inputCv.notify_all();
}

void MediaProcessor::feedInput(const VideoFrame &frame) {
  auto pair = mot.getInput();
  uint8_t *inputPointer = pair.first;

  // Relevant data starts from index 1, dims.at(0) = 1
  size_t imageWidth = static_cast<size_t>(pair.second[1]);
  size_t imageHeight = static_cast<size_t>(pair.second[2]);
  size_t imageNbChannels = static_cast<size_t>(pair.second[3]);
  const size_t dataLineSize = imageNbChannels * imageWidth;

  uint8_t *line{nullptr};
  // LineSize = width*spectrum + padding, e.g: width*3  if rgb and padding = 0
  size_t lineSize = static_cast<size_t>(frame.pointer()->linesize[0]);

  // Write pixel data
  size_t i = 0;
  for (size_t y = 0; y < imageHeight; y++) {
    line = frame.pointer()->data[0] + y * lineSize;
    std::memcpy(inputPointer + y * dataLineSize, line,
                dataLineSize * sizeof(uint8_t));
  }

  line = nullptr;
}

void MediaProcessor::computePredictions() {
  // Run the graph
  mot.runGraph();
  auto predictions = mot.predictionsWithBoundingBoxes();
  // Save the predictions
  computedPredictions = predictions;
}

void MediaProcessor::printPredictions() {
  for (auto const &prediction : computedPredictions) {
    // Get prediction index, probability and bounding box
    size_t index = static_cast<size_t>(std::get<2>(prediction));
    float probability = std::get<1>(prediction);
    std::array<float, 4> aa = std::get<0>(prediction);
    /**
     * SSD Mobilenet V1 Model assumes class 0 is background class
     * Therefore, the labels start from 1 to number_of_classes+1,
     */
    std::string objectName = mot.getLabel(index + 1);

    // Log the predictions
    JAMI_DBG() << objectName << "\n"
               << static_cast<int>(probability * 10000.0f) / 100.0 << "%%";
    JAMI_DBG() << "probability: " << probability << " " << objectName << " "
               << aa[0] << "," << aa[1] << "," << aa[2] << "," << aa[3];
  }
}

void MediaProcessor::drawPredictionsOnCopyFrame(
    const VideoFrame &frame,
    const std::vector<std::tuple<std::array<float, 4>, float, int>>
        &predictions,
    const unsigned int nbPredictions, const float threshold) {
  cv::Mat opencvFrame{frame.height(), frame.width(), CV_8UC3,
                      frame.pointer()->data[0],
                      static_cast<size_t>(frame.pointer()->linesize[0])};

  int linewidth = std::max(1, int(frame.height() * .005));

  for (size_t i = 0; i < predictions.size(); i++) {
    const auto &prediction = predictions[i];
    // Get the prediction label index
    size_t index = static_cast<size_t>(std::get<2>(prediction));
    // Get the prediction probability
    float probability = std::get<1>(prediction);
    // Get the prediction bounding box
    std::array<float, 4> aa = std::get<0>(prediction);
    if (probability > threshold && i < nbPredictions) {
      /**
       * SSD Mobilenet V1 Model assumes class 0 is background class
       * Therefore, the labels start from 1 to number_of_classes+1,
       */
      std::string objectName = mot.getLabel(index + 1);
      /**
       * Create two points (x0,y0) and (x1, y1) that define the upper left
       * and lower right corners of the rectangle
       **/
      const int x0 = static_cast<int>(aa[1] * frame.width());
      const int y0 = static_cast<int>(aa[0] * frame.height());
      const int x1 = static_cast<int>(aa[3] * frame.width());
      const int y1 =
          static_cast<int>(aa[2] * fcopy.predictionsFrameBGR.height());

      // Top left of the bounding box
      cv::Point tl = cvPoint(x0, y0);
      // Bottom right of the bounding box
      cv::Point br = cvPoint(x1, y1);

      // Draw the bounding box
      cv::rectangle(opencvFrame, tl, br, colors[i], linewidth);

      // Text Position Point
      cv::Point txt_up = cvPoint(x0 + linewidth, y0 - 5 * linewidth);
      cv::Point txt_in = cvPoint(x0, y0 + 12 * linewidth);

      if (y0 > 0.05) {
        cv::putText(opencvFrame, objectName, txt_up, 1, linewidth, colors[i]);
      } else {
        // writes inside the bounding box if the boundary of the bounding box is
        // close to the frame image boundaries
        cv::putText(opencvFrame, objectName, txt_in, 1, linewidth, colors[i]);
      }
    }
  }
}
} // namespace jami
