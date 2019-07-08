#include "frameresizer.h"
#include "logger.h"
// Tensorflow headers
#include "tensorflow/lite/builtin_op_data.h"
#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/kernels/register.h"

FrameResizer::FrameResizer()
    : FrameListener(), resizeContext{nullptr,
                                     [](SwsContext *raw) {
                                       sws_freeContext(raw);
                                     }},
      originalFrameRGB{av_frame_alloc(),
                       [](AVFrame *frame) { av_frame_free(&frame); }},
      originalFrameRGBBuffer{nullptr, [](uint_fast8_t *raw) { av_free(raw); }} {
}

FrameResizer::~FrameResizer() {}

void FrameResizer::onNewFrame(const AVFrame *frame) {
  if (!resizeContext) {
    resizeContext.reset(sws_getContext(
        frame->width, frame->height, static_cast<AVPixelFormat>(frame->format),
        static_cast<int>(wantedWidth), static_cast<int>(wantedHeight),
        AV_PIX_FMT_RGB24, SWS_FAST_BILINEAR, nullptr, nullptr, nullptr));
  }

  if (!originalFrameRGBBuffer) {
    createTransformedFrame(300, 300);
  }

  if (nbFrames < 1800) {
    std::chrono::steady_clock::time_point tic =
        std::chrono::steady_clock::now();
    transformFrameToRGB(frame);
    std::chrono::steady_clock::time_point tac =
        std::chrono::steady_clock::now();

    delta += tac - tic;
  } else {
    auto diff =
        std::chrono::duration_cast<std::chrono::nanoseconds>(delta).count();
    JAMI_DBG() << "Time taken for resize: " << diff / 1800;
  }

  nbFrames++;
}

void FrameResizer::createTransformedFrame(unsigned int width,
                                          unsigned int height) {
  /** Frame allocation
   *  In order to allocate space for the frame we not only need
   *  call av_frame_allocate(), but also create a buffer
   *  with a size that corresponds to the frame dimensions we want
   *  then call av_image_fill-arrays
   **/
  // Allocate video frame
  int numBytes;
  // Determine required buffer size and allocate buffer
  numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, static_cast<int>(width),
                                      static_cast<int>(height), 1);
  // Allocate space for the buffer
  originalFrameRGBBuffer.reset(static_cast<uint_fast8_t *>(
      av_malloc(static_cast<size_t>(numBytes) * sizeof(uint_fast8_t))));
  // Copy information of the buffer to the frame
  av_image_fill_arrays(
      originalFrameRGB.get()->data, originalFrameRGB.get()->linesize,
      originalFrameRGBBuffer.get(), AV_PIX_FMT_RGB24, width, height, 1);
  // Surprisingly when writing the data to the frame, the width and height
  // are not set, we need to add them manually
  originalFrameRGB->width = static_cast<int>(width);
  originalFrameRGB->height = static_cast<int>(height);
}

void FrameResizer::transformFrameToRGB(const AVFrame *frame) {

  /**
   *  /!\ sws_scale is not used only for scaling, it also
   *  helps to transform the frame from one pixel format to another
   */
  sws_scale(resizeContext.get(), frame->data, frame->linesize, 0, frame->height,
            originalFrameRGB->data, originalFrameRGB->linesize);
  // At this point the new transformedframe contains the data in the format we
  // want
}

/**
 * @brief TestFrameInput::saveFrameAsPPM
 * @param frame
 * @param filename
 */
void FrameResizer::saveFrameAsPPM(const AVFrame *frame,
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
           static_cast<size_t>(frame->linesize[0]) * sizeof(uint_fast8_t),
           pFile);
  }

  // Close file
  fclose(pFile);
}

void FrameResizer::resizeWithTensorflow(uint8_t *out, uint8_t *in,
                                        int image_height, int image_width,
                                        int image_channels, int wanted_height,
                                        int wanted_width, int wanted_channels) {
  int number_of_pixels = image_height * image_width * image_channels;
  std::unique_ptr<tflite::Interpreter> interpreter(new tflite::Interpreter);

  int base_index = 0;

  // two inputs: input and new_sizes
  interpreter->AddTensors(2, &base_index);
  // one output
  interpreter->AddTensors(1, &base_index);
  // set input and output tensors
  interpreter->SetInputs({0, 1});
  interpreter->SetOutputs({2});

  // set parameters of tensors
  TfLiteQuantizationParams quant;
  interpreter->SetTensorParametersReadWrite(
      0, kTfLiteFloat32, "input",
      {1, image_height, image_width, image_channels}, quant);
  interpreter->SetTensorParametersReadWrite(1, kTfLiteInt32, "new_size", {2},
                                            quant);
  interpreter->SetTensorParametersReadWrite(
      2, kTfLiteFloat32, "output",
      {1, wanted_height, wanted_width, wanted_channels}, quant);

  tflite::ops::builtin::BuiltinOpResolver resolver;
  const TfLiteRegistration *resize_op =
      resolver.FindOp(tflite::BuiltinOperator_RESIZE_BILINEAR, 1);
  auto *params = reinterpret_cast<TfLiteResizeBilinearParams *>(
      malloc(sizeof(TfLiteResizeBilinearParams)));
  params->align_corners = false;
  interpreter->AddNodeWithParameters({0, 1}, {2}, nullptr, 0, params, resize_op,
                                     nullptr);

  interpreter->AllocateTensors();

  // fill input image
  // in[] are integers, cannot do memcpy() directly
  auto input = interpreter->typed_tensor<float>(0);
  for (int i = 0; i < number_of_pixels; i++) {
    input[i] = in[i];
  }

  // fill new_sizes
  interpreter->typed_tensor<int>(1)[0] = wanted_height;
  interpreter->typed_tensor<int>(1)[1] = wanted_width;

  interpreter->Invoke();

  auto output = interpreter->typed_tensor<float>(2);
  auto output_number_of_pixels = wanted_height * wanted_width * wanted_channels;

  for (int i = 0; i < output_number_of_pixels; i++) {
    out[i] = static_cast<uint8_t>(output[i]);
  }
}
