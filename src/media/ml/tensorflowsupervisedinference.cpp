#include "tensorflowsupervisedinference.h"
// Std libraries
#include <fstream>
#include <numeric>
// Tensorflow headers
#include "tensorflow/lite/builtin_op_data.h"
#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/model.h"
#include "tensorflow/lite/optional_debug_tools.h"
// Logger
#include "logger.h"
// For AVFRAME
#include "libav_deps.h"

TensorflowSupervisedInference::TensorflowSupervisedInference(
    SupervisedModel model)
    : model(model) {}
TensorflowSupervisedInference::~TensorflowSupervisedInference() {}

std::vector<std::string> TensorflowSupervisedInference::readLinesFromFile(
    const std::string filename) const {
  {
    std::ifstream infile(filename);
    std::vector<std::string> fileLines;

    if (infile.is_open()) {
      std::string line;
      while (std::getline(infile, line)) {
        fileLines.push_back(line);
      }
      infile.close();
    }

    return fileLines;
  }
}

void TensorflowSupervisedInference::loadLabels() {
  labels.clear();
  labels = readLinesFromFile(model.labelsPath);
  // The real number of labels may not match the labels.size() because of
  // padding
  nbLabels = labels.size();
  // Some models require the labels vector to be a multiple of a certain size
  padLabels();
}

std::string TensorflowSupervisedInference::getLabel(size_t position) const {
  return labels[position];
}

void TensorflowSupervisedInference::padLabels() {
  while (labels.size() % model.labelsPadding) {
    labels.emplace_back();
  }
}

void TensorflowSupervisedInference::loadModel() {
  tfmodel = tflite::FlatBufferModel::BuildFromFile(model.modelPath.c_str());
}

void TensorflowSupervisedInference::buildInterpreter() {
  // Build the interpreter
  tflite::ops::builtin::BuiltinOpResolver resolver;
  tflite::InterpreterBuilder builder(*tfmodel, resolver);
  builder(&interpreter);
}

void TensorflowSupervisedInference::setInterpreterSettings() {
  interpreter->UseNNAPI(model.useNNAPI);
  interpreter->SetAllowFp16PrecisionForFp32(model.allowFp16PrecisionForFp32);
  interpreter->SetNumThreads(static_cast<int>(model.numberOfThreads));
}

void TensorflowSupervisedInference::init() {
  // Loading the model
  loadModel();
  buildInterpreter();
  setInterpreterSettings();
  // Loading the labels
  loadLabels();
}

void TensorflowSupervisedInference::describeModelTensors() const {
  if (interpreter->AllocateTensors() != kTfLiteOk) {
    JAMI_DBG() << "Failed to allocate tensors!"
               << "\n";
  }

  PrintInterpreterState(interpreter.get());
  JAMI_DBG() << "=============== inputs/outputs dimensions ==============="
             << "\n";
  const std::vector<int> inputs = interpreter->inputs();
  const std::vector<int> outputs = interpreter->outputs();
  JAMI_DBG() << "number of inputs: " << inputs.size() << "\n";
  JAMI_DBG() << "number of outputs: " << outputs.size() << "\n";

  int input = interpreter->inputs()[0];
  int output = interpreter->outputs()[0];
  JAMI_DBG() << "input 0 index: " << input << "\n";
  JAMI_DBG() << "output 0 index: " << output << "\n";
  JAMI_DBG() << "=============== input dimensions ==============="
             << "\n";
  // get input dimension from the input tensor metadata
  // assuming one input only

  for (size_t i = 0; i < inputs.size(); i++) {
    std::stringstream ss;
    ss << "Input  " << i << "   ➛ ";
    describeTensor(ss.str(), interpreter->inputs()[i]);
  }

  JAMI_DBG() << "=============== output dimensions ==============="
             << "\n";
  // get input dimension from the input tensor metadata
  // assuming one input only
  for (size_t i = 0; i < outputs.size(); i++) {
    std::stringstream ss;
    ss << "Output " << i << "   ➛ ";
    describeTensor(ss.str(), interpreter->outputs()[i]);
  }
}

void TensorflowSupervisedInference::describeTensor(std::string prefix,
                                                   int index) const {
  std::vector<int> dimensions = getTensorDimensions(index);
  size_t nbDimensions = dimensions.size();

  std::ostringstream tensorDescription;
  tensorDescription << prefix;
  for (size_t i = 0; i < nbDimensions; i++) {
    if (i == dimensions.size() - 1)
      tensorDescription << dimensions[i];
    else {
      tensorDescription << dimensions[i] << " x ";
    }
  }
  JAMI_DBG() << tensorDescription.str() << "\n";
}

std::vector<int>
TensorflowSupervisedInference::getTensorDimensions(int index) const {
  TfLiteIntArray *dims = interpreter->tensor(index)->dims;
  size_t size = static_cast<size_t>(interpreter->tensor(index)->dims->size);
  std::vector<int> result;
  result.reserve(size);
  for (size_t i = 0; i != size; i++) {
    result.push_back(dims->data[i]);
  }

  dims = nullptr;

  return result;
}

void TensorflowSupervisedInference::runGraph() {
  for (size_t i = 0; i < model.numberOfRuns; i++) {
    if (interpreter->Invoke() != kTfLiteOk) {
      JAMI_ERR() << "A problem occured when running the graph";
    }
  }
}
