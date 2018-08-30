/*
 *  Copyright (C) 2015-2018 Savoir-faire Linux Inc.
 *
 *  Author: Timothée Menais <timothee.menais@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */
#pragma once

#include <memory>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>

#include "libav_deps.h"
#include "media/video/video_scaler.h"

namespace tensorflow {

class Tensor;
class Session;

}

namespace jami {

class MediaProcessor {
public:
    MediaProcessor();
    ~MediaProcessor();

    void addFrame(AVFrame* frame);
    std::shared_ptr<std::vector<tensorflow::Tensor>> getResult() { return lastOutput_; };
    void stop() {
        running = false;
        inputCv.notify_all();
    }

private:
	std::unique_ptr<tensorflow::Session> session_;
    std::unique_ptr<tensorflow::Tensor> lastInput_;
    std::shared_ptr<std::vector<tensorflow::Tensor>> lastOutput_ {nullptr};
    std::vector<std::string> labels_;

    std::unique_ptr<tensorflow::Session> initSession();
    std::unique_ptr<video::VideoScaler> scaler_;
    std::vector<std::string> ReadLabelsFile(const std::string file_name);

    std::shared_ptr<std::vector<tensorflow::Tensor>> process(const tensorflow::Tensor&);

    bool running {true};
    std::thread t;

    std::mutex inputLock;
    std::condition_variable inputCv;
};

}
