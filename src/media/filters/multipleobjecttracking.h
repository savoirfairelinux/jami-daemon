#ifndef MULTIPLEOBJECTTRACKING_H
#define MULTIPLEOBJECTTRACKING_H
#include "tensorflowsupervisedinference.h"
/* Here we use stdint-gcc, there are other versions in the contrib which do
 * not contain uint8_t for some reason */
#include <stdint-gcc.h>

class MultipleObjectTracking : public TensorflowSupervisedInference {
public:
  /**
   * @brief MultipleObjectTracking
   * Is a type of supervised learning where we detect objects in images
   * Draw a bounding boxes around them
   * @param model
   */
  MultipleObjectTracking(SupervisedModel model);

  /**
   * @brief predictionsWithBoundingBoxes
   * computes the predictions with their bounding boxes
   * @return ([a0,a1,a2,a3], probability, index)
   * [a0,a1,a2,a3]: is an array containing normalized coordinates of the
   * rectangle top left (x0,y0) and bottom right (x1,y1) Here a0 =
   * y0/imageHeight, a1 = x0/imageWidth, a2 = y1/imageHeight, a3 = x1/imageWidth
   * probability: a float between 0 and 1 describing the probability
   * of the result index: is the index of the label in the labels
   */
  std::vector<std::tuple<std::array<float, 4>, float, int>>
  predictionsWithBoundingBoxes() const;

  /**
   * @brief feedInput
   * Checks if the image input dimensions matches the expected ones in the model
   * If so, fills the image data directly to the model input pointer
   * Otherwise, resizes the image in order to match the model expected image
   * dimensions And fills the image data throught the resize method
   * @param in: image data
   * @param imageWidth
   * @param imageHeight
   * @param imageNbChannels
   **/
  void feedInput(std::vector<uint8_t> &in, int imageWidth, int imageHeight,
                 int imageNbChannels);
  /**
   * @brief getInput
   * Returns the input where to fill the data
   * Use this method if you know what you are doing, all the necessary checks
   * on dimensions must be done on your part
   * @return std::tuple<uint8_t *, std::vector<int>>
   * The first element in the tuple is the pointer to the storage location
   * The second element is a dimensions vector that will helps you make
   * The necessary checks to make your data size match the input one
   */
  std::pair<uint8_t *, std::vector<int>> getInput();

  /**
   * @brief setExpectedImageDimensions
   * Sets imageWidth and imageHeight from the sources
   */
  void setExpectedImageDimensions();

  // Getters
  int getImageWidth() const;
  int getImageHeight() const;
  int getImageNbChannels() const;

private:
  int imageWidth;
  int imageHeight;
  int imageNbChannels;
};

#endif // MULTIPLEOBJECTTRACKING_H
