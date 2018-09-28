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
#include "debug_utils.h"


namespace ring {
class MediaFilter;

class MediaProcessor {
public:
    MediaProcessor();
    ~MediaProcessor();

    void addFrame(AVFrame* frame);
    std::shared_ptr<std::vector<cv::Rect>> getResult() { return lastOutput_; };
    AVFrame* getAudioResult() { return lastAudioOutput_; };
    
    
    void stop() {
        running = false;
        audiorunning = false;
        inputCv.notify_all();
        inputAudioCv.notify_all();
    }

private:
	
    std::unique_ptr<cv::CascadeClassifier> cascade_;
    std::unique_ptr<cv::CascadeClassifier> cascade2_;
    std::unique_ptr<cv::Mat> lastInput_;
    AVFrame* lastAudioInput_;
    std::shared_ptr<std::vector<cv::Rect>> lastOutput_ {nullptr};
    AVFrame* lastAudioOutput_ {nullptr};
    std::unique_ptr<MediaFilter> Filter_;
    
    std::unique_ptr<cv::CascadeClassifier> initCascade(const std::string cvcascade);
    std::unique_ptr<video::VideoScaler> scaler_;
    

    std::shared_ptr<std::vector<cv::Rect>> process(cv::Mat);
    AVFrame* processaudio(AVFrame* input);

    bool running {true};
    bool audiorunning {true};
    std::thread t;
    std::thread t2;

    std::mutex inputLock;
    std::mutex inputAudioLock;
    std::condition_variable inputCv;
    std::condition_variable inputAudioCv;
    std::unique_ptr<debug::WavWriter> wav1;
    std::unique_ptr<debug::WavWriter> wav2;

};

}//namespace ring
