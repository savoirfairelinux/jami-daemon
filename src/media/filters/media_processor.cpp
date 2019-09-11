﻿#include "media_processor.h"
// System includes
#include <cstring>
// OpenCV headers
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
// Logger
#include "logger.h"


namespace jami {

void FrameCopy::createCopyFrames(const VideoFrame &frame,
                                 const int resizedImageWidth,
                                 const int resizedImageHeight) {
  // Allocate space for the resized frame
  //resizedFrameRGB.reserve(AV_PIX_FMT_RGB24, resizedImageWidth,
  //                        resizedImageHeight);
  // Allocate space for the predictions frame
  //predictionsFrameBGR.reserve(AV_PIX_FMT_BGR24, frame.width(), frame.height());
}

void FrameCopy::copyFrameContent(const VideoFrame &iFrame, VideoFrame &oFrame) {
  // Transform the input frame to RGB and performs the scaling
  scaler.scale(iFrame, oFrame);
}

MediaProcessor::MediaProcessor() : mot{SupervisedModel{}} {
  auto prediction  = std::make_tuple<std::array<float, 4>, float, int>({0.25,0.25,0.75,0.75},0.9,5);
  computedPredictions.push_back(prediction);
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

      //feedInput(fcopy.resizedFrameRGB);
      newFrame = false;
      /** Unclock the mutex, this way we let the other thread
       *  copy new data while we are processing the old one
       **/

      l.unlock();

      //computePredictions();
    }
  });
}

MediaProcessor::~MediaProcessor() {
  JAMI_DBG("~MediaProcessor");
  stop();
  processFrameThread.join();
}

void MediaProcessor::onSubscribe(std::shared_ptr<Subscription<std::shared_ptr<ExVideoFrame>>>&& sub){
    subscription = sub;
    JAMI_DBG() << "  Subscribed ! ";
    subscription->request(1);
}

void MediaProcessor::onNext(std::shared_ptr<ExVideoFrame> frame) {
    if(subscription) {
      cv::Mat opencvFrame{frame->height(), frame->width(), CV_8UC3,
                      frame->pointer(),
                      static_cast<size_t>(frame->linesize())};
        cv::Size resizedSize;
        if (firstRun) {
            mot.setExpectedImageDimensions();
            //fcopy.createCopyFrames(*frame, mot.getImageWidth(), mot.getImageHeight());
            //resizedSize = cv::Size{mot.getImageWidth(), mot.getImageHeight()};
            //cv::resize(opencvFrame,fcopy.resizedFrameRGB,resizedSize);
            JAMI_DBG() << "FRAME[]: w: " << frame->width() << " , h: " << frame->height()
                       << " , format: "
                       << av_get_pix_fmt_name(
                              static_cast<AVPixelFormat>(frame->format()));
            firstRun = false;
        }
        std::chrono::steady_clock::time_point tic = std::chrono::steady_clock::now();
        if (!newFrame) {
            std::lock_guard<std::mutex> l(inputLock);
            //fcopy.copyFrameContent(*frame, fcopy.resizedFrameRGB);
            //cv::resize(opencvFrame,fcopy.resizedFrameRGB,resizedSize);
            printPredictions();
            newFrame = true;
            inputCv.notify_all();
        }
        //fcopy.copyFrameContent(*frame, fcopy.predictionsFrameBGR);
        fcopy.predictionsFrameBGR = opencvFrame.clone();
        drawPredictionsOnFrame(fcopy.predictionsFrameBGR, computedPredictions);
        JAMI_DBG() << " OnNext Called from Mediaprocessor";

        //frame->copyFrom(fcopy.predictionsFrameBGR);
        if(frame->pointer()) {
          uint8_t* frameData = frame->pointer();
          std::memcpy(frameData, fcopy.predictionsFrameBGR.data,frame->width()*frame->height()*3 * sizeof(uint8_t));
        }

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
      subscription->request(1);
    }
}

void MediaProcessor::onComplete() {
    JAMI_DBG()  << "  OnComplete called";
}

void MediaProcessor::stop() {
  running = false;
  inputCv.notify_all();
}

void MediaProcessor::feedInput(const cv::Mat &frame) {
  auto pair = mot.getInput();
  uint8_t *inputPointer = pair.first;

  // Relevant data starts from index 1, dims.at(0) = 1
  size_t imageWidth = static_cast<size_t>(pair.second[1]);
  size_t imageHeight = static_cast<size_t>(pair.second[2]);
  size_t imageNbChannels = static_cast<size_t>(pair.second[3]);
  const size_t dataLineSize = imageNbChannels * imageWidth;

/*   uint8_t *line{nullptr};
  // LineSize = width*spectrum + padding, e.g: width*3  if rgb and padding = 0
  size_t lineSize = static_cast<size_t>(3*frame.size().width);

  // Write pixel data
  for (size_t y = 0; y < imageHeight; y++) {
    line = frame.data + y * lineSize;
    std::memcpy(inputPointer + y * dataLineSize, line,
                dataLineSize * sizeof(uint8_t));
  }

  line = nullptr; */

  std::memcpy(inputPointer, frame.data , imageWidth* imageHeight*imageNbChannels *  sizeof(uint8_t));

  inputPointer = nullptr;
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

void MediaProcessor::drawPredictionsOnFrame(
    cv::Mat &frame,
    const std::vector<std::tuple<std::array<float, 4>, float, int>>
        &predictions,
    const unsigned int nbPredictions, const float threshold) {

  int linewidth = std::max(1, int(frame.size().height * .005));
  cv::cvtColor(frame,frame, cv::COLOR_RGB2BGR);
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
      const int x0 = static_cast<int>(aa[1] * frame.size().width);
      const int y0 = static_cast<int>(aa[0] * frame.size().height);
      const int x1 = static_cast<int>(aa[3] * frame.size().width);
      const int y1 =
          static_cast<int>(aa[2] * frame.size().height);

      // Top left of the bounding box
      cv::Point tl = cvPoint(x0, y0);
      // Bottom right of the bounding box
      cv::Point br = cvPoint(x1, y1);

      // Draw the bounding box
      cv::rectangle(frame, tl, br, colors[i], linewidth);

      // Text Position Point
      cv::Point txt_up = cvPoint(x0 + linewidth, y0 - 5 * linewidth);
      cv::Point txt_in = cvPoint(x0, y0 + 12 * linewidth);

      if (y0 > 0.05) {
        cv::putText(frame, objectName, txt_up, 1, linewidth, colors[i]);
      } else {
        // writes inside the bounding box if the boundary of the bounding box is
        // close to the frame image boundaries
        cv::putText(frame, objectName, txt_in, 1, linewidth, colors[i]);
      }
    }
  }
  cv::cvtColor(frame,frame, cv::COLOR_BGR2RGB);
}
} // namespace jami
