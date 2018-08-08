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

#include "logger.h"

#include <fstream>

#include <iostream>


using tensorflow::Flag;
using tensorflow::Tensor;
using tensorflow::Status;
using tensorflow::string;
using tensorflow::int32;
using tensorflow::uint8;

namespace ring {

const std::string image ="/home/tmenais/tutocpp/tensorflow/data/dog.jpg" ;
const std::string graph ="/home/tmenais/tutocpp/tensorflow/data/faster_rcnn_resnet101_coco_2018_01_28/frozen_inference_graph.pb";
const std::string labels ="/home/tmenais/tutocpp/tensorflow/data/mscoco_label2.txt";
const std::string input_layer = "image_tensor:0";
const std::vector<std::string> output_layer ={ "detection_boxes:0", "detection_scores:0", "detection_classes:0", "num_detections:0" };
const std::string root_dir = "/home/tmenais/tutocpp/tensorflow";


// Takes a file name, and loads a list of labels from it, one per line, and
// returns a vector of the strings. It pads with empty strings so the length
// of the result is a multiple of 16, because our model expects that.
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

     // size_t i =0;
     // for (int i = 0; i < labels.size(); ++i)
     // {
     //     std::cout<< labels.at(i) << " " << i+1 << std::endl;
     //     //RING_WARN("%s", labels.at(i).c_str());
     // }

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

// Given an image file name, read in the data, try to decode it as an image,
// resize it to the requested size, and then scale the values as desired.
/*
tensorflow::Status ReadTensorFromImageFile(const std::string& file_name, std::vector<Tensor>* out_tensors)
{
    auto root = tensorflow::Scope::NewRootScope();
    using namespace ::tensorflow::ops;  // NOLINT(build/namespaces)

    std::string input_name = "file_reader";
    std::string output_name = "normalized";

    // read file_name into a tensor named input
    Tensor input(tensorflow::DT_STRING, tensorflow::TensorShape());
    TF_RETURN_IF_ERROR(
        ReadEntireFile(tensorflow::Env::Default(), file_name, &input));

    // use a placeholder to read input data
    auto file_reader =
        Placeholder(root.WithOpName("input"), tensorflow::DataType::DT_STRING);

    std::vector<std::pair<std::string, tensorflow::Tensor>> inputs = {
        {"input", input},
    };

    // Now try to figure out what kind of file it is and decode it.
    const int wanted_channels = 3;
    tensorflow::Output image_reader;
    if (tensorflow::str_util::EndsWith(file_name, ".png")) {
        image_reader = DecodePng(root.WithOpName("png_reader"), file_reader,
                             DecodePng::Channels(wanted_channels));
    } else if (tensorflow::str_util::EndsWith(file_name, ".gif")) {
    // gif decoder returns 4-D tensor, remove the first dim
        image_reader =
        Squeeze(root.WithOpName("squeeze_first_dim"),
                DecodeGif(root.WithOpName("gif_reader"), file_reader));
    } else {
    // Assume if it's neither a PNG nor a GIF then it must be a JPEG.
        image_reader = DecodeJpeg(root.WithOpName("jpeg_reader"), file_reader,
                              DecodeJpeg::Channels(wanted_channels));
    }
    // Now cast the image data to float so we can do normal math on it.
    // auto float_caster =
    //     Cast(root.WithOpName("float_caster"), image_reader, tensorflow::DT_FLOAT);

    auto uint8_caster =  Cast(root.WithOpName("uint8_caster"), image_reader, tensorflow::DT_UINT8);

    // The convention for image ops in TensorFlow is that all images are expected
    // to be in batches, so that they're four-dimensional arrays with indices of
    // [batch, height, width, channel]. Because we only have a single image, we
    // have to add a batch dimension of 1 to the start with ExpandDims().
    auto dims_expander = ExpandDims(root.WithOpName("dim"), uint8_caster, 0);

    // This runs the GraphDef network definition that we've just constructed, and
    // returns the results in the output tensor.
    tensorflow::GraphDef graph;
    TF_RETURN_IF_ERROR(root.ToGraphDef(&graph));

    std::unique_ptr<tensorflow::Session> session(
        tensorflow::NewSession(tensorflow::SessionOptions()));
    TF_RETURN_IF_ERROR(session->Create(graph));
    TF_RETURN_IF_ERROR(session->Run({inputs}, {"dim"}, {}, out_tensors));
    return Status::OK();
}
*/

/*
Status SaveImage(const Tensor& tensor, const string& file_path) {
    LOG(INFO) << "Saving image to " << file_path;
    CHECK(tensorflow::str_util::EndsWith(file_path, ".png"))
    << "Only saving of png files is supported.";

    RING_WARN("IMAGE SHOULD BE SAVED 1");

    auto root = tensorflow::Scope::NewRootScope();
    using namespace ::tensorflow::ops;  // NOLINT(build/namespaces)

    string encoder_name = "encode";
    string output_name = "file_writer";
    

    RING_WARN("IMAGE SHOULD BE SAVED 2");

    

    tensorflow::Output image_encoder = EncodePng(root.WithOpName(encoder_name), tensor);
    RING_WARN("IMAGE SHOULD BE SAVED 3");
    tensorflow::ops::WriteFile file_saver = tensorflow::ops::WriteFile(root.WithOpName(output_name), file_path, image_encoder);
    RING_WARN("IMAGE SHOULD BE SAVED 4");

    tensorflow::GraphDef graph;
    TF_RETURN_IF_ERROR(root.ToGraphDef(&graph));
    auto ret = root.ToGraphDef(&graph);
    std::cout << "error: " << ret << std::endl;
    RING_WARN("IMAGE SHOULD BE SAVED 5");

    std::unique_ptr<tensorflow::Session> session(tensorflow::NewSession(tensorflow::SessionOptions()));
    RING_WARN("IMAGE SHOULD BE SAVED 6");
    TF_RETURN_IF_ERROR(session->Create(graph));
    RING_WARN("IMAGE SHOULD BE SAVED 7");
    std::vector<Tensor> outputs;
    TF_RETURN_IF_ERROR(session->Run({}, {}, {output_name}, &outputs));

    RING_WARN("IMAGE SHOULD BE SAVED 8");

    return Status::OK();
}
*/

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
    RING_WARN("LoadGraph() Load graph :)");
    session->reset(tensorflow::NewSession(tensorflow::SessionOptions()));
    Status session_create_status = (*session)->Create(graph_def);
    if (!session_create_status.ok()) {
        RING_WARN("LoadGraph() Create session :(");
        return session_create_status;
    }

    RING_WARN("LoadGraph() :)");
    return Status::OK();
}



float colors[6][3] = { {1,0,1}, {0,0,1},{0,1,1},{0,1,0},{1,1,0},{1,0,0} };

float get_color(int c, int x, int max)
{
    float ratio = ((float)x/max)*5;
    int i = floor(ratio);
    int j = ceil(ratio);
    ratio -= i;
    float r = (1-ratio) * colors[i][c] + ratio*colors[j][c];
    //printf("%f\n", r);
    return r;
}

void draw_box(AVFrame* a, int x1, int y1, int x2, int y2, int r, int g, int b)
{
    //normalize_image(a);
    auto data = a->data[0];
    auto linesize = a->linesize[0];
    if(x1 < 0) x1 = 0;
    if(x1 >= a->width) x1 = a->width-1;
    if(x2 < 0) x2 = 0;
    if(x2 >= a->width) x2 = a->width-1;

    if(y1 < 0) y1 = 0;
    if(y1 >= a->height) y1 = a->height-1;
    if(y2 < 0) y2 = 0;
    if(y2 >= a->height) y2 = a->height-1;

    for(size_t i = x1; i <= x2; ++i){
        data[i * 3 + y1 * linesize] = r;
        data[i * 3 + y2 * linesize] = r;

        data[i * 3 + y1 * linesize + 1] = g;
        data[i * 3 + y2 * linesize + 1] = g;

        data[i * 3 + y1 * linesize + 2] = b;
        data[i * 3 + y2 * linesize + 2] = b;
    }
    for(size_t i = y1; i <= y2; ++i){
        data[x1 * 3 + i * linesize] = r;
        data[x2 * 3 + i * linesize] = r;

        data[x1 * 3 + i * linesize + 1] = g;
        data[x2 * 3 + i * linesize + 1] = g;

        data[x1 * 3 + i * linesize + 2] = b;
        data[x2 * 3 + i * linesize + 2] = b;
    }
}

void draw_box_width(AVFrame* a, int x1, int y1, int x2, int y2, int w, int r, int g, int b)
{
    for(size_t i = 0; i < w; ++i){
        draw_box(a, x1+i, y1+i, x2-i, y2-i, r, g, b);
    }
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
        RING_WARN("Session still there");
        //session_->Status.reset(); // Tensorflow should do that by itself !!!
        //session_->Create();
        session_->Close();
        session_.reset();
    }
    if (session_) { RING_WARN("SESSION STILL THERE");}//good !!!
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
    buffoutput.reserve(video::VIDEO_PIXFMT_RGB, frame->width, frame->height);
    
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
                int r = get_color(0,offset,int(classes(i)))*255;
                int g = get_color(1,offset,int(classes(i)))*255;
                int b = get_color(2,offset,int(classes(i)))*255;

                LOG(ERROR) << i+1 << ",score:" << scores(i) << ",class:" << labels_.at(classes(i)-1) <<", " << classes(i) <<
                 ",box:" << "," << boxes(0,i,0) << "," << boxes(0,i,1) << "," << boxes(0,i,2)<< "," << boxes(0,i,3);

                draw_box_width(output, boxes(0,i,1)*output->width, boxes(0,i,0)*output->height, boxes(0,i,3)*output->width, boxes(0,i,2)*output->height, linewidth, r, g, b);

                
            }
        }
        LOG(ERROR) << " ";
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
