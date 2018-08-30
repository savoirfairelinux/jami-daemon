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
#include <algorithm> //std::max

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

#include "/usr/local/include/opencv2/objdetect.hpp"
#include "/usr/local/include/opencv2/highgui.hpp"
#include "/usr/local/include/opencv2/imgproc.hpp"


#include "logger.h"

#include <fstream>

#include <iostream>


using tensorflow::Flag;
using tensorflow::Tensor;
using tensorflow::Status;
using tensorflow::string;
using tensorflow::int32;
using tensorflow::uint8;

using namespace std;

namespace jami {

const std::string image ="/home/tmenais/tutocpp/tensorflow/data/dog.jpg" ;
const std::string graph ="/home/tmenais/tutocpp/tensorflow/data/faster_rcnn_resnet101_coco_2018_01_28/frozen_inference_graph.pb";
const std::string labels ="/home/tmenais/tutocpp/tensorflow/data/mscoco_label2.txt";
const std::string input_layer = "image_tensor:0";
const std::vector<std::string> output_layer ={ "detection_boxes:0", "detection_scores:0", "detection_classes:0", "num_detections:0" };


// Takes a file name, and loads a list of labels from it, one per line, and
// returns a vector of the strings.
std::vector<std::string>
MediaProcessor::ReadLabelsFile(const std::string file_name) {

    std::ifstream infile(file_name);
    std::string label;
    std::vector<std::string> labels;

    if(infile.is_open()){
        while(std::getline(infile, label)){
            labels.push_back(label);
        }
        infile.close();
    }

    return labels;
}


// Reads a model graph definition from disk, and creates a session object you
// can use to run it.
tensorflow::Status LoadGraph(const std::string& graph_file_name,
                 std::unique_ptr<tensorflow::Session>* session) {
    tensorflow::GraphDef graph_def;
    Status load_graph_status =
        ReadBinaryProto(tensorflow::Env::Default(), graph_file_name, &graph_def);
            if (!load_graph_status.ok()) {
            return tensorflow::errors::NotFound("Failed to load compute graph at '",
                                        graph_file_name, "'");
    }
        session->reset(tensorflow::NewSession(tensorflow::SessionOptions()));
    Status session_create_status = (*session)->Create(graph_def);
    if (!session_create_status.ok()) {
        RING_WARN("LoadGraph() Create session failed");
        return session_create_status;
    }

    return Status::OK();
}



float colors[6][3] = { {255,0,255}, {0,0,255},{0,255,255},{0,255,0},{255,255,0},{255,0,0} };

float get_color(int c, int x, int max)
{
    float ratio = ((float)x/max)*5;
    int i = floor(ratio);
    int j = ceil(ratio);
    ratio -= i;
    float r = (1-ratio) * colors[i][c] + ratio*colors[j][c];
    return r;
}



MediaProcessor::MediaProcessor()
    : session_(nullptr)
    , lastInput_(nullptr)
    , scaler_ (new video::VideoScaler())
    , labels_ ()
{
    t = std::thread([this] {
        labels_ = ReadLabelsFile(labels);

        session_ = initSession();
        

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
}

MediaProcessor::~MediaProcessor()
{   
    RING_WARN("~MediaProcessor");
    stop();
    t.join();
    if (session_) {
        //RING_WARN("Session still there");
        session_->Close();
        session_.reset();
    }
    //if (session_) { RING_WARN("SESSION STILL THERE");}//good !!!
}


std::unique_ptr<tensorflow::Session>
MediaProcessor::initSession()
{
    std::unique_ptr<tensorflow::Session> session;
    Status load_graph_status = LoadGraph(graph, &session);
    
    if (!load_graph_status.ok()) {
        RING_WARN("Failed to initialize a session");
    }   
  
    return session;
}



void
MediaProcessor::addFrame(AVFrame* frame)
{
    RING_WARN("MediaProcessor::addFrame() input %dx%d format: %s", frame->width, frame->height, av_get_pix_fmt_name((AVPixelFormat)frame->format));

    VideoFrame buffoutput;
    buffoutput.reserve(AV_PIX_FMT_RGB24, frame->width, frame->height);
    
    auto output = buffoutput.pointer();
    output->format = AV_PIX_FMT_RGB24;
    output->width = frame->width;
    output->height = frame->height;
    scaler_->scale(frame, output); // converts from YUV to RGB



    Tensor input_tensor(tensorflow::DT_UINT8, tensorflow::TensorShape({1,output->height,output->width,3}));
    auto input_tensor_mapped = input_tensor.tensor<uint8, 4>();    
    auto data = output->data[0];
    auto linesize = output->linesize[0];

    for (int x = 0; x < output->width; ++x) {
        for (int y = 0; y < output->height; ++y) {
            for (int c = 0; c < 3; ++c) {
                int offset = y * linesize + x * 3 + c;
                input_tensor_mapped(0, y, x, c) = tensorflow::uint8(data[offset]);
            }
        }
    }

    cv::Mat opencvframe(output->height,linesize, CV_8UC3, data, linesize);

    

    {
        std::lock_guard<std::mutex> l(inputLock);
        lastInput_ = std::make_unique<tensorflow::Tensor>(std::move(input_tensor));
    }
    inputCv.notify_all();


    if (lastOutput_){
        auto& out = *lastOutput_;
        tensorflow::TTypes<float>::Flat scores = out[1].flat<float>();
        tensorflow::TTypes<float>::Flat classes = out[2].flat<float>();
        tensorflow::TTypes<float>::Flat num_detections = out[3].flat<float>();
       
        
        int linewidth = std::max(1, int(output->height * .005));

        auto boxes = out[0].flat_outer_dims<float,3>();
        RING_WARN("AVFrame modification");

        for(size_t i = 0; i < num_detections(0); ++i) {
            if(scores(i) > 0.7) {

                int offset = 80*123457 % int(classes(i));
                
                LOG(ERROR) << i+1 << ",score:" << scores(i) << ",class:" << labels_.at(classes(i)-1) <<", " << classes(i) <<
                 ",box:" << "," << boxes(0,i,0) << "," << boxes(0,i,1) << "," << boxes(0,i,2)<< "," << boxes(0,i,3);

                cv::Scalar color = cv::Scalar(get_color(0,offset,int(classes(i))), get_color(1,offset,int(classes(i))), get_color(2,offset,int(classes(i))));
                cv::Point tl = cvPoint(boxes(0,i,1) * output->width, boxes(0,i,0) * output->height); //top left of box
                cv::Point br = cvPoint(boxes(0,i,3) * output->width, boxes(0,i,2) * output->height); //bottom right of box
                cv::Point txt_up = cvPoint(boxes(0,i,1) * output->width + linewidth, boxes(0,i,0) * output->height - 5 * linewidth);
                cv::Point txt_in = cvPoint(boxes(0,i,1) * output->width, boxes(0,i,0) * output->height + 12 * linewidth);

                cv::rectangle(opencvframe, tl, br, color, linewidth);                
                if (boxes(0,i,0) > 0.05 ){ 
                    cv::putText(opencvframe, labels_.at(classes(i)-1), txt_up, 1, linewidth , color );
                }else{//writes in box if box is too high
                    cv::putText(opencvframe, labels_.at(classes(i)-1), txt_in, 1, linewidth , color );
                }
            }
        }
    }

    av_frame_unref(frame);
    av_frame_move_ref(frame, output);
}

std::shared_ptr<std::vector<Tensor>>
MediaProcessor::process(const tensorflow::Tensor& input)
{
   
    auto outputs = std::make_shared<std::vector<Tensor>>();
    Status run_status = session_->Run({{input_layer, input}},
                                   output_layer, {}, outputs.get());
   
    
    if (!run_status.ok()) {
       LOG(ERROR) << "Running model failed: " << run_status;
       return nullptr;
    }

       
    return outputs;    
}

}
