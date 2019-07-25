#ifndef MODEL_H
#define MODEL_H

// Std libraries
#include <string>
#include <vector>

struct Model {
    std::string modelPath =
        "/home/ayounes/Projects/data/model/detect_300_quant.tflite";
    std::vector<unsigned int> normalizationValues;

    // Tensorflow specific settings
    bool useNNAPI = true;
    bool allowFp16PrecisionForFp32 = false;
    unsigned int numberOfThreads = 4;

    // User defined details
    bool inputFloating = false;
    unsigned int numberOfRuns = 1;
};

struct SupervisedModel : Model {
    std::string labelsPath =
        "/home/ayounes/Projects/data/model/detect_labels.txt";
    unsigned int labelsPadding = 16;
};

#endif // MODEL_H
