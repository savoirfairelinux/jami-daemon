#! /bin/bash
# Build the plugin for the project
SO_FILE_NAME="jamimultipleobjecttrackingtensorflow.so"
LIBS_DIR="/home/ayounes/Libs"
PREFIX="x86_64"
clang++ -std=c++14 -shared -fPIC \
	-lstdc++ \
	-I"../../../" \
	-I"../../filters" \
	-I"." \
	-I${LIBS_DIR}/opencv/dynamic/include/opencv4 \
	-I${LIBS_DIR}/flatbuffers \
	-I${LIBS_DIR}/tensorflow \
	main.cpp \
	media_processor.cpp \
        tensorflowsupervisedinference.cpp \
	multipleobjecttracking.cpp \
	-L${LIBS_DIR}/_tensorflow_dist_/lib/${PREFIX}  -ltensorflowlite \
	-L${LIBS_DIR}/opencv/dynamic/lib -lopencv_core -lopencv_imgproc -lopencv_imgcodecs \
	-o ${SO_FILE_NAME}
