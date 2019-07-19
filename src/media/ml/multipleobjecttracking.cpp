#include "multipleobjecttracking.h"
// Std libraries
#include <numeric>
// Logger
#include "logger.h"
// Tensorflow headers
#include "tensorflow/lite/interpreter.h"

MultipleObjectTracking::MultipleObjectTracking(SupervisedModel model)
    : TensorflowSupervisedInference(model) {}

/**
 * @brief MultipleObjectTracking::feedInput
 * Checks if the image input dimensions match the expected ones in the model
 * If so, fills the image data directly to the model input pointer
 * Otherwise, resizes the image in order to match the model expected image
 * dimensions And fills the image data throught the resize method
 * @param in: image data
 * @param imageWidth
 * @param imageHeight
 * @param imageNbChannels
 */
void MultipleObjectTracking::feedInput(std::vector<uint8_t> &in, int imageWidth,
                                       int imageHeight, int imageNbChannels) {
  // We assume that we have only one input
  // Get the input index
  int input = interpreter->inputs()[0];
  std::vector<int> dims = getTensorDimensions(input);
  // Relevant data starts from index 1, dims.at(0) = 1
  int expectedWidth = dims.at(1);
  int expectedHeight = dims.at(2);
  int expectedNbChannels = dims.at(3);

  if (imageNbChannels != expectedNbChannels) {
    JAMI_ERR() << "The number of channels in the input should match the number "
                  "of channels in the model";
  } else if (imageWidth != expectedWidth || imageHeight != expectedHeight) {
    JAMI_ERR() << "The width and height of the input image doesn't match the "
                  "expected width and height of the model";
  } else {
    // Get the input pointer and feed it with data
    uint8_t *inputDataPointer = interpreter->typed_tensor<uint8_t>(input);
    for (size_t i = 0; i < in.size(); i++) {
      inputDataPointer[i] = in.at(i);
    }
  }
}

/**
 * @brief MultipleObjectTracking::predictionsWithBoundingBoxes
 * computes the predictions with their bounding boxes
 * @return ([a0,a1,a2,a3], probability, index)
 * [a0,a1,a2,a3]: is an array containing normalized coordinates of the rectangle
 *      top left (x0,y0) and bottom right (x1,y1)
 *      Here a0 = y0/imageHeight, a1 = x0/imageWidth,
 *           a2 = y1/imageHeight, a3 = x1/imageWidth
 * probability: a float between ) and 1 describing the probability
 * of the result index: is the index of the label in the labels
 */
std::vector<std::tuple<std::array<float, 4>, float, int>>
MultipleObjectTracking::predictionsWithBoundingBoxes() const {
  auto boardingBoxPointer = interpreter->typed_output_tensor<float>(0);
  auto classesPointer = interpreter->typed_output_tensor<float>(1);
  auto probabilitiesPointer = interpreter->typed_output_tensor<float>(2);
  auto numberOfObjects = interpreter->typed_output_tensor<float>(3);

  std::vector<int> numberOfObjectsVector =
      getTensorDimensions(interpreter->outputs()[3]);
  JAMI_DBG() << "Number of dimensions " << numberOfObjectsVector.size() << "\n";
  size_t total = static_cast<size_t>(std::accumulate(
      std::begin(numberOfObjectsVector), std::end(numberOfObjectsVector), 1,
      std::multiplies<int>()));
  for (size_t i = 0; i < total; i++) {
    JAMI_DBG() << "There are " << numberOfObjects[i] << " In the the model "
               << "\n";
  }

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
