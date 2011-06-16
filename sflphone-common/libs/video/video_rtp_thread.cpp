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

#define __STDC_CONSTANT_MACROS

#include "video_rtp_thread.h"

// libav includes
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
}

// shm includes
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>     /* semaphore functions and structs.    */
#include <sys/shm.h>

namespace sfl_video {

namespace { // anonymouse namespace

#if _SEM_SEMUN_UNDEFINED
union semun
{
 int val;				    /* value for SETVAL */
 struct semid_ds *buf;		/* buffer for IPC_STAT & IPC_SET */
 unsigned short int *array;	/* array for GETALL & SETALL */
 struct seminfo *__buf;		/* buffer for IPC_INFO */
};
#endif

typedef struct {
    unsigned size;
    unsigned width;
    unsigned height;
} FrameInfo;

#define TEMPFILE "/tmp/frame.txt"

void postFrameSize(unsigned width, unsigned height, unsigned numBytes)
{
    FILE *tmp = fopen(TEMPFILE, "w");

    /* write to file*/
    fprintf(tmp, "%u\n", numBytes);
    fprintf(tmp, "%u\n", width);
    fprintf(tmp, "%u\n", height);
    fclose(tmp);
}

int createSemSet()
{
    /* this variable will contain the semaphore set. */
    int sem_set_id;
    key_t key = ftok("/tmp", 'b');

    /* semaphore value, for semctl().                */
    union semun sem_val;

    /* first we create a semaphore set with a single semaphore, */
    /* whose counter is initialized to '0'.                     */
    sem_set_id = semget(key, 1, 0600 | IPC_CREAT);
    if (sem_set_id == -1) {
        perror("semget");
        exit(1);
    }
    sem_val.val = 0;
    semctl(sem_set_id, 0, SETVAL, sem_val);
    return sem_set_id;
}

void
cleanupSemaphore(int sem_set_id)
{
    semctl(sem_set_id, 0, IPC_RMID);
}


/*
 * function: sem_signal. signals the process that a frame is ready.
 * input:    semaphore set ID.
 * output:   none.
 */
void
sem_signal(int sem_set_id)
{
    /* structure for semaphore operations.   */
    struct sembuf sem_op;

    /* signal the semaphore - increase its value by one. */
    sem_op.sem_num = 0;
    sem_op.sem_op = 1;
    sem_op.sem_flg = 0;
    semop(sem_set_id, &sem_op, 1);
}

/* join and/or create a shared memory segment */
int createShm(unsigned numBytes)
{
    key_t key;
    int shm_id;
    /* connect to and possibly create a segment with 644 permissions
       (rw-r--r--) */
    key = ftok("/tmp", 'c');
    shm_id = shmget(key, numBytes, 0644 | IPC_CREAT);

    return shm_id;
}

/* attach a shared memory segment */
uint8_t *attachShm(int shm_id)
{
    uint8_t *data = NULL;

    /* attach to the segment and get a pointer to it */
    data = reinterpret_cast<uint8_t*>(shmat(shm_id, (void *)0, 0));
    if (data == (uint8_t *)(-1)) {
        perror("shmat");
        data = NULL;
    }

    return data;
}

void detachShm(uint8_t *data)
{
    /* detach from the segment: */
    if (shmdt(data) == -1) {
        perror("shmdt");
    }
}

void destroyShm(int shm_id)
{
    /* destroy it */
    shmctl(shm_id, IPC_RMID, NULL);
}

void cleanupShm(int shm_id, uint8_t *data)
{
    detachShm(data);
    destroyShm(shm_id);
}

int bufferSizeRGB24(int width, int height)
{
    int numBytes;
    // determine required buffer size and allocate buffer
    numBytes = avpicture_get_size(PIX_FMT_RGB24, width, height);
    return numBytes * sizeof(uint8_t);
}

} // end anonymous namespace


void VideoRtpReceiveThread::setup()
{
    av_register_all();
    avdevice_register_all();

    AVInputFormat *file_iformat = 0;

    // Open video file
    if (av_open_input_file(&inputCtx_, args_["input"].c_str(), file_iformat, 0, NULL) != 0)
    {
        std::cerr <<  "Could not open input file " << args_["input"] <<
            std::endl;
        cleanup();
    }

    // retrieve stream information
    if (av_find_stream_info(inputCtx_) < 0)
    {
        std::cerr << "Could not find stream info!" << std::endl;
        cleanup();
    }

    // find the first video stream from the input
    unsigned i;
    for (i = 0; i < inputCtx_->nb_streams; i++)
    {
        if (inputCtx_->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoStreamIndex_ = i;
            break;
        }
    }
    if (videoStreamIndex_ == -1)
    {
        std::cerr << "Could not find video stream!" << std::endl;
        cleanup();
    }

    // Get a pointer to the codec context for the video stream
    decoderCtx_ = inputCtx_->streams[videoStreamIndex_]->codec;

    // find the decoder for the video stream
    AVCodec *inputDecoder = avcodec_find_decoder(decoderCtx_->codec_id);
    if (inputDecoder == NULL)
    {
        std::cerr << "Unsupported codec!" << std::endl;
        cleanup();
    }

    // open codec
    if (avcodec_open(decoderCtx_, inputDecoder) < 0)
    {
        std::cerr << "Could not open codec!" << std::endl;
        cleanup();
    }

    scaledPicture_ = avcodec_alloc_frame();
    if (scaledPicture_ == 0)
    {
        std::cerr << "Could not allocated output frame!" << std::endl;
        cleanup();
    }

    unsigned numBytes;
    // determine required buffer size and allocate buffer
    numBytes = bufferSizeRGB24(decoderCtx_->width, decoderCtx_->height);
    /*printf("%u bytes\n", numBytes);*/
    postFrameSize(decoderCtx_->width, decoderCtx_->height, numBytes);

    // create shared memory segment and attach to it
    shmID_ = createShm(numBytes);
    shmBuffer_  = attachShm(shmID_);
    semSetID_ = createSemSet();

    // assign appropriate parts of buffer to image planes in scaledPicture 
    avpicture_fill(reinterpret_cast<AVPicture *>(scaledPicture_),
            reinterpret_cast<uint8_t*>(shmBuffer_),
            PIX_FMT_RGB24, decoderCtx_->width, decoderCtx_->height);

    // allocate video frame
    rawFrame_ = avcodec_alloc_frame();
}

void VideoRtpReceiveThread::cleanup()
{
    // free shared memory
    cleanupSemaphore(semSetID_);
    cleanupShm(shmID_, shmBuffer_);

    // free the scaled frame
    av_free(scaledPicture_);
    // free the YUV frame
    av_free(rawFrame_);

    // doesn't need to be freed, we didn't use avcodec_alloc_context
    avcodec_close(decoderCtx_);

    // close the video file
    av_close_input_file(inputCtx_);

    std::cerr << "Exitting the decoder thread" << std::endl;
    // exit this thread
    exit();
}

SwsContext * VideoRtpReceiveThread::createScalingContext()
{
    // Create scaling context, no scaling done here
    SwsContext *imgConvertCtx = sws_getContext(decoderCtx_->width,
            decoderCtx_->height, decoderCtx_->pix_fmt, decoderCtx_->width,
            decoderCtx_->height, PIX_FMT_RGB24, SWS_BICUBIC,
            NULL, NULL, NULL);
    if (imgConvertCtx == 0)
    {
        std::cerr << "Cannot init the conversion context!" << std::endl;
        cleanup();
    }
    return imgConvertCtx;
}

VideoRtpReceiveThread::VideoRtpReceiveThread(const std::map<std::string, std::string> &args) : args_(args),
    interrupted_(false) {}

void VideoRtpReceiveThread::run()
{
    setup();
    AVPacket inpacket;
    int frameFinished;
    SwsContext *imgConvertCtx = createScalingContext();

    while (not interrupted_ and av_read_frame(inputCtx_, &inpacket) >= 0)
    {
        // is this a packet from the video stream?
        if (inpacket.stream_index == videoStreamIndex_)
        {
            // decode video frame from camera
            avcodec_decode_video2(decoderCtx_, rawFrame_, &frameFinished, &inpacket);
            if (frameFinished)
            {
                sws_scale(imgConvertCtx, rawFrame_->data, rawFrame_->linesize,
                        0, decoderCtx_->height, scaledPicture_->data,
                        scaledPicture_->linesize);

                /* signal the semaphore that a new frame is ready */ 
                sem_signal(semSetID_);
            }
        }
        // free the packet that was allocated by av_read_frame
        av_free_packet(&inpacket);
    }
    // free resources, exit thread
    cleanup();
}

void VideoRtpReceiveThread::stop()
{
    // FIXME: not thread safe, add mutex
    interrupted_ = true;
}

VideoRtpReceiveThread::~VideoRtpReceiveThread()
{
    terminate();
}

void VideoRtpSendThread::print_error(const char *filename, int err)
{
    char errbuf[128];
    const char *errbuf_ptr = errbuf;

    if (av_strerror(err, errbuf, sizeof(errbuf)) < 0)
        errbuf_ptr = strerror(AVUNERROR(err));
    std::cerr << filename << ":" <<  errbuf_ptr << std::endl;
}

void VideoRtpSendThread::print_and_save_sdp()
{
    size_t sdp_size = outputCtx_->streams[0]->codec->extradata_size + 2048;
    char *sdp = reinterpret_cast<char*>(malloc(sdp_size)); /* theora sdp can be huge */
    std::cerr << "sdp_size: " << sdp_size << std::endl;
    av_sdp_create(&outputCtx_, 1, sdp, sdp_size);
    std::ofstream sdp_file("test.sdp");
    std::istringstream iss(sdp);
    std::string line;
    while (std::getline(iss, line))
    {
        /* strip windows line ending */
        sdp_file << line.substr(0, line.length() - 1) << std::endl;
        std::cerr << line << std::endl;
    }
    sdp_file << std::endl;
    sdp_file.close();
    free(sdp);
    sdpReady_.signal();
}

// NOT called from this (the run() ) thread
void VideoRtpSendThread::waitForSDP()
{
    sdpReady_.wait();
}

void VideoRtpSendThread::forcePresetX264()
{
    std::cerr << "get x264 preset time" << std::endl;
    //int opt_name_count = 0;
    FILE *f = 0;
    // FIXME: hardcoded! should look for FFMPEG_DATADIR
    const char* preset_filename = "libx264-ultrafast.ffpreset";
    /* open preset file for libx264 */
    f = fopen(preset_filename, "r");
    if (f == 0)
    {
        std::cerr << "File for preset ' " << preset_filename << "' not"
            "found" << std::endl;
        cleanup();
    }

    /* grab preset file and put it in character buffer */
    fseek(f, 0, SEEK_END);
    long pos = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *encoder_options_string = reinterpret_cast<char*>(malloc(pos + 1));
    fread(encoder_options_string, pos, 1, f);
    encoder_options_string[pos] = '\0';
    fclose(f);

    std::cerr << "Encoder options: " << encoder_options_string << std::endl;
    av_set_options_string(encoderCtx_, encoder_options_string, "=", "\n");
    free(encoder_options_string); // free allocated memory
}

void VideoRtpSendThread::prepareEncoderContext()
{
    encoderCtx_ = avcodec_alloc_context();
    // set some encoder settings here
    encoderCtx_->bit_rate = atoi(args_["bitrate"].c_str());
    // emit one intra frame every gop_size frames
    encoderCtx_->gop_size = 15;
    encoderCtx_->max_b_frames = 0;
    encoderCtx_->rtp_payload_size = 0; // Target GOB length
    // resolution must be a multiple of two
    encoderCtx_->width = inputDecoderCtx_->width; // get resolution from input
    encoderCtx_->height = inputDecoderCtx_->height;
    // fps
    encoderCtx_->time_base = (AVRational){1, 30};
    encoderCtx_->pix_fmt = PIX_FMT_YUV420P;
}

void VideoRtpSendThread::setup()
{
    av_register_all();
    avdevice_register_all();

    AVInputFormat *file_iformat = 0;
    // it's a v4l device if starting with /dev/video
    if (args_["input"].substr(0, strlen("/dev/video")) == "/dev/video")
    {
        std::cerr << "Using v4l2 format" << std::endl;
        file_iformat = av_find_input_format("video4linux2");
        if (!file_iformat)
        {
            std::cerr << "Could not find format!" << std::endl;
            cleanup();
        }
    }

    // Open video file
    if (av_open_input_file(&inputCtx_, args_["input"].c_str(), file_iformat, 0, NULL) != 0)
    {
        std::cerr <<  "Could not open input file " << args_["input"] <<
            std::endl;
        cleanup();
    }

    // retrieve stream information
    if (av_find_stream_info(inputCtx_) < 0) {
        std::cerr << "Could not find stream info!" << std::endl;
        cleanup();
    }

    // find the first video stream from the input
    unsigned i;
    for (i = 0; i < inputCtx_->nb_streams; i++)
    {
        if (inputCtx_->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoStreamIndex_ = i;
            break;
        }
    }
    if (videoStreamIndex_ == -1)
    {
        std::cerr << "Could not find video stream!" << std::endl;
        cleanup();
    }

    // Get a pointer to the codec context for the video stream
    inputDecoderCtx_ = inputCtx_->streams[videoStreamIndex_]->codec;

    // find the decoder for the video stream
    AVCodec *inputDecoder = avcodec_find_decoder(inputDecoderCtx_->codec_id);
    if (inputDecoder == NULL)
    {
        std::cerr << "Unsupported codec!" << std::endl;
        cleanup();
    }

    // open codec
    if (avcodec_open(inputDecoderCtx_, inputDecoder) < 0)
    {
        std::cerr << "Could not open codec!" << std::endl;
        cleanup();
    }

    outputCtx_ = avformat_alloc_context();

    AVOutputFormat *file_oformat = av_guess_format("rtp", args_["destination"].c_str(), NULL);
    if (!file_oformat)
    {
        std::cerr << "Unable to find a suitable output format for " <<
            args_["destination"] << std::endl;
        cleanup();
    }
    outputCtx_->oformat = file_oformat;
    strncpy(outputCtx_->filename, args_["destination"].c_str(),
            sizeof(outputCtx_->filename));

    AVCodec *encoder = 0;
    /* find the video encoder */
    encoder = avcodec_find_encoder_by_name(args_["codec"].c_str());
    if (encoder == 0)
    {
        std::cerr << "encoder not found" << std::endl;
        cleanup();
    }

    prepareEncoderContext();

    /* let x264 preset override our encoder settings */
    if (args_["codec"] == "libx264")
        forcePresetX264();

    scaledPicture_ = avcodec_alloc_frame();

    // open encoder
    if (avcodec_open(encoderCtx_, encoder) < 0)
    {
        std::cerr << "Could not open encoder" << std::endl;
        cleanup();
    }

    // add video stream to outputformat context
    videoStream_ = av_new_stream(outputCtx_, 0);
    if (videoStream_ == 0)
    {
        std::cerr << "Could not alloc stream" << std::endl;
        cleanup();
    }
    videoStream_->codec = encoderCtx_;

    // set the output parameters (must be done even if no
    //   parameters).
    if (av_set_parameters(outputCtx_, NULL) < 0)
    {
        std::cerr << "Invalid output format parameters" << std::endl;
        cleanup();
    }

    // open the output file, if needed
    if (!(file_oformat->flags & AVFMT_NOFILE))
    {
        if (avio_open(&outputCtx_->pb, outputCtx_->filename, AVIO_FLAG_WRITE) < 0)
        {
            std::cerr << "Could not open '" << outputCtx_->filename << "'" <<
                std::endl;
            cleanup();
        }
    }
    else
        std::cerr << "No need to open" << std::endl;

    av_dump_format(outputCtx_, 0, outputCtx_->filename, 1);
    print_and_save_sdp();

    // write the stream header, if any
    if (av_write_header(outputCtx_) < 0)
    {
        std::cerr << "Could not write header for output file (incorrect codec "
            << "parameters ?)" << std::endl;
        cleanup();
    }

    // allocate video frame
    rawFrame_ = avcodec_alloc_frame();

    // alloc image and output buffer
    outbufSize_ = encoderCtx_->width * encoderCtx_->height;
    outbuf_ = reinterpret_cast<uint8_t*>(av_malloc(outbufSize_));
    // allocate buffer that fits YUV 420
    scaledPictureBuf_ = reinterpret_cast<uint8_t*>(av_malloc((outbufSize_ * 3) / 2));

    scaledPicture_->data[0] = reinterpret_cast<uint8_t*>(scaledPictureBuf_);
    scaledPicture_->data[1] = scaledPicture_->data[0] + outbufSize_;
    scaledPicture_->data[2] = scaledPicture_->data[1] + outbufSize_ / 4;
    scaledPicture_->linesize[0] = encoderCtx_->width;
    scaledPicture_->linesize[1] = encoderCtx_->width / 2;
    scaledPicture_->linesize[2] = encoderCtx_->width / 2;
}

void VideoRtpSendThread::cleanup()
{
    // write the trailer, if any.  the trailer must be written
    // before you close the CodecContexts open when you wrote the
    // header; otherwise write_trailer may try to use memory that
    // was freed on av_codec_close()
    av_write_trailer(outputCtx_);

    av_free(scaledPictureBuf_);
    av_free(outbuf_);

    // free the scaled frame
    av_free(scaledPicture_);
    // free the YUV frame
    av_free(rawFrame_);

    // close the codecs
    avcodec_close(encoderCtx_);

    // doesn't need to be freed, we didn't use avcodec_alloc_context
    avcodec_close(inputDecoderCtx_);

    // close the video file
    av_close_input_file(inputCtx_);

    // exit this thread
    exit();
}

SwsContext * VideoRtpSendThread::createScalingContext()
{
    // Create scaling context
    SwsContext *imgConvertCtx = sws_getContext(inputDecoderCtx_->width,
            inputDecoderCtx_->height, inputDecoderCtx_->pix_fmt, encoderCtx_->width,
            encoderCtx_->height, encoderCtx_->pix_fmt, SWS_BICUBIC,
            NULL, NULL, NULL);
    if (imgConvertCtx == 0)
    {
        std::cerr << "Cannot init the conversion context!" << std::endl;
        cleanup();
    }
    return imgConvertCtx;
}


VideoRtpSendThread::VideoRtpSendThread(const std::map<std::string, std::string> &args) :
    args_(args),
    interrupted_(false) {}

void VideoRtpSendThread::run()
{
    setup();
    AVPacket inpacket;
    int frameFinished;
    int64_t frameNumber = 0;
    SwsContext *imgConvertCtx = createScalingContext();

    while (not interrupted_ and av_read_frame(inputCtx_, &inpacket) >= 0)
    {
        // is this a packet from the video stream?
        if (inpacket.stream_index == videoStreamIndex_)
        {
            // decode video frame from camera
            avcodec_decode_video2(inputDecoderCtx_, rawFrame_, &frameFinished, &inpacket);
            if (frameFinished)
            {
                sws_scale(imgConvertCtx, rawFrame_->data, rawFrame_->linesize,
                        0, inputDecoderCtx_->height, scaledPicture_->data,
                        scaledPicture_->linesize);

                // Set presentation timestamp on our scaled frame before encoding it
                scaledPicture_->pts = frameNumber;
                frameNumber++;

                int encodedSize = avcodec_encode_video(encoderCtx_,
                        outbuf_, outbufSize_, scaledPicture_);

                if (encodedSize > 0) {
                    AVPacket opkt;
                    av_init_packet(&opkt);

                    opkt.data = outbuf_;
                    opkt.size = encodedSize;

                    // rescale pts from encoded video framerate to rtp
                    // clock rate
                    if (static_cast<unsigned>(encoderCtx_->coded_frame->pts) !=
                            AV_NOPTS_VALUE)
                        opkt.pts = av_rescale_q(encoderCtx_->coded_frame->pts,
                                encoderCtx_->time_base, videoStream_->time_base);
                    else
                        opkt.pts = 0;

                    // is it a key frame?
                    if (encoderCtx_->coded_frame->key_frame)
                        opkt.flags |= AV_PKT_FLAG_KEY;
                    opkt.stream_index = videoStream_->index;

                    // write the compressed frame in the media file
                    int ret = av_interleaved_write_frame(outputCtx_, &opkt);
                    if (ret < 0)
                    {
                        print_error("av_interleaved_write_frame() error", ret);
                        cleanup();
                    }
                    av_free_packet(&opkt);
                }
            }
        }
        // free the packet that was allocated by av_read_frame
        av_free_packet(&inpacket);
    }
    // free resources, exit thread
    cleanup();
}

void VideoRtpSendThread::stop()
{
    // FIXME: not thread safe
    interrupted_ = true;
}

VideoRtpSendThread::~VideoRtpSendThread()
{
    terminate();
}

} // end namespace sfl_video
