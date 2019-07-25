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
    /**
     * @brief TensorflowSupervisedInference
     * Takes a supervised model where the model and labels files are defined
     * @param model
     */
    TensorflowSupervisedInference(SupervisedModel model);
    ~TensorflowSupervisedInference();
    /**
     * @brief loadLabels
     * Loads the labels from the labels file defined in the Supervised Model
     * Does some padding if the model expects that
     */
    void loadLabels();
    /**
     * @brief loadModel
     * Load the model from the file described in the Supervised Model
     */
    void loadModel();
    void buildInterpreter();
    void setInterpreterSettings();

    /**
     * @brief allocateTensors
     * Tries to allocate space for the tensors
     * In case of success isAllocated() should return true
     */
    void allocateTensors();

    /**
     * @brief runGraph
     * runs the underlaying graph model.numberOfRuns times
     * Where numberOfRuns is defined in the model
     */
    void runGraph();

    /**
     * @brief init
     * Inits the model, interpreter, allocates tensors and load the labels
     */
    void init();
    // Getters
    std::string getLabel(size_t postion) const;
    bool isAllocated() const;
    // Debug methods
    void describeModelTensors() const;
    void describeTensor(std::string prefix, int index) const;

protected:
    /**
     * @brief padLabels
     * Some models expect a label size of a certain type, e.g: power of 2
     * Therefore we need to pad our labels vector to match that size
     */
    void padLabels();

    /**
     * @brief getTensorDimensions
     * Utility method to get Tensorflow Tensor dimensions
     * Given the index of the tensor, the function gives back a vector
     * Where each element is the dimension of the vector-space (finite dimension)
     * Thus, vector.size() is the number of vector-space used by the tensor
     * @param index
     * @return
     */
    std::vector<int> getTensorDimensions(int index) const;

    SupervisedModel model;
    std::vector<std::string> labels;

    /**
     * @brief nbLabels
     * The real number of labels may not match the labels.size() because of padding
     */
    size_t nbLabels;

    // Tensorflow model and interpreter
    std::unique_ptr<tflite::FlatBufferModel> tfmodel;
    std::unique_ptr<tflite::Interpreter> interpreter;
    bool allocated = false;

private:
    std::vector<std::string> readLinesFromFile(const std::string filename) const;
};

#endif // TENSORFLOWSUPERVISEDINFERENCE_H
