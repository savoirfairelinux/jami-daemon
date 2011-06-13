#include <cassert>
#include <sstream>
#include <iostream>
#include <fstream>
#include <signal.h>

extern "C" {
#define __STDC_CONSTANT_MACROS
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
}

static volatile int interrupted = 0;

void signal_handler(int sig) { (void)sig; interrupted = 1; }
void attach_signal_handlers() { signal(SIGINT, signal_handler); }

void print_error(const char *filename, int err)
{
    char errbuf[128];
    const char *errbuf_ptr = errbuf;

    if (av_strerror(err, errbuf, sizeof(errbuf)) < 0)
        errbuf_ptr = strerror(AVUNERROR(err));
    std::cerr << filename << ":" <<  errbuf_ptr << std::endl;
}

void print_and_save_sdp(AVFormatContext **avc)
{
    size_t sdp_size = avc[0]->streams[0]->codec->extradata_size + 2048;
    char *sdp = reinterpret_cast<char*>(malloc(sdp_size)); /* theora sdp can be huge */
    std::cout << "sdp_size: " << sdp_size << std::endl;
    av_sdp_create(avc, 1, sdp, sdp_size);
    std::ofstream sdp_file("test.sdp");
    std::istringstream iss(sdp);
    std::string line;
    while (std::getline(iss, line))
    {
        /* strip windows line ending */
        sdp_file << line.substr(0, line.length() - 1) << std::endl;
        std::cout << line << std::endl;
    }
    sdp_file << std::endl;
    sdp_file.close();
    free(sdp);
}

int main(int argc, char *argv[])
{
    attach_signal_handlers();
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] <<  " <filename> <codec>" << std::endl;
        return 1;
    }

    av_register_all();
    avdevice_register_all();
    AVFormatContext *ic;

    AVInputFormat *file_iformat = NULL;
    // it's a v4l device if starting with /dev/video
    if (strncmp(argv[1], "/dev/video", strlen("/dev/video")) == 0) {
        std::cout << "Using v4l2 format" << std::endl;
        file_iformat = av_find_input_format("video4linux2");
        if (!file_iformat) {
            std::cerr << "Could not find format!" << std::endl;
            return 1;
        }
    }

    // Open video file
    if (av_open_input_file(&ic, argv[1], file_iformat, 0, NULL) != 0) {
        std::cerr <<  "Could not open input file!" << std::endl;
        return 1; // couldn't open file
    }

    // retrieve stream information
    if (av_find_stream_info(ic) < 0) {
        std::cerr << "Could not find stream info!" << std::endl;
        return 1; // couldn't find stream info
    }

    AVCodecContext *inputDecoderCtx;

    // find the first video stream from the input
    int videoStream = -1;
    unsigned i;
    for (i = 0; i < ic->nb_streams; i++) {
        if (ic->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStream = i;
            break;
        }
    }
    if (videoStream == -1) {
        std::cerr << "Could not find video stream!" << std::endl;
        return 1; // didn't find a video stream
    }

    // Get a pointer to the codec context for the video stream
    inputDecoderCtx = ic->streams[videoStream]->codec;

    // find the decoder for the video stream
    AVCodec *inputDecoder = avcodec_find_decoder(inputDecoderCtx->codec_id);
    if (inputDecoder == NULL) {
        std::cerr << "Unsupported codec!" << std::endl;
        return 1; // codec not found
    }

    // open codec
    if (avcodec_open(inputDecoderCtx, inputDecoder) < 0) {
        std::cerr << "Could not open codec!" << std::endl;
        return 1; // could not open codec
    }

    AVFormatContext *oc = avformat_alloc_context();
    const char *DEST = "rtp://127.0.0.1:5000";

    AVOutputFormat *file_oformat = av_guess_format("rtp", DEST, NULL);
    if (!file_oformat) {
        std::cerr << "Unable to find a suitable output format for " << DEST
                << std::endl;
        exit(EXIT_FAILURE);
    }
    oc->oformat = file_oformat;
    strncpy(oc->filename, DEST, sizeof(oc->filename));

    AVCodec *encoder = NULL;
    const char *vcodec_name = argc > 2 ? argv[2] : "mpeg4";

    AVCodecContext *encoderCtx;
    /* find the video encoder */
    encoder = avcodec_find_encoder_by_name(vcodec_name);
    if (!encoder) {
        std::cerr << "encoder not found" << std::endl;
        exit(EXIT_FAILURE);
    }

    encoderCtx = avcodec_alloc_context();

    /* set some encoder settings here */
    encoderCtx->bit_rate = argc > 3 ? atoi(argv[3]) : 1000000;
    /* emit one intra frame every gop_size frames */
    encoderCtx->gop_size = 15;
    encoderCtx->max_b_frames = 0;
    encoderCtx->rtp_payload_size = 0; // Target GOB length
    /* resolution must be a multiple of two */
    encoderCtx->width = inputDecoderCtx->width; // get resolution from input
    encoderCtx->height = inputDecoderCtx->height;
    /* fps */
    encoderCtx->time_base = (AVRational){1, 30};
    encoderCtx->pix_fmt = PIX_FMT_YUV420P;

    /* let x264 preset override our encoder settings */
    if (!strcmp(vcodec_name, "libx264")) {
        std::cout << "get x264 preset time" << std::endl;
        //int opt_name_count = 0;
        FILE *f = NULL;
        // FIXME: hardcoded! should look for FFMPEG_DATADIR
        const char* preset_filename = "libx264-ultrafast.ffpreset";
        /* open preset file for libx264 */
        f = fopen(preset_filename, "r");
        if (!f) {
            std::cerr << "File for preset ' " << preset_filename << "' not"
                "found" << std::endl;
            exit(EXIT_FAILURE);
        }

        /* grab preset file and put it in character buffer */
        fseek(f, 0, SEEK_END);
        long pos = ftell(f);
        fseek(f, 0, SEEK_SET);

        char *encoder_options_string = reinterpret_cast<char*>(malloc(pos + 1));
        fread(encoder_options_string, pos, 1, f);
        fclose(f);

        std::cout << "Encoder options: " << encoder_options_string << std::endl;
        av_set_options_string(encoderCtx, encoder_options_string, "=", "\n");
        free(encoder_options_string); // free allocated memory
    }

    AVFrame *scaled_picture = avcodec_alloc_frame();

    /* open encoder */
    if (avcodec_open(encoderCtx, encoder) < 0) {
        std::cerr << "Could not open encoder" << std::endl;
        exit(EXIT_FAILURE);
    }

    /* add video stream to outputformat context */
    AVStream *video_st = av_new_stream(oc, 0);
    if (!video_st) {
        std::cerr << "Could not alloc stream" << std::endl;
        exit(EXIT_FAILURE);
    }
    video_st->codec = encoderCtx;

    /* set the output parameters (must be done even if no
       parameters). */
    if (av_set_parameters(oc, NULL) < 0) {
        std::cerr << "Invalid output format parameters" << std::endl;
        exit(EXIT_FAILURE);
    }

    /* open the output file, if needed */
    if (!(file_oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&oc->pb, oc->filename, AVIO_FLAG_WRITE) < 0) {
            std::cerr << "Could not open '" << oc->filename << "'" <<
                std::endl;
            exit(EXIT_FAILURE);
        }
    }
    else std::cerr << "No need to open" << std::endl;

    av_dump_format(oc, 0, oc->filename, 1);
    print_and_save_sdp(&oc);

    char error[1024];
    /* write the stream header, if any */
    if (av_write_header(oc) < 0) {
        snprintf(error, sizeof(error), "Could not write header for output file (incorrect codec parameters ?)");
        return AVERROR(EINVAL);
    }

    /* alloc image and output buffer */
    int size = encoderCtx->width * encoderCtx->height;
    int outbuf_size = size;
    uint8_t *outbuf = reinterpret_cast<uint8_t*>(av_malloc(outbuf_size));
    uint8_t *scaled_picture_buf = reinterpret_cast<uint8_t*>(av_malloc((size * 3) / 2)); /* size for YUV 420 */

    scaled_picture->data[0] = scaled_picture_buf;
    scaled_picture->data[1] = scaled_picture->data[0] + size;
    scaled_picture->data[2] = scaled_picture->data[1] + size / 4;
    scaled_picture->linesize[0] = encoderCtx->width;
    scaled_picture->linesize[1] = encoderCtx->width / 2;
    scaled_picture->linesize[2] = encoderCtx->width / 2;

    // allocate video frame
    AVFrame *raw_frame = avcodec_alloc_frame();

    int frameFinished;
    AVPacket inpacket;
    double frame_number = 0;

    /* Create scaling context */
    struct SwsContext *img_convert_ctx = sws_getContext(inputDecoderCtx->width,
            inputDecoderCtx->height, inputDecoderCtx->pix_fmt, encoderCtx->width,
            encoderCtx->height, encoderCtx->pix_fmt, SWS_BICUBIC,
            NULL, NULL, NULL);
    if (img_convert_ctx == NULL) {
        std::cerr << "Cannot init the conversion context!" << std::endl;
        exit(EXIT_FAILURE);
    }

    while (!interrupted && av_read_frame(ic, &inpacket) >= 0) {
        // is this a packet from the video stream?
        if (inpacket.stream_index == videoStream) {
            // decode video frame from camera
            avcodec_decode_video2(inputDecoderCtx, raw_frame, &frameFinished, &inpacket);
            if (frameFinished)  {
                sws_scale(img_convert_ctx, raw_frame->data, raw_frame->linesize,
                        0, inputDecoderCtx->height, scaled_picture->data,
                        scaled_picture->linesize);

                /* Set presentation timestamp on our scaled frame before encoding it */
                scaled_picture->pts = frame_number;
                frame_number++;

                int out_size = avcodec_encode_video(encoderCtx,
                        outbuf, outbuf_size,
                        scaled_picture);

                if (out_size > 0) {
                    AVPacket opkt;
                    av_init_packet(&opkt);

                    opkt.data = outbuf;
                    opkt.size = out_size;

                    if (static_cast<unsigned>(encoderCtx->coded_frame->pts) !=
                        AV_NOPTS_VALUE)
                        opkt.pts = av_rescale_q(encoderCtx->coded_frame->pts,
                                encoderCtx->time_base, video_st->time_base);
                    else
                        opkt.pts = 0;

                    if (encoderCtx->coded_frame->key_frame)
                        opkt.flags |= AV_PKT_FLAG_KEY;
                    opkt.stream_index = video_st->index;

                    /* write the compressed frame in the media file */
                    int ret = av_interleaved_write_frame(oc, &opkt);
                    if (ret < 0) {
                        print_error("av_interleaved_write_frame() error", ret);
                        exit(EXIT_FAILURE);
                    }
                    av_free_packet(&opkt);
                }
            }
        }
        // free the packet that was allocated by av_read_frame
        av_free_packet(&inpacket);
    }
    av_free(scaled_picture_buf);
    av_free(outbuf);

    /* write the trailer, if any.  the trailer must be written
     * before you close the CodecContexts open when you wrote the
     * header; otherwise write_trailer may try to use memory that
     * was freed on av_codec_close() */
    av_write_trailer(oc);

    // free the scaled frame
    av_free(scaled_picture);
    // free the YUV frame
    av_free(raw_frame);

    // close the codecs
    avcodec_close(encoderCtx);

    /* doesn't need to be freed, we didn't use avcodec_alloc_context */
    avcodec_close(inputDecoderCtx);

    // close the video file
    av_close_input_file(ic);

    std::cout << "Exitting..." << std::endl;
    return 0;
}
