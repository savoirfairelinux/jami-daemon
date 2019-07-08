#include "testframeinput.h"
// Std
#include <fstream>
#include <random>
// Logger
#include "logger.h"
// Libraries for image processing
#include "CImg.h"
// Library headers
#include "multipleobjecttracking.h"

TestFrameInput::TestFrameInput() {}
TestFrameInput::~TestFrameInput() {}

void TestFrameInput::onNewFrame(const AVFrame *frame) {
  if (firstRun) {
    // Write Frame details
    JAMI_DBG() << "Frame: (" << frame->width << "," << frame->height << ")";
    copyOriginalFrame(frame);
    //***********************************************************************
    std::vector<uint8_t> originalImgRGBNonInterleaved =
        toNonInterleavedRGB(originalFrameRGB.get());
    cimg_library::CImg<uint8_t> originalImg(
        originalImgRGBNonInterleaved.data(),
        static_cast<unsigned int>(originalFrameRGB->width),
        static_cast<unsigned int>(originalFrameRGB->height), 1, 3, false);
    // Save the newly created RGB frame
    //    originalImg.save_png("/home/ayounes/Bureau/original.png");

    // Scale the image
    cimg_library::CImg<uint8_t> scaledImg(
        originalImgRGBNonInterleaved.data(),
        static_cast<unsigned int>(originalFrameRGB->width),
        static_cast<unsigned int>(originalFrameRGB->height), 1, 3, false);
    scaledImg.resize(300, 300);
    // Save the scaled version of the RGB frame
    //    scaledImg.save_png("/home/ayounes/Bureau/scaled.png");

    //***********************************************************************
    // Set the model settings
    SupervisedModel sm;
    MultipleObjectTracking mot(sm);
    // Loading the model + description for debug
    mot.init();
    mot.describeModelTensors();
    //***********************************************************************
    // Feed data and run the graph
    std::vector<uint8_t> in = toInterleavedRGB(resizedFrameRGB.get());
    mot.feedInput(in, resizedFrameRGB->width, resizedFrameRGB->height, 3);
    mot.runGraph();
    //***********************************************************************
    // Get the predictions
    auto predictions = mot.predictionsWithBoundingBoxes();

    // Define some constants for display
    const int image_width = originalImg.width();
    const int image_height = originalImg.height();
    const int paddingLeft = image_width / 50;
    const unsigned int fontSize = static_cast<unsigned int>(image_width / 25);

    int i = 1;
    for (auto const &prediction : predictions) {
      // Get prediction index, probability and boarding box
      size_t index = static_cast<size_t>(std::get<2>(prediction));
      float probability = std::get<1>(prediction);
      std::array<float, 4> aa = std::get<0>(prediction);

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

      JAMI_DBG() << objectName << "\n"
                 << static_cast<int>(probability * 10000.0f) / 100.0 << "%%";
      JAMI_DBG() << "probability: " << probability << " " << objectName << " "
                 << aa[0] << "," << aa[1] << "," << aa[2] << "," << aa[3];

      JAMI_DBG() << "   P0 = (" << x0 << " , " << y0 << ")";
      JAMI_DBG() << "   P1 = (" << x1 << " , " << y1 << ")";
      // Will be used to obtain a seed for the random number engine
      std::random_device rd;
      // Standard mersenne_twister_engine seeded with rd()
      std::mt19937 gen(rd());
      std::uniform_int_distribution<> dis(0, 255);
      unsigned char color[] = {static_cast<unsigned char>(dis(gen)),
                               static_cast<unsigned char>(dis(gen)),
                               static_cast<unsigned char>(dis(gen))};
      // Draw a rectangle
      //***********************************************************************
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
      //***********************************************************************
      if (i == 5)
        break;
      i++;
    }

    originalImg.display("Detected objects");

    firstRun = false;
  }
}

void TestFrameInput::copyOriginalFrame(const AVFrame *frame) {
  // Allocate space for the original frame
  originalFrameRGB =
      createFrame(static_cast<unsigned int>(frame->width),
                  static_cast<unsigned int>(frame->height), AV_PIX_FMT_RGB24);
  resizedFrameRGB =
      createFrame(static_cast<unsigned int>(300),
                  static_cast<unsigned int>(300), AV_PIX_FMT_RGB24);
  // Transform the input frame to RGB
  transformFrame(originalRGBContext, frame, originalFrameRGB.get());
  transformFrame(resizeRGBContext, frame, resizedFrameRGB.get());
  // At this point the originalFrame contains the data of the frame in RGB
}

/**
 * @brief TestFrameInput::transformFrame
 * Takes an input frame, transforms it to an output frame using a context
 * @param context
 * @param inputFrame
 * @param outputFrame
 */
void TestFrameInput::transformFrame(std::shared_ptr<SwsContext> context,
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
  // At this point the new transformed frame contains the data in the format we
  // want
}

/**
 * @brief TestFrameInput::saveFrameAsPPM
 * @param frame
 * @param filename
 */
void TestFrameInput::saveFrameAsPPM(const AVFrame *frame,
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
 * @brief TestFrameInput::createFrame
 * Creates a new frame allocates the resources and takes care of the cleanup
 * with custom deleters
 * @param width
 * @param height
 * @param format
 * @return
 */
std::shared_ptr<AVFrame>
TestFrameInput::createFrame(const unsigned int width, const unsigned int height,
                            const AVPixelFormat format) {
  /** Frame allocation
   *  In order to allocate space for the frame we not only need
   *  call av_frame_allocate(), but also create a buffer
   *  with a size that corresponds to the frame dimensions
   **/
  // Allocate video frame
  auto frame = std::shared_ptr<AVFrame>{av_frame_alloc(),
                                        [](AVFrame *raw) { av_free(raw); }};
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
 * @brief TestFrameInput::createContext
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
std::shared_ptr<SwsContext> TestFrameInput::createContext(
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
 * @brief TestFrameInput::toNonInterleavedRGB
 * Creates a vector where the channels are separated
 * E.g: with RGB, we should have rrr...r, ggg...g,bbb...b
 * This is useful in case of libraries that needs the data ordered this way
 * @param frame
 * @return vector of separated channels
 */
std::vector<uint8_t>
TestFrameInput::toNonInterleavedRGB(const AVFrame *frame) const {
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
 * @brief TestFrameInput::toInterleavedRGB
 * returns a vector with interleaved data from the frame
 * E.g: with RGB, we should have r,g,b...r,g,b...r,g,b
 * @param frame
 * @return vector of interleaved channels
 */
std::vector<uint8_t>
TestFrameInput::toInterleavedRGB(const AVFrame *frame) const {
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
