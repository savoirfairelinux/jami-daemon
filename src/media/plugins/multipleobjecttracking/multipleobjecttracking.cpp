#include "multipleobjecttracking.h"
// Std libraries
#include <cstring>
#include <numeric>
#include <iostream>
// Tensorflow headers
#include "tensorflow/lite/interpreter.h"

namespace jami {
MultipleObjectTracking::MultipleObjectTracking(SupervisedModel model)
    : TensorflowSupervisedInference([&model] {
#ifdef __ANDROID__
        model.useNNAPI = true;
        return model;
#else
        model.useNNAPI = false;
        return model;
#endif // __ANDROID__
      }()) {
}

void MultipleObjectTracking::feedInput(std::vector<uint8_t> &in, int imageWidth,
                                       int imageHeight, int imageNbChannels) {

  auto input = getInput();
  std::vector<int> dims = input.second;
  // Relevant data starts from index 1, dims.at(0) = 1
  int expectedWidth = dims.at(1);
  int expectedHeight = dims.at(2);
  int expectedNbChannels = dims.at(3);

  if (imageNbChannels != expectedNbChannels) {
      std::cerr << "The number of channels in the input should match the number "
                  "of channels in the model";
  } else if (imageWidth != expectedWidth || imageHeight != expectedHeight) {
    std::cerr << "The width and height of the input image doesn't match the "
                  "expected width and height of the model";
  } else {
    // Get the input pointer and feed it with data
    uint8_t *inputDataPointer = input.first;
    for (size_t i = 0; i < in.size(); i++) {
      inputDataPointer[i] = in.at(i);
    }
    // Use of memcopy for performance
    std::memcpy(inputDataPointer, in.data(), in.size() * sizeof(uint8_t));
  }
}

std::pair<uint8_t *, std::vector<int>> MultipleObjectTracking::getInput() {
  // We assume that we have only one input
  // Get the input index
  int input = interpreter->inputs()[0];
  uint8_t *inputDataPointer = interpreter->typed_tensor<uint8_t>(input);
  // Get the input dimensions vector
  std::vector<int> dims = getTensorDimensions(input);

  return std::make_pair(inputDataPointer, dims);
}

std::vector<std::tuple<std::array<float, 4>, float, int>>
MultipleObjectTracking::predictionsWithBoundingBoxes() const {
  auto boardingBoxPointer = interpreter->typed_output_tensor<float>(0);
  auto classesPointer = interpreter->typed_output_tensor<float>(1);
  auto probabilitiesPointer = interpreter->typed_output_tensor<float>(2);
  auto numberOfObjects = interpreter->typed_output_tensor<float>(3);

  const size_t MAX_NUM_OBJECTS = static_cast<size_t>(numberOfObjects[0]);

  std::vector<std::tuple<std::array<float, 4>, float, int>> results;
  results.reserve(MAX_NUM_OBJECTS);
  for (size_t i = 0; i < MAX_NUM_OBJECTS; i++) {
    std::tuple<std::array<float, 4>, float, int> tuple;
    std::array<float, 4> fourCoordiantes{
        boardingBoxPointer[i * 4], boardingBoxPointer[i * 4 + 1],
        boardingBoxPointer[i * 4 + 2], boardingBoxPointer[i * 4 + 3]};
    // Clamp the values to the interval [0,1]
    for (size_t j = 0; j < 4; j++) {
      if (fourCoordiantes[j] < 0.0f)
        fourCoordiantes[j] = 0.0f;
      if (fourCoordiantes[j] > 1.0f)
        fourCoordiantes[j] = 1.0f;
    }
    tuple = std::make_tuple(fourCoordiantes, probabilitiesPointer[i],
                            static_cast<int>(classesPointer[i]));
    results.push_back(tuple);
  }

  return results;
}

void MultipleObjectTracking::setExpectedImageDimensions() {
  // We assume that we have only one input
  // Get the input index
  int input = interpreter->inputs()[0];
  // Get the input dimensions vector
  std::vector<int> dims = getTensorDimensions(input);
  // Relevant data starts from index 1, dims.at(0) = 1
  imageWidth = dims.at(1);
  imageHeight = dims.at(2);
  imageNbChannels = dims.at(3);
}

int MultipleObjectTracking::getImageWidth() const { return imageWidth; }

int MultipleObjectTracking::getImageHeight() const { return imageHeight; }

int MultipleObjectTracking::getImageNbChannels() const {
  return imageNbChannels;
}
} // namespace jami
