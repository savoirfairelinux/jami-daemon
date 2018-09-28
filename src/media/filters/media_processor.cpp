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
#include "media_stream.h"
#include "media_filter.h"
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
    , lastAudioInput_(nullptr)
    , scaler_ (new video::VideoScaler())
    , lastOutput_(nullptr)
    , lastAudioOutput_(nullptr)
{
    
    t = std::thread([this] {

        //Filter_.reset();        
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
        }
    });

    t2 = std::thread([this] {

        Filter_.reset();        
        //cascade_ = initCascade(opencvcascade);
        //cascade2_ = initCascade(opencvcascade2);
         
        
        while (audiorunning) {
            decltype(lastAudioInput_) inputaudio(nullptr);
            {
                std::unique_lock<std::mutex> l2(inputAudioLock);

                inputAudioCv.wait(l2, [this]{
                    return not audiorunning or lastAudioInput_;
                });
                if (not audiorunning) {
                    break;
                }
                inputaudio = std::move(lastAudioInput_);
            }
            lastAudioOutput_ = processaudio(inputaudio);
        }
    });
}

MediaProcessor::~MediaProcessor()
{   
    RING_WARN("~MediaProcessor");
    stop();
    t.join();
    t2.join();
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

    //double scale=1;
    //int linewidth = std::max(1, int(original.rows * .005));
    //cv::Scalar color = cv::Scalar(0, 0, 255); // Color for Drawing tool
    //cv::Point faceCenter(r.x + r.width/2, r.y + r.height/2);

    //cv::rectangle( original, r, color, linewidth, 8, 0);

    int blurlevel = 10;

    for (int i = 0; i < blurlevel; ++i)
    {
    	cv::GaussianBlur(original(r), original(r), cv::Size(29, 29), 4);
    }

    return ;    
}

void
MediaProcessor::addFrame(AVFrame* frame)
{
	if (frame->width > 0 && frame->height > 0) // it is a video frame
	{		
	    //RING_WARN("MediaProcessor::addFrame() input %dx%d format: %s", frame->width, frame->height, av_get_pix_fmt_name((AVPixelFormat)frame->format));

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


	    if (auto out = getResult()){
	        for (int i = 0; i < out->size(); ++i) {
	            //std::cout <<"Bluring" << std::endl;
	            blurFace(opencvframe, (*out)[i]);
	        }
	    } else {
	    	libav_utils::fillWithBlack(output); // black frame before first detection
	    }

	    av_frame_unref(frame);
        av_frame_move_ref(frame, output);

    } else { // it is an audio frame

        //std::cout <<"AUDIOPROCESSINGGGGGGGGGGGGGGGGGGGGGGGG" << std::endl;
        //RING_WARN("MediaProcessor::addFrame() input  format %d nb samples %d ", frame->format, frame->nb_samples);

        if (not Filter_)
        {
            auto ms = MediaStream("plop", frame->format, rational<int>(1, frame->sample_rate), frame->sample_rate, frame->channels);
            std::vector<MediaStream> vec;
            vec.push_back(ms);
            Filter_.reset(new MediaFilter);
            Filter_->initialize("[plop] vibrato=f=7:d=1.0",vec);
        }      

        

        AVFrame* output = av_frame_clone(frame);
        {
            std::lock_guard<std::mutex> l2(inputAudioLock);           
            //lastAudioInput_ = std::make_unique<AVFrame*>(std::move(output));
            lastAudioInput_ = std::move(output);

        }
        inputAudioCv.notify_all();

        if (auto out = getAudioResult()){
            //output = Filter_->readOutput();
       
            if (!wav1)
                wav1.reset(new debug::WavWriter("/tmp/wav1.wav", frame->channels, frame->sample_rate, 2));
            wav1->write(frame);
            if (!wav2)
                wav2.reset(new debug::WavWriter("/tmp/wav2.wav", out->channels, out->sample_rate, 2));
            wav2->write(out);
        
        av_frame_unref(frame);
        av_frame_move_ref(frame, out);
        }


    }

    return ;

}




std::shared_ptr<std::vector<cv::Rect>>
MediaProcessor::process(cv::Mat input)
{
    std::vector<cv::Rect> rects;
    std::vector<cv::Rect> rects2;

    cascade_->cv::CascadeClassifier::detectMultiScale( input, rects, 1.1, 
                            7, 0|cv::CASCADE_SCALE_IMAGE , cv::Size(30, 30) );


    cascade2_->cv::CascadeClassifier::detectMultiScale( input, rects2, 1.1, 
                            7, 0|cv::CASCADE_SCALE_IMAGE , cv::Size(30, 30) );

   
	for (int i = 0; i < rects2.size(); ++i) {
	    	rects.emplace_back(rects2[i]);
	}
    

    if (rects.size() == 0) {
        return lastOutput_; // In case there is no detection on one single frame, there is no sudden unblurring
    } else {

    // std::vector<cv::Rect> rects3;

    // rects3.reserve(rects.size());

    // for (int i = 0; i < rects.size(); ++i){
    // 	bool overlap = false;

    // 	for (int j = i+1; j < rects.size(); ++j){
    // 		if (cv::Rect(rects[i] & rects[j]).area() > 0){ // if 2 rects intersect
    // 			overlap = true;
    // 			rects3.push_back(cv::Rect(rects[i] | rects[j])); // push back the biggest rectangle containing both
    // 		}
    // 	}
    // 	if (overlap == false){
    // 		rects3.emplace_back(rects[i]); // emplace back rects not overlapping any other
    // 	}
    // }
    // this works provided there is no overlapping with more than 2 rectangles, otherwise n -1 double loops 
    // are necessary, with n the max number of rectangles in a cluster ( rects.size() -1 in general case)

    // This should be the correct way to do things but then it is too laggy :(
    // I will leave it here in case someone wants to implement it with openGL

    //std::shared_ptr<std::vector<cv::Rect>> facerects = std::make_shared<std::vector<cv::Rect>> (rects3); 
	
    std::shared_ptr<std::vector<cv::Rect>> facerects = std::make_shared<std::vector<cv::Rect>> (rects);   

    return facerects ; 
    }
}

AVFrame*
MediaProcessor::processaudio(AVFrame* input)
{
    if (auto out = getAudioResult()){
        Filter_->feedInput(input, "plop");

       // std::shared_ptr<AVFrame*> audiooutput = std::make_shared<AVFrame*> (Filter_->readOutput());    
        return Filter_->readOutput();
    } else {
        return nullptr ;
    }

}




}// namespace ring
