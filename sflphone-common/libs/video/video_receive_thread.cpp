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

#include "video_receive_thread.h"

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


void VideoReceiveThread::setup()
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

    AVDictionary *options = NULL;
    if (!args_["framerate"].empty())
        av_dict_set(&options, "framerate", args_["framerate"].c_str(), 0);
    if (!args_["video_size"].empty())
        av_dict_set(&options, "video_size", args_["video_size"].c_str(), 0);
    if (!args_["channel"].empty())
        av_dict_set(&options, "channel", args_["channel"].c_str(), 0);

    // Open video file
    if (avformat_open_input(&inputCtx_, args_["input"].c_str(), file_iformat, &options) != 0)
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
    shmReady_.signal();

    // assign appropriate parts of buffer to image planes in scaledPicture 
    avpicture_fill(reinterpret_cast<AVPicture *>(scaledPicture_),
            reinterpret_cast<uint8_t*>(shmBuffer_),
            PIX_FMT_RGB24, decoderCtx_->width, decoderCtx_->height);

    // allocate video frame
    rawFrame_ = avcodec_alloc_frame();
}

// NOT called from this (the run() ) thread
void VideoReceiveThread::waitForShm()
{
    shmReady_.wait();
}

void VideoReceiveThread::cleanup()
{
    // free shared memory
    cleanupSemaphore(semSetID_);
    cleanupShm(shmID_, shmBuffer_);

    // free the scaled frame
    if (scaledPicture_)
        av_free(scaledPicture_);
    // free the YUV frame
    if (rawFrame_)
        av_free(rawFrame_);

    // doesn't need to be freed, we didn't use avcodec_alloc_context
    if (decoderCtx_)
        avcodec_close(decoderCtx_);

    // close the video file
    if (inputCtx_)
        av_close_input_file(inputCtx_);

    std::cerr << "Exitting the decoder thread" << std::endl;
    // exit this thread
    exit();
}

SwsContext * VideoReceiveThread::createScalingContext()
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

VideoReceiveThread::VideoReceiveThread(const std::map<std::string, std::string> &args) : args_(args),
    interrupted_(false),
    scaledPictureBuf_(0),
    shmBuffer_(0),
    shmID_(-1),
    semSetID_(-1),
    decoderCtx_(0),
    rawFrame_(0),
    scaledPicture_(0),
    videoStreamIndex_(-1),
    inputCtx_(0)
    {}

void VideoReceiveThread::run()
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

void VideoReceiveThread::stop()
{
    // FIXME: not thread safe, add mutex
    interrupted_ = true;
}

VideoReceiveThread::~VideoReceiveThread()
{
    terminate();
}

} // end namespace sfl_video

