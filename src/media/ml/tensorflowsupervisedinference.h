#ifndef TENSORFLOWSUPERVISEDINFERENCE_H
#define TENSORFLOWSUPERVISEDINFERENCE_H
// Std libraries
#include <memory>
// Library headers
#include "model.h"

namespace tflite {
class FlatBufferModel;
class Interpreter;
} // namespace tflite

class TensorflowSupervisedInference {
public:
  TensorflowSupervisedInference(SupervisedModel model);
  ~TensorflowSupervisedInference();
  void loadLabels();
  void loadModel();
  void buildInterpreter();
  void setInterpreterSettings();
  void allocateTensors();
  void runGraph();
  void init();
  // Getter
  std::string getLabel(size_t postion) const;
  // Debug methods
  void describeModelTensors() const;
  void describeTensor(std::string prefix, int index) const;

protected:
  void padLabels();
  // Utility method to get Tensorflow Tensor dimension
  std::vector<int> getTensorDimensions(int index) const;

  SupervisedModel model;
  std::vector<std::string> labels;
  // The real number of labels may not match the labels.size() because of
  // padding
  size_t nbLabels;

  // Tensorflow model and interpreter
  std::unique_ptr<tflite::FlatBufferModel> tfmodel;
  std::unique_ptr<tflite::Interpreter> interpreter;

private:
  std::vector<std::string> readLinesFromFile(const std::string filename) const;
};

#endif // TENSORFLOWSUPERVISEDINFERENCE_H
