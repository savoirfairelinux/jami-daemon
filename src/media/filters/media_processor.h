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

#include <opencv2/objdetect.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/face.hpp>

#include "libav_deps.h"
#include "media/video/video_scaler.h"


namespace ring {

class MediaProcessor {
public:
    MediaProcessor();
    ~MediaProcessor();

    void addFrame(AVFrame* frame);
    std::shared_ptr<std::vector<cv::Rect>> getResult() { return lastOutput_; };
    std::shared_ptr<std::vector<cv::Rect>> getResult2() { return lastOutput2_; };
    
    void stop() {
        running = false;
        inputCv.notify_all();
    }

private:
	
    std::unique_ptr<cv::CascadeClassifier> cascade_;
    std::unique_ptr<cv::CascadeClassifier> cascade2_;
    std::unique_ptr<cv::Mat> lastInput_;
    std::shared_ptr<std::vector<cv::Rect>> lastOutput_ {nullptr};
    std::shared_ptr<std::vector<cv::Rect>> lastOutput2_ {nullptr};
 
    std::unique_ptr<cv::CascadeClassifier> initCascade(const std::string cvcascade);
    std::unique_ptr<video::VideoScaler> scaler_;
    

    std::shared_ptr<std::vector<cv::Rect>> process(cv::Mat);
    std::shared_ptr<std::vector<cv::Rect>> process2(cv::Mat);

    bool running {true};
    std::thread t;

    std::mutex inputLock;
    std::condition_variable inputCv;

};

}//namespace ring
