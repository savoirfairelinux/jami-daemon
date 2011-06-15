/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include "video_rtp_session.h"
#include <string>
#include <iostream>
#include <cassert>
#include <sstream>
#include <fstream>
#include <map>
#include <signal.h>

extern "C" {
#define __STDC_CONSTANT_MACROS
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
}

namespace sfl_video {

class VideoRtpThread {
    private:
        static void print_error(const char *filename, int err)
        {
            char errbuf[128];
            const char *errbuf_ptr = errbuf;

            if (av_strerror(err, errbuf, sizeof(errbuf)) < 0)
                errbuf_ptr = strerror(AVUNERROR(err));
            std::cerr << filename << ":" <<  errbuf_ptr << std::endl;
        }

        static void print_and_save_sdp(AVFormatContext **avc)
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

        static volatile int interrupted_;

    public:

        static int run(std::map<std::string, std::string> args)
        {
            av_register_all();
            avdevice_register_all();
            AVFormatContext *ic;

            AVInputFormat *file_iformat = NULL;
            // it's a v4l device if starting with /dev/video
            if (args["input"].substr(0, strlen("/dev/video")) == "/dev/video") {
                std::cout << "Using v4l2 format" << std::endl;
                file_iformat = av_find_input_format("video4linux2");
                if (!file_iformat) {
                    std::cerr << "Could not find format!" << std::endl;
                    return 1;
                }
            }

            // Open video file
            if (av_open_input_file(&ic, args["input"].c_str(), file_iformat, 0, NULL) != 0) {
                std::cerr <<  "Could not open input file " << args["input"] <<
                    std::endl;
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

            AVOutputFormat *file_oformat = av_guess_format("rtp",
                                                           args["destination"].c_str(), NULL);
            if (!file_oformat) {
                std::cerr << "Unable to find a suitable output format for " <<
                    args["destination"] << std::endl;
                exit(EXIT_FAILURE);
            }
            oc->oformat = file_oformat;
            strncpy(oc->filename, args["destination"].c_str(), sizeof(oc->filename));

            AVCodec *encoder = NULL;
            const char *vcodec_name = args["codec"].c_str();

            AVCodecContext *encoderCtx;
            /* find the video encoder */
            encoder = avcodec_find_encoder_by_name(vcodec_name);
            if (!encoder) {
                std::cerr << "encoder not found" << std::endl;
                exit(EXIT_FAILURE);
            }

            encoderCtx = avcodec_alloc_context();

            /* set some encoder settings here */
            encoderCtx->bit_rate = args.size() > 2 ? atoi(args["bitrate"].c_str()) : 1000000;
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

            while (!interrupted_ && av_read_frame(ic, &inpacket) >= 0) {
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

            return 0;
        }
};
volatile int VideoRtpThread::interrupted_ = 0;


VideoRtpSession::VideoRtpSession(const std::string &input,
        const std::string &codec,
        int bitrate,
        const std::string &destinationURI) :
    input_(input), codec_(codec), bitrate_(bitrate),
    destinationURI_(destinationURI)
{}

void VideoRtpSession::start()
{
    std::cout << "Capturing from " << input_ << ", encoding to " << codec_ <<
        " at " << bitrate_ << " bps, sending to " << destinationURI_ <<
        std::endl;
    VideoRtpThread th;
    std::map<std::string, std::string> args;
    args["input"] = input_;
    args["codec"] = codec_;
    std::stringstream bitstr;
    bitstr << bitrate_;

    args["bitrate"] = bitstr.str();
    args["destination"] = destinationURI_;

    th.run(args);
}

void VideoRtpSession::stop()
{
    std::cout << "Stopping video rtp session " << std::endl;
}

} // end namspace sfl_video
