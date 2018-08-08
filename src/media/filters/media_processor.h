
#pragma once

#include <memory>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>

#include "libav_deps.h"

namespace tensorflow {

class Tensor;
class Session;

}

namespace ring {

class MediaProcessor {
public:
    MediaProcessor();
    ~MediaProcessor();

    void addFrame(AVFrame* frame);
    
    void stop() {
        running = false;
        inputCv.notify_all();
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

}
