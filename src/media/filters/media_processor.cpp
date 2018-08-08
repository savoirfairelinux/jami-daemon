#include "media_processor.h"
#include <fstream>

#include <iostream>

using namespace std;

const string image ="/home/tmenais/tutocpp/tensorflow/data/dog.jpg" ;
const string graph ="/home/tmenais/tutocpp/tensorflow/data/frozen_inference_graph.pb";
const string labels ="/home/tmenais/tutocpp/tensorflow/data/mscoco_label_map.pbtxt";
const int32 input_width = 299;
const int32 input_height = 299;
const float input_mean = 0;
const float input_std = 255;
const string input_layer = "image_tensor:0";
const vector<string> output_layer ={ "detection_boxes:0", "detection_scores:0", "detection_classes:0", "num_detections:0" };
const string root_dir = "/home/tmenais/tutocpp/tensorflow";




// Takes a file name, and loads a list of labels from it, one per line, and
// returns a vector of the strings. It pads with empty strings so the length
// of the result is a multiple of 16, because our model expects that.
tensorflow::Status ReadLabelsFile(const string& file_name, std::vector<string>* result,
					  size_t* found_label_count) {
	std::ifstream file(file_name);
	if (!file) {
	return tensorflow::errors::NotFound("Labels file ", file_name,
										" not found.");
	}
	result->clear();
	string line;
	while (std::getline(file, line)) {
	    result->push_back(line);
	}
	*found_label_count = result->size();
	const int padding = 16;
	while (result->size() % padding) {
	    result->emplace_back();
	}
	return Status::OK();
}

static Status ReadEntireFile(tensorflow::Env* env, const string& filename,
							 Tensor* output) {
	tensorflow::uint64 file_size = 0;
	TF_RETURN_IF_ERROR(env->GetFileSize(filename, &file_size));

	string contents;
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
	output->scalar<string>()() = data.ToString();
	return Status::OK();
}

// Given an image file name, read in the data, try to decode it as an image,
// resize it to the requested size, and then scale the values as desired.
tensorflow::Status ReadTensorFromImageFile(const string& file_name, std::vector<Tensor>* out_tensors)
{
	auto root = tensorflow::Scope::NewRootScope();
	using namespace ::tensorflow::ops;  // NOLINT(build/namespaces)

	string input_name = "file_reader";
	string output_name = "normalized";

	// read file_name into a tensor named input
	Tensor input(tensorflow::DT_STRING, tensorflow::TensorShape());
	TF_RETURN_IF_ERROR(
	    ReadEntireFile(tensorflow::Env::Default(), file_name, &input));

	// use a placeholder to read input data
	auto file_reader =
	    Placeholder(root.WithOpName("input"), tensorflow::DataType::DT_STRING);

	std::vector<std::pair<string, tensorflow::Tensor>> inputs = {
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

// Status ReadTensorFromBuffer(const float[], const int input_height,
//                                const int input_width,
//                                std::vector<Tensor>* out_tensors) {


// auto file_reader =
//       Placeholder(root.WithOpName("input"), tensorflow::DataType::DT_STRING);

//   std::vector<std::pair<string, tensorflow::Tensor>> inputs = {
//       {"input", input},
//   };

//   auto uint8_caster =  Cast(root.WithOpName("uint8_caster"), image_reader, tensorflow::DT_UINT8);



// }

// Reads a model graph definition from disk, and creates a session object you
// can use to run it.
tensorflow::Status LoadGraph(const string& graph_file_name,
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
		return session_create_status;
	}
	return Status::OK();
}

// tensorflow::Tensor convertToTensor(AVFrame frame){

// 	tensorflow::Tensor tensor;

// 	//make tensor from avframe

// 	return tensor;
// }



//MediaProcessor::MediaProcessor(const char *datacfg, const char *frozengraph)
MediaProcessor::MediaProcessor()
	: session_(nullptr/*, session_->Close()*/), lastInput(nullptr/*, &destroyTensor*/)
{
	//t = std::thread([this, datacfg, frozengraph] {
	t = std::thread([this] {
		
		session_ = initSession();
		

		while (running) {
			decltype(lastInput) input(nullptr/*, nullptr*/);
			{
				std::unique_lock<std::mutex> l(inputLock);
				inputCv.wait(l, [this]{
					return not running or lastInput;
				});
				if (not running) {
					break;
				}
				input = std::move(lastInput);
			}
			//lastOutput = MediaProcessor::process(*input);
			//tensorflow:Tensor convertedFrame = convertToTensor(*input);
			//MediaProcessor::process(convertedFrame);
			MediaProcessor::process(*input);

		}
	});
}

MediaProcessor::~MediaProcessor() {
	stop();

	t.join();
}


std::unique_ptr<tensorflow::Session>
MediaProcessor::initSession() {
	

	 // First we load and initialize the model.
    std::unique_ptr<tensorflow::Session> session;
    string graph_path = tensorflow::io::JoinPath(root_dir, graph);
    LOG(ERROR) << "graph_path:" << graph_path;
    Status load_graph_status = LoadGraph(graph_path, &session);
    // if (!load_graph_status.ok()) {
	   //  LOG(ERROR) << "LoadGraph ERROR!!!!"<< load_graph_status;
	   //  return -1;
    // }
  
  return session;
}

void
MediaProcessor::addFrame(AVFrame buffertoanalyze) {
	//decltype(lastInput) frame { new tensorflow::Tensor/*, &destroyTensor */};
	std::vector<Tensor> frames;
	//do conversion from buffer
	Status read_tensor_status = ReadTensorFromImageFile(image, &frames);

	for(size_t j = 0; j < buffertoanalyze.height ; ++j){
		for(size_t i = 0; i < buffertoanalyze.width ; ++i) {

			//
			
			////direct conversion BGRA->darknet format
			//frame->data[i + frame->w * j ] = buffertoanalyze->data[(i + frame->w * j) * 4 +2]/255.;
			//frame->data[i + frame->w * j + frame->w * frame->h ] = buffertoanalyze[(i + frame->w * j) * 4 + 1]/255.;
			//frame->data[i + frame->w * j + frame->w * frame->h * 2] = buffertoanalyze[(i + frame->w * j) * 4]/255.;
		}
	}                    

	{
		std::lock_guard<std::mutex> l(inputLock);
		lastInput = std::make_unique<tensorflow::Tensor>(std::move(frames[0]));
	}
	inputCv.notify_all();
}

void
MediaProcessor::process(const tensorflow::Tensor& input)
{


	std::vector<Tensor> outputs;
	Status run_status = session_->Run({{input_layer, input}},
                                   output_layer, {}, &outputs);
	//Status run_status = session_->Run({{input_layer, input}},
	//			   {output_score_layer, output_location_layer}, {}, &outputs);
	// if (!run_status.ok()) {
	// 	  LOG(ERROR) << "Running model failed: " << run_status;
	// 	  return -1;
	// }

	tensorflow::TTypes<float>::Flat scores = outputs[1].flat<float>();
	tensorflow::TTypes<float>::Flat classes = outputs[2].flat<float>();
	tensorflow::TTypes<float>::Flat num_detections = outputs[3].flat<float>();
    auto boxes = outputs[0].flat_outer_dims<float,3>();

	for(size_t i = 0; i < num_detections(0);++i)
    {
	    if(scores(i) > 0.7)
	    {
	  	    LOG(ERROR) << i << ",score:" << scores(i) << ",class:" << classes(i)<< ",box:" << "," << boxes(0,i,0) << "," << boxes(0,i,1) << "," << boxes(0,i,2)<< "," << boxes(0,i,3);
	    }
    }

	
}