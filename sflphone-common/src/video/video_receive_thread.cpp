/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Copyright © 2008 Rémi Denis-Courmont
 *
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
#include <libavutil/pixdesc.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
}

// shm includes
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>     /* semaphore functions and structs.    */
#include <sys/shm.h>

#include <time.h>
#include <cstdlib>

#include "manager.h"
#include "video_picture.h"

namespace sfl_video {

namespace { // anonymouse namespace

static char program_path[PATH_MAX+1] = "";

#if _SEM_SEMUN_UNDEFINED
union semun
{
 int val;				    /* value for SETVAL */
 struct semid_ds *buf;		/* buffer for IPC_STAT & IPC_SET */
 unsigned short int *array;	/* array for GETALL & SETALL */
 struct seminfo *__buf;		/* buffer for IPC_INFO */
};
#endif

int createSemSet(int shmKey, int *semKey)
{
    /* this variable will contain the semaphore set. */
    int sem_set_id;
    /* semaphore value, for semctl().                */
    union semun sem_val;
    key_t key;

    do
		key = ftok(program_path, rand());
    while(key == shmKey);

    *semKey = key;

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
int createShm(unsigned numBytes, int *shmKey)
{
    key_t key;
    int shm_id;
    /* connect to and possibly create a segment with 644 permissions
       (rw-r--r--) */

    srand(time(NULL));
    int proj_id = rand();
    key = ftok(program_path, proj_id);
    *shmKey = key;
    shm_id = shmget(key, numBytes, 0644 | IPC_CREAT);

    if (shm_id == -1)
        perror("shmget");

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

int bufferSize(int width, int height, int format)
{
	enum PixelFormat fmt = (enum PixelFormat) format;
    // determine required buffer size and allocate buffer
    return sizeof(uint8_t) * avpicture_get_size(fmt, width, height);
}


std::string openTemp(std::string path, std::ofstream& f)
{
    path += "/XXXXXX";
    std::vector<char> dst_path(path.begin(), path.end());
    dst_path.push_back('\0');

    int fd = -1;
    while (fd == -1)
    {
        fd = mkstemp(&dst_path[0]);
        if (fd != -1)
        {
            path.assign(dst_path.begin(), dst_path.end() - 1);
            f.open(path.c_str(),
                    std::ios_base::trunc | std::ios_base::out);
            close(fd);
        }
    }
    return path;
}

} // end anonymous namespace

void VideoReceiveThread::prepareSDP()
{
    // this memory will be released on next call to tmpnam
    std::ofstream os;
    sdpFilename_ = openTemp("/tmp", os);

    os << "v=0" << std::endl;
    os << "o=- 0 0 IN IP4 127.0.0.1" << std::endl;
    os << "s=No Name" << std::endl;
    os << "c=IN IP4 127.0.0.1" << std::endl;
    os << "t=0 0" << std::endl;
    os << "a=tool:libavformat 53.2.0" << std::endl;
    os << "m=video 5000 RTP/AVP 96" << std::endl;  
    os << "b=AS:1000" << std::endl;
    os << "a=rtpmap:96 MP4V-ES/90000" << std::endl;
    os << "a=rtpmap:96 H264/90000" << std::endl;
    os << "a=fmtp:96 packetization-mode=1; sprop-parameter-sets=Z0LAHtoCgPaEAAADAAQAAAMA8DxYuoA=,aM48gA==" << std::endl;

    os.close();
}

void VideoReceiveThread::setup()
{
    av_register_all();
    avdevice_register_all();
    setProgramPath();

    dstWidth_ = atoi(args_["width"].c_str());
    dstHeight_ = atoi(args_["height"].c_str());
    format_ = av_get_pix_fmt(args_["format"].c_str());
    if (format_ == -1)
    {
        std::cerr << "Couldn't find a pixel format for `" << args_["format"]
            << "'" << std::endl;
        cleanup();
    }

    AVInputFormat *file_iformat = 0;
    // it's a v4l device if starting with /dev/video
    // FIXME: This is not the most robust way of checking if we mean to use a
    // v4l device
    if (args_["input"].empty())
    {
        std::cerr << "Preparing SDP" << std::endl;
        prepareSDP();
        args_["input"] = sdpFilename_;
        file_iformat = av_find_input_format("sdp");
        if (!file_iformat)
        {
            std::cerr << "Could not find format!" << std::endl;
            cleanup();
        }
    }
    else if (args_["input"].substr(0, strlen("/dev/video")) == "/dev/video")
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
        std::cerr <<  "Could not open input file \"" << args_["input"] <<
            "\"" << std::endl;
        cleanup();
    }
    else
        std::cerr << "Opened input " << args_["input"] << std::endl;

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
    Manager::instance().avcodecLock();
    int ret = avcodec_open(decoderCtx_, inputDecoder);
    Manager::instance().avcodecUnlock();
    if (ret < 0)
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

    // determine required buffer size and allocate buffer
    videoBufferSize_ = bufferSize(dstWidth_, dstHeight_, format_);

    // create shared memory segment and attach to it
    shmID_ = createShm(videoBufferSize_, &shmKey_);
    shmBuffer_  = attachShm(shmID_);
    semSetID_ = createSemSet(shmKey_, &semKey_);
    shmReady_.signal();

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
    if (decoderCtx_) {
        Manager::instance().avcodecLock();
        avcodec_close(decoderCtx_);
        Manager::instance().avcodecUnlock();
    }

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
            decoderCtx_->height, decoderCtx_->pix_fmt, dstWidth_,
            dstHeight_, (enum PixelFormat) format_, SWS_BICUBIC,
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
    shmKey_(-1),
    semKey_(-1),
    decoderCtx_(0),
    rawFrame_(0),
    scaledPicture_(0),
    videoStreamIndex_(-1),
    inputCtx_(0),
    dstWidth_(-1),
    dstHeight_(-1)
{}

void VideoReceiveThread::run()
{
    setup();

    AVPacket inpacket;
    int frameFinished;
    SwsContext *imgConvertCtx = createScalingContext();
    enum PixelFormat fmt = (enum PixelFormat) format_;
    int bpp = (av_get_bits_per_pixel(&av_pix_fmt_descriptors[fmt]) + 7) & ~7;

    while (not interrupted_ and av_read_frame(inputCtx_, &inpacket) >= 0)
    {
        // is this a packet from the video stream?
        if (inpacket.stream_index == videoStreamIndex_)
        {
            // decode video frame from camera
            avcodec_decode_video2(decoderCtx_, rawFrame_, &frameFinished, &inpacket);
            if (frameFinished)
            {
                VideoPicture pic(bpp, dstWidth_, dstHeight_, rawFrame_->pts);
                // assign appropriate parts of buffer to image planes in scaledPicture
                avpicture_fill(reinterpret_cast<AVPicture *>(scaledPicture_),
                        reinterpret_cast<uint8_t*>(pic.data), fmt, dstWidth_, dstHeight_);

                sws_scale(imgConvertCtx, rawFrame_->data, rawFrame_->linesize,
                        0, decoderCtx_->height, scaledPicture_->data,
                        scaledPicture_->linesize);

                // FIXME : put pictures in a pool and use the PTS to get them out and displayed from a separate thread
                // i.e., picturePool_.push_back(pic);
                memcpy(shmBuffer_, pic.data, pic.Size());

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
    // FIXME: not thread safe
    interrupted_ = true;
}

VideoReceiveThread::~VideoReceiveThread()
{
    terminate();
}

void VideoReceiveThread::setProgramPath()
{
    if (*program_path)
        return;

    // fallback
    strcpy(program_path, "/tmp");

    char *line = NULL;
    size_t linelen = 0;
    uintptr_t needle = (uintptr_t)createSemSet;

    /* Find the path to sflphoned (i.e. ourselves) */
    FILE *maps = fopen ("/proc/self/maps", "rt");
    if (maps == NULL)
        return;

    for (;;)
    {
        ssize_t len = getline (&line, &linelen, maps);
        if (len == -1)
            break;

        void *start, *end;
        if (sscanf (line, "%p-%p", &start, &end) < 2)
            continue;
        if (needle < (uintptr_t)start || (uintptr_t)end <= needle)
            continue;
        char *dir = strchr (line, '/');
        if (!end || !dir )
            continue;
        char *nl  = strchr (line, '\n');
        if (*nl)
            *nl = '\0';
        strncpy(program_path, dir, PATH_MAX);
        break;
    }
    free (line);
    fclose (maps);
}

} // end namespace sfl_video

