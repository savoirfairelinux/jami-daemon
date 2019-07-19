#include "testframeinput.h"
// Std
#include <fstream>
#include <random>
// Logger
#include "logger.h"
// Libraries for image processing
#include "CImg.h"
// Library headers
#include "media_buffer.h"

TestFrameInput::TestFrameInput()
    : mot{SupervisedModel{}},
      /**
       * Waits for new frames and then process them
       * Writes the predictions in computedPredictions
       **/
      processFrameThread{[this] {
        while (!stopProcessingThread) {
          std::unique_lock<std::mutex> lk(frameMutex);
          data_cond.wait(lk, [this] { return (newFrame); });
          if (!processing) {
            feedInput();
            newFrame = false;
          }
          /** Unclock the mutex, this way we let the other thread
           *  copy new data while we are processing the old one
           **/

          lk.unlock();

          processing = true;
          computePredictions();
          processing = false;
        }
      }} {
  //***********************************************************************
  // Loading the model + description for debug
  mot.init();
  //  mot.describeModelTensors();
}
TestFrameInput::~TestFrameInput() {
  stopProcessingThread = true;
  processFrameThread.join();
  JAMI_DBG() << "~TestFrameInput\n";
}

/**
 * @brief TestFrameInput::onNewFrame
 * Called by the decoder whenever there is a new frame
 * @param frame
 */
void TestFrameInput::onNewFrame(jami::VideoFrame &frame) {
  if (firstRun) {
    fcopy.createCopyFrames(frame);
    JAMI_DBG() << frame.width() << ":" << frame.height() << "\n";
    firstRun = false;
  }

  if (!newFrame) {
    std::lock_guard<std::mutex> lk(frameMutex);
    // Copy the frames
    fcopy.copyOriginalFrame(frame);
    std::vector<uint8_t> originalImgRGBNonInterleaved =
        fcopy.toNonInterleavedRGB(fcopy.originalFrameRGB.pointer());
    cimg_library::CImg<uint8_t> originalImg(
        originalImgRGBNonInterleaved.data(),
        static_cast<unsigned int>(fcopy.originalFrameRGB.width()),
        static_cast<unsigned int>(fcopy.originalFrameRGB.height()), 1, 3,
        false);

    std::vector<uint8_t> resizedImgRGBNonInterleaved =
        fcopy.toNonInterleavedRGB(fcopy.resizedFrameRGB.pointer());

    cimg_library::CImg<uint8_t> resizedImg(
        resizedImgRGBNonInterleaved.data(),
        static_cast<unsigned int>(fcopy.resizedFrameRGB.width()),
        static_cast<unsigned int>(fcopy.resizedFrameRGB.height()), 1, 3, false);

    std::vector<uint8_t> resized2RGBNonInterleaved =
        fcopy.toNonInterleavedRGB(fcopy.resized.get());

    cimg_library::CImg<uint8_t> resized2(
        resized2RGBNonInterleaved.data(),
        static_cast<unsigned int>(fcopy.resizedFrameRGB.width()),
        static_cast<unsigned int>(fcopy.resizedFrameRGB.height()), 1, 3, false);
    originalImg.display();
    resizedImg.display();
    resized2.display();
    newFrame = true;
    data_cond.notify_one();
  }

  drawPredictionsOnCopyFrame(frame, computedPredictions);
  frame.copyFrom(fcopy.predictionsFrame);
}
void TestFrameInput::onNewFrame(AVFrame *frame) {}
void TestFrameInput::onNewFrame(const AVFrame *frame) {}

/**
 * @brief TestFrameInput::feedInput
 */
void TestFrameInput::feedInput() {
  //***********************************************************************
  // Feed data
  std::vector<uint8_t> in =
      fcopy.toInterleavedRGB(fcopy.resizedFrameRGB.pointer());
  mot.feedInput(in, fcopy.resizedFrameRGB.width(),
                fcopy.resizedFrameRGB.height(), 3);
}

/**
 * @brief TestFrameInput::computePredictions
 */
void TestFrameInput::computePredictions() {
  mot.runGraph();
  //***********************************************************************
  // Run the graph and get the predictions
  auto predictions = mot.predictionsWithBoundingBoxes();
  computedPredictions = predictions;
}

/**
 * @brief TestFrameInput::drawPredictionsOnImage
 * @param originalFrame
 * @param predictions
 */
void TestFrameInput::drawPredictionsOnCopyFrame(
    const jami::VideoFrame &frame,
    const std::vector<std::tuple<std::array<float, 4>, float, int>>
        &predictions) {
  //***********************************************************************
  fcopy.scaler.scale(frame, fcopy.predictionsFrame);
  // Create images using CImg for final display/saving to file
  std::vector<uint8_t> originalImgRGBNonInterleaved =
      fcopy.toNonInterleavedRGB(fcopy.predictionsFrame.pointer());
  cimg_library::CImg<uint8_t> originalImg(
      originalImgRGBNonInterleaved.data(),
      static_cast<unsigned int>(fcopy.predictionsFrame.width()),
      static_cast<unsigned int>(fcopy.predictionsFrame.height()), 1, 3, false);
  // Define some constants for display
  const int image_width = originalImg.width();
  const int image_height = originalImg.height();
  const int paddingLeft = image_width / 50;
  const unsigned int fontSize = static_cast<unsigned int>(image_width / 25);

  int i = 0;
  for (auto const &prediction : predictions) {
    // Get prediction index, probability and boarding box
    size_t index = static_cast<size_t>(std::get<2>(prediction));
    float probability = std::get<1>(prediction);
    std::array<float, 4> aa = std::get<0>(prediction);
    if (probability > threshold) {
      // SSD Mobilenet V1 Model assumes class 0 is background class
      // in label file and class labels start from 1 to number_of_classes+1,
      std::string objectName = mot.getLabel(index + 1);
      /**
       * Create two points (x0,y0) and (x1, y1) that define the upper left
       * and lower right corners of the rectangle
       **/
      const int x0 = static_cast<int>(aa[1] * image_width);
      const int y0 = static_cast<int>(aa[0] * image_height);
      const int x1 = static_cast<int>(aa[3] * image_width);
      const int y1 = static_cast<int>(aa[2] * image_height);

      // Log the predictions
      //      JAMI_DBG() << objectName << "\n"
      //                 << static_cast<int>(probability * 10000.0f) / 100.0 <<
      //                 "%%";
      //      JAMI_DBG() << "probability: " << probability << " " << objectName
      //      << " "
      //                 << aa[0] << "," << aa[1] << "," << aa[2] << "," <<
      //                 aa[3];

      //      JAMI_DBG() << "   P0 = (" << x0 << " , " << y0 << ")";
      //      JAMI_DBG() << "   P1 = (" << x1 << " , " << y1 << ")";

      // Draw a rectangle
      //***********************************************************************
      auto color = colors[i];
      i++;
      originalImg.draw_line(x0, y0, x1, y0, color);
      originalImg.draw_line(x1, y0, x1, y1, color);
      originalImg.draw_line(x0, y0, x0, y1, color);
      originalImg.draw_line(x0, y1, x1, y1, color);
      // Draw text
      //***********************************************************************
      std::stringstream title;
      title << objectName << "\n"
            << static_cast<int>(probability * 10000.0f) / 100.0 << "%%";
      originalImg.draw_text(paddingLeft + x0, y0, title.str().c_str(), color, 0,
                            1, fontSize);
    }
  }

  //***********************************************************************
  // Here either you display the image using display or change the frame
  //  /* OPTION 1 */ originalImg.display("Detected objects");
  //***********************************************************************
  /* OPTION 2 */
  unsigned long s = originalImg.size();
  unsigned long nbChannels = static_cast<unsigned long>(originalImg.spectrum());
  //  std::vector<uint8_t> result;

  // This pointer is freed by the videoFrame
  uint8_t *result = reinterpret_cast<uint8_t *>(av_malloc(s));
  //  result.reserve(s);
  if (nbChannels == 3) {
    uint8_t *p0 = originalImg.data();
    uint8_t *p1 = originalImg.data();
    uint8_t *p2 = originalImg.data();
    unsigned long q = s / nbChannels;
    p1 += q;
    p2 += (nbChannels - 1) * q;

    for (size_t i = 0; i != q; i++) {
      result[nbChannels * i] = *p0;
      result[nbChannels * i + 1] = *p1;
      result[nbChannels * i + 2] = *p2;
      //      result.push_back(*p0);
      //      result.push_back(*p1);
      //      result.push_back(*p2);

      p0++;
      p1++;
      p2++;
    }

    p0 = nullptr;
    p1 = nullptr;
    p2 = nullptr;
  }

  //  for (size_t i = 0; i < result.size(); i++) {
  //    *(fcopy.predictionsFrame->data[0] + i) = result[i];
  //  }
  fcopy.predictionsFrame.setFromMemory(result, AV_PIX_FMT_RGB24,
                                       fcopy.predictionsFrame.width(),
                                       fcopy.predictionsFrame.height());
}

/**
 * @brief FrameCopy::createCopyFrames
 * Qll
 * @param frame
 */
void FrameCopy::createCopyFrames(const jami::VideoFrame &frame) {
  // Allocate space for the original frame
  originalFrameRGB.reserve(AV_PIX_FMT_RGB24, frame.width(), frame.height());
  // Allocate space for the resized frame
  resizedFrameRGB.reserve(AV_PIX_FMT_RGB24, 300, 300);
  resized = createFrame(300, 300, AV_PIX_FMT_RGB24);
  // Allocate space for the predictions frame
  predictionsFrame.reserve(AV_PIX_FMT_RGB24, frame.width(), frame.height());
}

/**
 * @brief FrameCopy::copyOriginalFrame
 * @param frame
 */
void FrameCopy::copyOriginalFrame(const jami::VideoFrame &frame) {
  // Transform the input frame to RGB
  scaler.scale(frame, originalFrameRGB);
  scaler.scale(frame, resizedFrameRGB);
  transformFrame(resizeRGBContext, frame.pointer(), resized.get());
  // At this point the originalFrame contains the data of the frame in RGB
}

/**
 * @brief FrameCopy::transformFrame
 * Takes an input frame, transforms it to an output frame using a context
 * @param context
 * @param inputFrame
 * @param outputFrame
 */
void FrameCopy::transformFrame(std::shared_ptr<SwsContext> context,
                               const AVFrame *inputFrame,
                               AVFrame *outputFrame) {
  /**
   * In order to transform a frame into another
   *  e.g: scaling, changing the format
   *  we first need to create a context if it does not exist
   *  where we specify what we want
   *  then, we call sws_scale to make the transformation
   **/
  if (!context)
    context = createContext(static_cast<unsigned int>(inputFrame->width),
                            static_cast<unsigned int>(inputFrame->height),
                            static_cast<AVPixelFormat>(inputFrame->format),
                            static_cast<unsigned int>(outputFrame->width),
                            static_cast<unsigned int>(outputFrame->height),
                            static_cast<AVPixelFormat>(outputFrame->format));
  /**
   *  /!\ sws_scale is not used only for scaling, it also
   *  helps to transform the frame from one pixel format to another
   */
  sws_scale(context.get(), inputFrame->data, inputFrame->linesize, 0,
            inputFrame->height, outputFrame->data, outputFrame->linesize);
  // At this point the new transformed frame contains the data in the format
  // we want
}

/**
 * @brief FrameCopy::saveFrameAsPPM
 * @param frame
 * @param filename
 */
void FrameCopy::saveFrameAsPPM(const AVFrame *frame,
                               const std::string &filename) const {
  // Open file
  FILE *pFile;
  pFile = fopen(filename.c_str(), "wb");
  if (pFile == nullptr)
    return;

  // Write header
  fprintf(pFile, "P6\n%d %d\n255\n", frame->width, frame->height);

  // Write to file
  for (size_t y = 0; static_cast<int>(y) < frame->height; y++) {
    fwrite(frame->data[0] + static_cast<int>(y) * frame->linesize[0], 1,
           static_cast<size_t>(frame->linesize[0]) * sizeof(uint8_t), pFile);
  }

  // Close file
  fclose(pFile);
}

/**
 * @brief FrameCopy::createFrame
 * Creates a new frame allocates the resources and takes care of the cleanup
 * with custom deleters
 * @param width
 * @param height
 * @param format
 * @return
 */
std::shared_ptr<AVFrame> FrameCopy::createFrame(const unsigned int width,
                                                const unsigned int height,
                                                const AVPixelFormat format) {
  /** Frame allocation
   *  In order to allocate space for the frame we not only need
   *  call av_frame_allocate(), but also create a buffer
   *  with a size that corresponds to the frame dimensions
   **/
  // Allocate video frame
  auto frame = std::shared_ptr<AVFrame>{
      av_frame_alloc(), [](AVFrame *raw) { av_frame_free(&raw); }};
  // Allocate a buffer that corresponds to the frame dimensions and format
  av_image_alloc(frame.get()->data, frame.get()->linesize, width, height,
                 format, 1);
  // Surprisingly when writing the data to the frame, the width and height
  // are not set, we need to add them manually
  frame->width = static_cast<int>(width);
  frame->height = static_cast<int>(height);
  frame->format = static_cast<int>(format);
  return frame;
}

/**
 * @brief FrameCopy::createContext
 * Creates a shared pointer to a SwsContext with custom deleter
 * It takes care of the cleanup
 * @param inputdWidth
 * @param inputHeight
 * @param inputFormat
 * @param outputWidth
 * @param outputHeight
 * @param outputFormat
 * @return
 */
std::shared_ptr<SwsContext> FrameCopy::createContext(
    const unsigned int inputdWidth, const unsigned int inputHeight,
    const AVPixelFormat inputFormat, const unsigned int outputWidth,
    const unsigned int outputHeight, const AVPixelFormat outputFormat) {

  return std::shared_ptr<SwsContext>{
      sws_getContext(static_cast<int>(inputdWidth),
                     static_cast<int>(inputHeight), inputFormat,
                     static_cast<int>(outputWidth),
                     static_cast<int>(outputHeight), outputFormat,
                     SWS_FAST_BILINEAR, nullptr, nullptr, nullptr),
      [](SwsContext *raw) { sws_freeContext(raw); }};
}

/**
 * @brief FrameCopy::toNonInterleavedRGB
 * Creates a vector where the channels are separated
 * E.g: with RGB, we should have rrr...r, ggg...g,bbb...b
 * This is useful in case of libraries that needs the data ordered this way
 * @param frame
 * @return vector of separated channels
 */
std::vector<uint8_t>
FrameCopy::toNonInterleavedRGB(const AVFrame *frame) const {
  std::vector<uint8_t> result;
  int nbChannels = 3; // RGB
  result.reserve(
      static_cast<size_t>(frame->height * frame->width * nbChannels));

  uint8_t *line = frame->data[0];

  // R
  for (size_t c = 0; static_cast<int>(c) < frame->height * frame->width; c++) {
    result.push_back(*(line + 3 * c));
  }
  // G
  for (size_t c = 0; static_cast<int>(c) < frame->height * frame->width; c++) {
    result.push_back(*(line + 1 + 3 * c));
  }
  // B
  for (size_t c = 0; static_cast<int>(c) < frame->height * frame->width; c++) {
    result.push_back(*(line + 2 + 3 * c));
  }

  line = nullptr;

  return result;
}

/**
 * @brief FrameCopy::toInterleavedRGB
 * returns a vector with interleaved data from the frame
 * E.g: with RGB, we should have r,g,b...r,g,b...r,g,b
 * @param frame
 * @return vector of interleaved channels
 */
std::vector<uint8_t> FrameCopy::toInterleavedRGB(const AVFrame *frame) const {
  std::vector<uint8_t> result;
  int nbChannels = 3; // RGB
  result.reserve(
      static_cast<size_t>(frame->height * frame->width * nbChannels));

  uint8_t *line = nullptr;
  // LineSize = width*spectrum, e.g: width*3 if rgb
  size_t lineSize = static_cast<size_t>(frame->linesize[0]);
  // Write pixel data
  for (size_t y = 0; static_cast<int>(y) < frame->height; y++) {
    line = frame->data[0] + static_cast<int>(y) * frame->linesize[0];
    for (size_t z = 0; z < lineSize; z++) {
      result.push_back(*(line + z));
    }
  }

  line = nullptr;

  return result;
}
