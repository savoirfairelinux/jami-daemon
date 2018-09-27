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
#include "media_processor.h"
#include "media_buffer.h"
#include <vector>

#include <opencv2/objdetect.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/face.hpp>


#include "logger.h"

#include <fstream>

#include <iostream>


using cv::CascadeClassifier;

using namespace std;

namespace ring {

const std::string model_dir= "/usr/local/share/OpenCV/haarcascades/";

const std::string opencvcascade = model_dir + "haarcascade_frontalface_alt.xml";
const std::string opencvcascade2 = model_dir + "haarcascade_profileface.xml";




MediaProcessor::MediaProcessor()
    : cascade_(nullptr)
    , lastInput_(nullptr)
    , scaler_ (new video::VideoScaler())
    , lastOutput_(nullptr)
{
    t = std::thread([this] {
        
        cascade_ = initCascade(opencvcascade);
        cascade2_ = initCascade(opencvcascade2);       
        
        while (running) {
            decltype(lastInput_) input(nullptr);
            {
                std::unique_lock<std::mutex> l(inputLock);

                inputCv.wait(l, [this]{
                    return not running or lastInput_;
                });
                if (not running) {
                    break;
                }
                input = std::move(lastInput_);
            }
            lastOutput_ = process(*input);
            lastOutput2_ = process2(*input);
        
        }
    });
}

MediaProcessor::~MediaProcessor()
{   
    RING_WARN("~MediaProcessor");
    stop();
    t.join();
}


std::unique_ptr<cv::CascadeClassifier>
MediaProcessor::initCascade(const std::string cvcascade)
{       
    std::unique_ptr<cv::CascadeClassifier> cascade ;   
    cascade = std::make_unique<cv::CascadeClassifier>(cvcascade);
    cascade->load(cvcascade);  
  
    return cascade;
}



void blurFace(cv::Mat &original, cv::Rect r){

    double scale=1;
    int linewidth = std::max(1, int(original.rows * .005));
    cv::Scalar color = cv::Scalar(0, 0, 255); // Color for Drawing tool
    cv::Point faceCenter(r.x + r.width/2, r.y + r.height/2);

    //cv::rectangle( original, r, color, linewidth, 8, 0);
    
    cv::GaussianBlur(original(r), original(r), cv::Size(29, 29), 4);
    cv::GaussianBlur(original(r), original(r), cv::Size(29, 29), 4);
    cv::GaussianBlur(original(r), original(r), cv::Size(29, 29), 4);

    return ;    
}

void
MediaProcessor::addFrame(AVFrame* frame)
{
	if (frame->width > 0 && frame->height > 0)
	{		
	    RING_WARN("MediaProcessor::addFrame() input %dx%d format: %s", frame->width, frame->height, av_get_pix_fmt_name((AVPixelFormat)frame->format));

	    VideoFrame buffoutput;
	    buffoutput.reserve(AV_PIX_FMT_RGB24, frame->width, frame->height);
	    
	    auto output = buffoutput.pointer();
	    output->format = AV_PIX_FMT_RGB24;
	    output->width = frame->width;
	    output->height = frame->height;

	    scaler_->scale(frame, output); // converts from YUV to RGB   

	    cv::Mat opencvframe(output->height,output->width, CV_8UC3, output->data[0], output->linesize[0]);

	    

	    {
	        std::lock_guard<std::mutex> l(inputLock);
	        cv::Mat opencvframe_copy = opencvframe.clone();
	        cv::cvtColor(opencvframe_copy, opencvframe_copy, CV_BGR2RGB);
	        lastInput_ = std::make_unique<cv::Mat>(std::move(opencvframe_copy));
	    }
	    inputCv.notify_all();

	    //there is some overlap for the 2 outputs so it should cover both front and profile faces

	    if (auto out = getResult()){
	        for (int i = 0; i < out->size(); ++i) {            
	            std::cout <<"Bluring" << std::endl;
	            blurFace(opencvframe, (*out)[i]);
	        }
	    }

	    if (auto out2 = getResult2()){
	        for (int i = 0; i < out2->size(); ++i) {            
	            std::cout <<"Bluring2" << std::endl;
	            blurFace(opencvframe, (*out2)[i]);
	        }
	    }




	    av_frame_unref(frame);
        av_frame_move_ref(frame, output);

    } else {

    	std::cout <<"AUDIOPROCESSINGGGGGGGGGGGGGGGGGGGGGGGG" << std::endl;

    	AudioFrame buffoutput;
    	auto output = buffoutput.pointer();

        av_frame_unref(frame);
        av_frame_move_ref(frame, output);

    }    

    return ;

}




std::shared_ptr<std::vector<cv::Rect>>
MediaProcessor::process(cv::Mat input)
{
    std::vector<cv::Rect> rects;

    cascade_->cv::CascadeClassifier::detectMultiScale( input, rects, 1.1, 
                            7, 0|cv::CASCADE_SCALE_IMAGE , cv::Size(30, 30) );


    if (rects.size() == 0) {
        return lastOutput_;
    } else {

    std::shared_ptr<std::vector<cv::Rect>> facerects = std::make_shared<std::vector<cv::Rect>> (rects);   

    return facerects ; 
    }
}

std::shared_ptr<std::vector<cv::Rect>>
MediaProcessor::process2(cv::Mat input)
{
    std::vector<cv::Rect> rects;

    cascade2_->cv::CascadeClassifier::detectMultiScale( input, rects, 1.1, 
                            7, 0|cv::CASCADE_SCALE_IMAGE , cv::Size(30, 30) );

    if (rects.size() == 0) {
        return lastOutput2_;
    } else {

    std::shared_ptr<std::vector<cv::Rect>> facerects = std::make_shared<std::vector<cv::Rect>> (rects);   

    return facerects ; 
    }
}


}// namespace ring
