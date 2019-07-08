#ifndef MULTIPLEOBJECTTRACKING_H
#define MULTIPLEOBJECTTRACKING_H
#include "tensorflowsupervisedinference.h"
/* Here we use stdint-gcc, there are other versions in the contrib which do
 * not contain uint8_t for some reason */
#include <stdint-gcc.h>

class MultipleObjectTracking : public TensorflowSupervisedInference {
public:
  MultipleObjectTracking(SupervisedModel model);

  // User defined method for interpretation
  std::vector<std::tuple<std::array<float, 4>, float, int>>
  predictionsWithBoundingBoxes() const;
  void feedInput(std::vector<uint8_t> &in, int imageWidth, int imageHeight,
                 int imageNbChannels);
};

#endif // MULTIPLEOBJECTTRACKING_H
