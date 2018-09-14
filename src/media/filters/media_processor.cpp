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

#include <opencv2/objdetect.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/face.hpp>


#include "logger.h"

#include <fstream>

#include <iostream>


using tensorflow::Flag;
using tensorflow::Tensor;
using tensorflow::Status;
using tensorflow::string;
using tensorflow::int32;
using tensorflow::uint8;

using cv::CascadeClassifier;
using cv::face::Facemark;



using namespace std;

namespace ring {

const std::string graph ="/home/tmenais/tutocpp/tensorflow/face_recognition_only_cpp/20170512-110547.pb";
const std::string opencvcascade ="/home/tmenais/tutocpp/tensorflow/face_recognition_only_cpp/haarcascade_frontalface_alt.xml";
const std::string landmarkfile ="/home/tmenais/tutocpp/tensorflow/face_recognition_only_cpp/lbfmodel.yaml";
const std::string label_database_file = "/home/tmenais/tutocpp/tensorflow/face_recognition_only_cpp/face_embeddings_database.txt";


const std::string input_layer = "input";
const std::string phase_train_layer = "phase_train";
const std::string output_layer = "embeddings";



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

std::vector<std::string> ReadLabelsAndEmbeddings(const std::string file_name) {
    
    std::ifstream infile(file_name);
    std::string label_file;
    std::vector<std::string> labels_files;
    if(infile.is_open()){
        while(std::getline(infile, label_file)){
            labels_files.push_back(label_file);
        }
          infile.close();
    }
    return labels_files;
}

std::vector<float> ConvStringToFloats(std::string str){

    std::vector<float> vect;
    std::istringstream stm(str) ;
    
    float number;

    while(stm >> number){
       
                vect.push_back(number);
                            
    }

    return vect;
}



MediaProcessor::MediaProcessor()
    : session_(nullptr)
    , cascade_(nullptr)
    , facemark_()
    , lastInput_(nullptr)
    , scaler_ (new video::VideoScaler())
    , labels_ ()
    , lastOutput_(nullptr)
{
    t = std::thread([this] {
        
        session_ = initSession();
        cascade_ = initCascade();
        facemark_ = initFacemark();

        database_ = ReadLabelsAndEmbeddings(label_database_file);
        label_database_.reserve(database_.size());
        embeddings_database_.reserve(database_.size());       


        for (const auto& mystring : database_){

            label_database_.emplace_back(mystring.substr(0, mystring.find_first_of(" ")));
            embeddings_database_.emplace_back(mystring.substr(mystring.find_first_of(" ")+1));

        }
        

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
        session_->Close();
        session_.reset();
    }
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

std::unique_ptr<cv::CascadeClassifier>
MediaProcessor::initCascade()
{       
    std::unique_ptr<cv::CascadeClassifier> cascade ;   
    cascade = std::make_unique<cv::CascadeClassifier>(opencvcascade);
    cascade->load(opencvcascade);  
  
    return cascade;
}

cv::Ptr<cv::face::Facemark>
MediaProcessor::initFacemark()
{   
    cv::Ptr<cv::face::Facemark> facemark = cv::face::FacemarkLBF::create();
    facemark->loadModel(landmarkfile);

    return facemark;
}

cv::Mat faceCenterRotateCrop(cv::Mat &im, vector<cv::Point2f> landmarks, cv::Rect face){

    //description of the landmarks in case someone wants to do something custom
    // landmarks 0, 16           // Jaw line
    // landmarks 17, 21          // Left eyebrow
    // landmarks 22, 26          // Right eyebrow
    // landmarks 27, 30          // Nose bridge
    // landmarks 30, 35          // Lower nose
    // landmarks 36, 41          // Left eye
    // landmarks 42, 47          // Right Eye
    // landmarks 48, 59          // Outer lip
    // landmarks 60, 67          // Inner lip


    // 2D image points. If you change the image, you need to change vector
    std::vector<cv::Point2d> image_points;
    image_points.push_back(landmarks[30]);    // Nose tip
    image_points.push_back(landmarks[8]);     // Chin
    image_points.push_back(landmarks[45]);    // Left eye left corner
    image_points.push_back(landmarks[36]);    // Right eye right corner
    image_points.push_back(landmarks[54]);    // Left Mouth corner
    image_points.push_back(landmarks[48]);    // Right mouth corner

    // 3D model points.
    std::vector<cv::Point3d> model_points;
    model_points.push_back(cv::Point3d(0.0f, 0.0f, 0.0f));               // Nose tip
    model_points.push_back(cv::Point3d(0.0f, -330.0f, -65.0f));          // Chin
    model_points.push_back(cv::Point3d(-225.0f, 170.0f, -135.0f));       // Left eye left corner
    model_points.push_back(cv::Point3d(225.0f, 170.0f, -135.0f));        // Right eye right corner
    model_points.push_back(cv::Point3d(-150.0f, -150.0f, -125.0f));      // Left Mouth corner
    model_points.push_back(cv::Point3d(150.0f, -150.0f, -125.0f));       // Right mouth corner

    // Camera internals
    double focal_length = im.cols; // Approximate focal length. //3 nb channels
    cv::Point2d center = cv::Point2d(im.cols/2,im.rows/2);
    cv::Mat camera_matrix = (cv::Mat_<double>(3,3) << focal_length, 0, center.x, 0 , focal_length, center.y, 0, 0, 1);
    cv::Mat dist_coeffs = cv::Mat::zeros(4,1,cv::DataType<double>::type); // Assuming no lens distortion

    cv::Mat rotation_vector; // Rotation in axis-angle form
    cv::Mat translation_vector;
         
    // Solve for pose
    cv::solvePnP(model_points, image_points, camera_matrix, dist_coeffs, rotation_vector, translation_vector);

    // Access the last element in the Rotation Vector
    double rot = rotation_vector.at<double>(0,2);
    double theta_deg = rot/M_PI*180;
    
    // Rotate around the center    
    cv::Point2f pt = landmarks[30]; //center is nose tip
    cv::Mat r = getRotationMatrix2D(pt, theta_deg, 1.0);
    // determine bounding rectangle
    cv::Rect bbox = cv::RotatedRect(pt,im.size(), theta_deg).boundingRect();

    // Apply affine transform
    cv::Mat dst;
    warpAffine(im, dst, r, bbox.size());

    // Now crop the face
    cv::Mat Cropped_Face = dst(face);

    resize( Cropped_Face, Cropped_Face, cv::Size(160, 160), CV_INTER_LINEAR);
    

    return Cropped_Face ;
}

void drawBox(cv::Mat &original, cv::Rect r, std::string label){

    double scale=1;
    int linewidth = std::max(1, int(original.rows * .005));
    cv::Scalar color = cv::Scalar(0, 0, 255); // Color for Drawing tool

    cv::rectangle( original, r, color, linewidth, 8, 0);

    cout <<"in drawBox "  << endl;
    cv::Point txt_up = cvPoint(cvRound(r.x*scale + linewidth ), cvRound(r.y*scale - 4 * linewidth));      
    cv::Point txt_in = cvPoint(cvRound(r.x*scale + linewidth ), cvRound(r.y*scale + 12 * linewidth));
     
    if ( cvRound(r.y*scale -12 * linewidth) > 0 )
    {
        cv::putText(original, label, txt_up, 1, linewidth , color );
    }else{ // write in box if box is too high
        cv::putText(original, label, txt_in, 1, linewidth , color );
    }        
    
    return ;    
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

    double scale=1;
    int linewidth = std::max(1, int(output->height * .005));
    cv::Scalar color = cv::Scalar(255, 0, 0); // Color for Drawing tool
    cv::Scalar color2 = cv::Scalar(0, 0, 255); // Color for Drawing tool

    cv::Mat opencvframe(output->height,output->width, CV_8UC3, output->data[0], output->linesize[0]);

    

    {
        std::lock_guard<std::mutex> l(inputLock);
        cv::Mat opencvframe_copy = opencvframe.clone();
        cv::cvtColor(opencvframe_copy, opencvframe_copy, CV_BGR2RGB);
        lastInput_ = std::make_unique<cv::Mat>(std::move(opencvframe_copy));
    }
    inputCv.notify_all();


    if (lastOutput_)
    {
        for (int i = 0; i < (*lastOutput_).rects.size(); ++i)
        {
            cout <<"Hello " <<  (*lastOutput_).labels[i] << ", confidence :" << (*lastOutput_).confidence[i]  << endl;
            drawBox(opencvframe, (*lastOutput_).rects[i],(*lastOutput_).labels[i]);
        }
        
    }

    av_frame_unref(frame);
    av_frame_move_ref(frame, output);

    return ;

}




std::shared_ptr<Mask>
MediaProcessor::process(cv::Mat input)
{
    Mask mask;

    cascade_->cv::CascadeClassifier::detectMultiScale( input, mask.rects, 1.1, 
                            7, 0|cv::CASCADE_SCALE_IMAGE , cv::Size(30, 30) );

    if (mask.rects.size() == 0) {
        return nullptr;
    }

    vector<vector<cv::Point2f>> landmarks;

    bool success = facemark_->fit(input,mask.rects,landmarks);

    double scale = 1;
    double fx = 1 / scale;
    if (!success)
    {
        return nullptr;
    }

    mask.labels.reserve(mask.rects.size());
    mask.confidence.reserve(mask.rects.size());
    
    size_t i=0;
    for (const auto& r : mask.rects)
    {
        cv::Mat smallImgROI ;
        const auto& landmark = landmarks[i];

        // If success, align face
        if (landmark.size()==68) { //68 landmarks detected by the model
            smallImgROI = faceCenterRotateCrop(input, landmark, r);
        }

        if(! smallImgROI.data )                // Check for invalid input
        {
            std::cout <<  "Could not open or find the image" << std::endl ;
            return nullptr;
        }

          
        auto data = smallImgROI.data;
        Tensor input_tensor(tensorflow::DT_FLOAT, tensorflow::TensorShape({1,smallImgROI.rows,smallImgROI.cols,3}));
        auto input_tensor_mapped = input_tensor.tensor<float, 4>(); 

        for (int x = 0; x < smallImgROI.cols; ++x) {
            for (int y = 0; y < smallImgROI.rows; ++y) {
                for (int c = 0; c < 3; ++c) {
                    int offset = y * smallImgROI.cols + x * 3 + c;
                    input_tensor_mapped(0, y, x, c) = tensorflow::uint8(data[offset]);
                }
            }
        }

        tensorflow::Tensor phase_tensor(tensorflow::DT_BOOL, tensorflow::TensorShape());
        phase_tensor.scalar<bool>()() = false;

        //Run session
        std::vector<Tensor> outputs ;
        Status run_status = session_->Run({{input_layer, input_tensor},
                                       {phase_train_layer, phase_tensor}}, 
                                       {output_layer}, {}, &outputs);

        if (!run_status.ok()) {
            LOG(ERROR) << "\tRunning model failed: " << run_status << "\n";
            return nullptr;
        }

        auto output_c = outputs[0].tensor<float, 2>();

        float min_emb_diff =10000;
        
        int posofmin=0;


        for (int k = 0; k < label_database_.size(); ++k){
            float diff=0;

            std::vector<float> vect = ConvStringToFloats(embeddings_database_[k]);


            for (int j = 0; j < outputs[0].shape().dim_size(1); ++j){
                diff += (output_c(0,j) - vect[j]) * (output_c(0,j) - vect[j]) ;
            }
            //diff= sqrt(diff); //no need to sqrt, just adapt the threshold
            if (diff < min_emb_diff)
            {
                min_emb_diff = diff ;
                posofmin = k;
            }               
        }
        std::cout <<  "just before confidence" << std::endl ;
        mask.confidence.emplace_back(min_emb_diff);
        std::cout <<  "just before label in labels" << std::endl ;
        mask.labels.emplace_back(label_database_[posofmin]);
        std::cout <<  "put labels in labels :" << mask.labels.back() << ", condifence :" << mask.confidence.back()  << std::endl ;

        i++; // counter for landmarks[i]
    }


    std::shared_ptr<Mask> mask_shared = std::make_shared<Mask> (mask);   

    return mask_shared ;     
}


}// namespace ring
