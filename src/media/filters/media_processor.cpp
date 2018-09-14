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

const std::string image ="/home/tmenais/tutocpp/tensorflow/data/dog.jpg" ;
const std::string graph ="/home/tmenais/tutocpp/tensorflow/face_recognition_only_cpp/20170512-110547.pb";
const std::string opencvcascade ="/home/tmenais/tutocpp/tensorflow/face_recognition_only_cpp/haarcascade_frontalface_alt.xml";
const std::string landmarkfile ="/home/tmenais/tutocpp/tensorflow/face_recognition_only_cpp/lbfmodel.yaml";
//const std::string labels ="/home/tmenais/tutocpp/tensorflow/data/mscoco_label2.txt";
const std::string label_database_file = "/home/tmenais/tutocpp/tensorflow/face_recognition_only_cpp/face_embeddings_database.txt";
const std::string input_layer = "input";
const std::string phase_train_layer = "phase_train";
const std::string output_layer = "embeddings";
//const std::string input_layer = "image_tensor:0";
//const std::vector<std::string> output_layer ={ "detection_boxes:0", "detection_scores:0", "detection_classes:0", "num_detections:0" };



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

static Status ReadEntireFile(tensorflow::Env* env, const std::string& filename,
                             Tensor* output) {
    tensorflow::uint64 file_size = 0;
    TF_RETURN_IF_ERROR(env->GetFileSize(filename, &file_size));

    std::string contents;
    contents.resize(file_size);

    std::unique_ptr<tensorflow::RandomAccessFile> file;
    TF_RETURN_IF_ERROR(env->NewRandomAccessFile(filename, &file));

    tensorflow::StringPiece data;
    TF_RETURN_IF_ERROR(file->Read(0, file_size, &data, &(contents)[0]));
    if (data.size() != file_size) {
    return tensorflow::errors::DataLoss("Truncated read of '", filename,
                                        "' expected ", file_size, " got ",
                                        data.size());
    }
    output->scalar<std::string>()() = data.ToString();
    return Status::OK();
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
    , cascade_(nullptr)
    , facemark_()
    //, lastInput_(nullptr)
    , lastInput2_(nullptr)
    , scaler_ (new video::VideoScaler())
    , labels_ ()
    , lastOutput2_(nullptr)
{
    t = std::thread([this] {
        //labels_ = ReadLabelsFile(labels);

        session_ = initSession();
        cascade_ = initCascade();
        facemark_ = initFacemark();

        database_ = ReadLabelsAndEmbeddings(label_database_file);
        label_database_  = database_; // segfault if not initialized
        embeddings_database_ = database_; // segfault if not initialized
        embeddings_float_.reserve(database_.size() * 512); //512 embeddings


        for (int i = 0; i < label_database_.size(); ++i){

                std::string mystring = database_[i];
                label_database_[i] = mystring.substr(0, mystring.find_first_of(" "));
                embeddings_database_[i] = mystring.substr(mystring.find_first_of(" ")+1);

        }
        

        while (running) {
            //decltype(lastInput_) input(nullptr);
            decltype(lastInput2_) input2(nullptr);
            {
                std::unique_lock<std::mutex> l(inputLock);
                inputCv.wait(l, [this]{
                    return not running or lastInput2_;
                });
                if (not running) {
                    break;
                }
                //input = std::move(lastInput_);
                input2 = std::move(lastInput2_);
            }
            //lastOutput_ = process(*input);
            lastOutput2_ = process2(*input2);
            //cout <<"in t rects size " <<  lastOutput2_->rects.size() << endl;
            //cout <<"in t labels size " <<  lastOutput2_->labels.size() << endl;
            //cout <<"in t confidence size " <<  lastOutput2_->confidence.size() << endl;
            //cout <<"in t label " <<  lastOutput2_->labels[0] << endl;
            //cout <<"in t confidence " <<  lastOutput2_->confidence[0] << endl;


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
    //if (session_) { RING_WARN("SESSION STILL THERE");}
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
    RING_WARN("init cascade");
    std::unique_ptr<cv::CascadeClassifier> cascade ;
    RING_WARN("init cascade2");
    cascade = std::make_unique<cv::CascadeClassifier>(opencvcascade);
    cascade->load(opencvcascade);
    RING_WARN("init cascade3");
  
    return cascade;
}

cv::Ptr<cv::face::Facemark>
MediaProcessor::initFacemark()
{
    RING_WARN("init facemark");
    //cv::Ptr<cv::face::Facemark> facemark ;
    cv::Ptr<cv::face::Facemark> facemark = cv::face::FacemarkLBF::create();
    //facemark = std::make_unique<cv::Ptr<cv::face::Facemark>>(cv::face::FacemarkLBF::create());
    RING_WARN("init facemark 2");
    facemark->loadModel(landmarkfile);
    RING_WARN("init facemark 3");
  
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
    //cv::Scalar color = cv::Scalar(255, 0, 0); // Color for Drawing tool
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
MediaProcessor::addFrame2(AVFrame* frame)
{
    RING_WARN("MediaProcessor::addFrame2() input %dx%d format: %s", frame->width, frame->height, av_get_pix_fmt_name((AVPixelFormat)frame->format));

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

        //cv::imwrite("analyzed.jpg", opencvframe_copy);
        //lastInput_ = std::make_unique<tensorflow::Tensor>(std::move(input_tensor));
        lastInput2_ = std::make_unique<cv::Mat>(std::move(opencvframe_copy));
    }
    inputCv.notify_all();

    //drawBox(opencvframe, cv::Rect(cvPoint(10,10),cvPoint(150,150)),"plop");

    if (lastOutput2_)
    {


        cout <<"in addFrame2 rects size " <<  (*lastOutput2_).rects.size() << endl;
        cout <<"in addFrame2 labels size " <<  (*lastOutput2_).labels.size() << endl;
        cout <<"in addFrame2 confidence size " <<  (*lastOutput2_).confidence.size() << endl;
        cout <<"in addFrame2 label " <<  (*lastOutput2_).labels[0] << endl;
        cout <<"in addFrame2 confidence " <<  (*lastOutput2_).confidence[0] << endl;

        //cv::Mat newframe;
        //(*lastInput2_).copyTo(opencvframe, (*lastOutput2_));
        for (int i = 0; i < (*lastOutput2_).rects.size(); ++i)
        {
            cout <<"Hello " <<  (*lastOutput2_).labels[i] << ", confidence :" << (*lastOutput2_).confidence[i]  << endl;
            drawBox(opencvframe, (*lastOutput2_).rects[i],(*lastOutput2_).labels[i]);
        }
        
        av_frame_unref(frame);
        av_frame_move_ref(frame, output);    


    }else{        
        av_frame_unref(frame);
        av_frame_move_ref(frame, output);
    }

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
                }else{//write in box if box is too high
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
    //Status run_status = session_->Run({{input_layer, input}},
    //                               output_layer, {}, outputs.get());


   
    
    //if (!run_status.ok()) {
    //   LOG(ERROR) << "Running model failed: " << run_status;
    //   return nullptr;
    //}

       
    return outputs;    
}

std::shared_ptr<Mask>
MediaProcessor::process2(cv::Mat input2)
{
    std::cout <<  "start of process2" << std::endl ;
    std::vector<cv::Rect> faces;
    std::vector<std::string> mask_labels;
    std::vector<float> confidence;

    


    //vector<cv::Rect> face = faces.get() ;
    cascade_->cv::CascadeClassifier::detectMultiScale( input2, faces, 1.1, 
                            7, 0|cv::CASCADE_SCALE_IMAGE , cv::Size(30, 30) );

    RING_WARN("in process2 face detection numb : %d", faces.size());

    if (faces.size() == 0) {
        return nullptr;
    }

    vector<vector<cv::Point2f>> landmarks;

    bool success = facemark_->fit(input2,faces,landmarks);

    double scale = 1;
    double fx = 1 / scale;
    if (!success)
    {
        return nullptr;
    }

    //cv::Mat mask_matrix(input2.rows, input2.cols, CV_8UC3, cv::Scalar(0, 0, 0)) ;


    //cv::Mat mask_matrix = input2;

        //mask->rects = faces;
        mask_labels.reserve(faces.size());
        confidence.reserve(faces.size());
        std::cout <<  "put face in mask" << std::endl ;
        

        for ( size_t i = 0; i <faces.size(); i++ )
        {
            cv::Rect r = faces[i];
            
            // cv::Scalar color = cv::Scalar(255, 0, 0); // Color for Drawing tool

            // cv::rectangle( mask_matrix, cvPoint(cvRound(r.x*scale), cvRound(r.y*scale)),
            //         cvPoint(cvRound((r.x + r.width-1)*scale), 
            //         cvRound((r.y + r.height-1)*scale)), color, linewidth, 8, 0);

            cv::Mat smallImgROI ;

            
            // If success, align face
            if (landmarks[i].size()==68)
            {
                smallImgROI = faceCenterRotateCrop(input2,landmarks[i],faces[i]);
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


            for (int i = 0; i < label_database_.size(); ++i){
                float diff=0;

                std::vector<float> vect = ConvStringToFloats(embeddings_database_[i]);


                for (int j = 0; j < outputs[0].shape().dim_size(1); ++j){
                    diff += (output_c(0,j) - vect[j]) * (output_c(0,j) - vect[j]) ;
                }
                //diff= diff; //no need to sqrt
                if (diff < min_emb_diff)
                {
                    min_emb_diff = diff ;
                    posofmin = i ;
                }                
            }
            std::cout <<  "just before label in labels" << std::endl ;
            //mask->labels.push_back(label_database_[posofmin]);
            //mask->confidence.push_back(min_emb_diff);
            mask_labels[i] = label_database_[posofmin];
            confidence[i] = min_emb_diff;
            std::cout <<  "put labels in labels :" << mask_labels[i] << ", condifence :" << confidence[i]  << std::endl ;

            // cv::Point txt_up = cvPoint(cvRound(r.x*scale + linewidth ), cvRound(r.y*scale - 4 * linewidth));      
            // cv::Point txt_in = cvPoint(cvRound(r.x*scale + linewidth ), cvRound(r.y*scale + 12 * linewidth));

            // if(min_emb_diff < 0.09) {
            //     cout <<"Hello " << label_database_[posofmin] << " confidence: " << min_emb_diff << endl;
            //     if ( cvRound(r.y*scale -12 * linewidth) > 0 )
            //     {
            //         cv::putText(mask_matrix, label_database_[posofmin], txt_up, 1, linewidth , color );
            //     }else{ // write in box if box is too high
            //         cv::putText(mask_matrix, label_database_[posofmin], txt_in, 1, linewidth , color );
            //     }
            // }else{
            //     cout <<"WHO ARE YOU ?"<< " confidence: " << min_emb_diff << endl;
            //     if ( cvRound(r.y*scale - 12 * linewidth) > 0 )
            //     {
            //         cv::putText(mask_matrix, "404", txt_up, 1, linewidth , color );
            //     }else{
            //         cv::putText(mask_matrix, "404", txt_in, 1, linewidth , color );
            //     }
            // }
        //mask.confidence[i] = min_emb_diff;
        }
        //mask.labels = mask_labels;
        //mask.confidence = confidence;

        //for (size_t i = 0; i < faces.size(); ++i) {
        //    mask->labels.emplace_back("label");
        //    mask->confidence.emplace_back(1.0);
        //}

        Mask mask;

        for (size_t i = 0; i < faces.size(); ++i) {
            mask.rects.push_back(faces[i]);
            mask.labels.push_back(mask_labels[i]);
            mask.confidence.push_back(confidence[i]);
        }


        std::cout <<  "put labels in mask" << std::endl ;
        std::shared_ptr<Mask> mask_shared = std::make_shared<Mask> (mask);
        

        std::cout <<  "before imwrite" << std::endl ;
        //cv::imwrite("amask_matrix.jpg", mask_matrix);
        return mask_shared ;

     
}



}
