#! /bin/bash
# Build the plugin for the project
SO_FILE_NAME="jamimultipleobjecttrackingtensorflow.so"
LIBS_DIR="/home/ayounes/Libs"
PREFIX="x86_64"
OPENCV_LIBRARY="/home/ayounes/Projects/ring-project/daemon/contrib/x86_64-linux-gnu"
clang++ -std=c++14 -shared -fPIC \
	-lstdc++ \
	-I"../.." \
	-I".." \
	-I"." \
	-I${OPENCV_LIBRARY}/include \
	-I${LIBS_DIR}/flatbuffers \
	-I${LIBS_DIR}/tensorflow \
	multipleobjecttrackingplugin.cpp \
	media_processor.cpp \
        tensorflowsupervisedinference.cpp \
	multipleobjecttracking.cpp \
	reactive_streams_signals.cpp \
	-L${LIBS_DIR}/_tensorflow_dist_/lib/${PREFIX}  -ltensorflowlite \
	-L${OPENCV_LIBRARY}/lib -lopencv_core -lopencv_imgproc \
	-o ${SO_FILE_NAME}
