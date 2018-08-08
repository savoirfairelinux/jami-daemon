
#pragma once

#include <memory>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>

#include "tensorflow/cc/ops/const_op.h"
#include "tensorflow/cc/ops/image_ops.h"
#include "tensorflow/cc/ops/standard_ops.h"
#include "tensorflow/cc/client/client_session.h"

#include "tensorflow/core/framework/graph.pb.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/graph/default_device.h"
#include "tensorflow/core/graph/graph_def_builder.h"
#include "tensorflow/core/lib/core/errors.h"
#include "tensorflow/core/lib/core/stringpiece.h"
#include "tensorflow/core/lib/core/threadpool.h"
#include "tensorflow/core/lib/io/path.h"
#include "tensorflow/core/lib/strings/str_util.h"
#include "tensorflow/core/lib/strings/stringprintf.h"
#include "tensorflow/core/platform/env.h"
#include "tensorflow/core/platform/init_main.h"
#include "tensorflow/core/platform/logging.h"
#include "tensorflow/core/platform/types.h"
#include "tensorflow/core/public/session.h"
#include "tensorflow/core/util/command_line_flags.h"

#include "libav_deps.h"

#include <iostream>

using tensorflow::Flag;
using tensorflow::Tensor;
using tensorflow::Status;
using tensorflow::string;
using tensorflow::int32;
using tensorflow::uint8;

// static void destroyFrameBuffer(AVFrame** frame) {
    
    
//     av_frame_free(frame);
    
// }

// static void destroyTensor(Tensor frame) {
    
    
//     tensorflow::~Tensor(frame);
    
// }

class MediaProcessor {
public:
    //MediaProcessor(const char *datacfg, const char *frozengraph);
    MediaProcessor();
    ~MediaProcessor();

    void addFrame(AVFrame frame);
    
    //std::shared_ptr<std::vector<guchar>> getResult() { return lastOutput; };
    void stop() {
        running = false;
        inputCv.notify_all();

    tensorflow::Tensor convertToTensor(AVFrame frame);


    }



private:
	std::unique_ptr<tensorflow::Session> session_;

    std::unique_ptr<tensorflow::Tensor> lastInput;

    std::unique_ptr<tensorflow::Session> initSession();

    void process(const tensorflow::Tensor&);

    bool running {true};
    std::thread t;

    std::mutex inputLock;
    std::condition_variable inputCv;
};
