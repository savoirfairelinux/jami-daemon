#ifndef TENSORFLOWSUPERVISEDINFERENCE_H
#define TENSORFLOWSUPERVISEDINFERENCE_H
// Tensorflow headers
#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/model.h"
// Library headers
#include "model.h"

class TensorflowSupervisedInference {
public:
  TensorflowSupervisedInference(SupervisedModel model);
  void loadLabels();
  void loadModel();
  void buildInterpreter();
  void setInterpreterSettings();
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
