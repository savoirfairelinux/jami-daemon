/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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
#include "dbus/callmanager.h"
#include "video_picture.h"
#include "fileutils.h"

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

int createSemSet(int shmKey, int *semKey)
{
    /* this variable will contain the semaphore set. */
    int sem_set_id;
    /* semaphore value, for semctl().                */
    union semun sem_val;
    key_t key;

    do
		key = ftok(get_program_dir(), rand());
    while(key == shmKey);

    *semKey = key;

    /* first we create a semaphore set with a single semaphore, */
    /* whose counter is initialized to '0'.                     */
    sem_set_id = semget(key, 1, 0600 | IPC_CREAT);
    if (sem_set_id == -1) {
        _error("%s:semget:%m", __PRETTY_FUNCTION__);
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
    key = ftok(get_program_dir(), proj_id);
    *shmKey = key;
    shm_id = shmget(key, numBytes, 0644 | IPC_CREAT);

    if (shm_id == -1)
        _error("%s:shmget:%m", __PRETTY_FUNCTION__);

    return shm_id;
}

/* attach a shared memory segment */
uint8_t *attachShm(int shm_id)
{
    uint8_t *data = NULL;

    /* attach to the segment and get a pointer to it */
    data = reinterpret_cast<uint8_t*>(shmat(shm_id, (void *)0, 0));
    if (data == (uint8_t *)(-1)) {
        _error("%s:shmat:%m", __PRETTY_FUNCTION__);
        data = NULL;
    }

    return data;
}

void detachShm(uint8_t *data)
{
    /* detach from the segment: */
    if (shmdt(data) == -1)
        _error("%s:shmdt:%m", __PRETTY_FUNCTION__);
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

void VideoReceiveThread::loadSDP()
{
    assert(not args_["receiving_sdp"].empty());
    // this memory will be released on next call to tmpnam
    std::ofstream os;
    sdpFilename_ = openTemp("/tmp", os);

    os << args_["receiving_sdp"];
    _debug("%s:loaded SDP %s", __PRETTY_FUNCTION__,
            args_["receiving_sdp"].c_str());

    os.close();
}

void VideoReceiveThread::setup()
{
    dstWidth_ = atoi(args_["width"].c_str());
    dstHeight_ = atoi(args_["height"].c_str());
    format_ = av_get_pix_fmt(args_["format"].c_str());
    if (format_ == -1)
    {
        _error("%s:Couldn't find a pixel format for \"%s\"",
                __PRETTY_FUNCTION__, args_["format"].c_str());
        exit();
    }

    AVInputFormat *file_iformat = 0;

    if (!test_source_)
    {
        if (args_["input"].empty())
        {
            loadSDP();
            args_["input"] = sdpFilename_;
            file_iformat = av_find_input_format("sdp");
            if (!file_iformat)
            {
                _error("%s:Could not find format \"sdp\"", __PRETTY_FUNCTION__);
                exit();
            }
        }
        else if (args_["input"].substr(0, strlen("/dev/video")) == "/dev/video")
        {
            // it's a v4l device if starting with /dev/video
            // FIXME: This is not the most robust way of checking if we mean to use a
            // v4l device
            _debug("Using v4l2 format");
            file_iformat = av_find_input_format("video4linux2");
            if (!file_iformat)
            {
                _error("%s:Could not find format!", __PRETTY_FUNCTION__);
                exit();
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
            _error("%s:Could not open input file \"%s\"", __PRETTY_FUNCTION__,
                    args_["input"].c_str());
            exit();
        }

        int ret;
        // retrieve stream information
        ret = av_find_stream_info(inputCtx_);
        if (ret < 0)
        {
            _error("%s:Could not find stream info!", __PRETTY_FUNCTION__);
            exit();
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
            _error("%s:Could not find video stream!", __PRETTY_FUNCTION__);
            exit();
        }

        // Get a pointer to the codec context for the video stream
        decoderCtx_ = inputCtx_->streams[videoStreamIndex_]->codec;

        // find the decoder for the video stream
        AVCodec *inputDecoder = avcodec_find_decoder(decoderCtx_->codec_id);
        if (inputDecoder == NULL)
        {
            _error("%s:Unsupported codec!", __PRETTY_FUNCTION__);
            exit();
        }

        // open codec
        ret = avcodec_open(decoderCtx_, inputDecoder);
        if (ret < 0)
        {
            _error("%s:Could not open codec!", __PRETTY_FUNCTION__);
            exit();
        }
    }

    scaledPicture_ = avcodec_alloc_frame();
    if (scaledPicture_ == 0)
    {
        _error("%s:Could not allocated output frame!", __PRETTY_FUNCTION__);
        exit();
    }

    if (dstWidth_ == 0 and dstHeight_ == 0)
    {
        dstWidth_ = decoderCtx_->width;
        dstHeight_ = decoderCtx_->height;
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

    // we're receiving RTP
    if (args_["input"] == sdpFilename_)
    {
        // publish our new video stream's existence
        _debug("Publishing shm: %d sem: %d size: %d", shmKey_, semKey_,
                videoBufferSize_);
        // Fri Jul 15 12:15:59 EDT 2011:tmatth:FIXME: access to call manager
        // from this thread may not be thread-safe
        DBusManager::instance().getCallManager()->receivingVideoEvent(shmKey_,
                semKey_, videoBufferSize_, dstWidth_, dstHeight_);
    }
}

// NOT called from this (the run() ) thread
void VideoReceiveThread::waitForShm()
{
    shmReady_.wait();
}

void VideoReceiveThread::cleanup()
{
    _debug("%s", __PRETTY_FUNCTION__);
    // make sure no one is waiting for the SHM event which will never come if we've
    // error'd out
    shmReady_.signal();
    // free shared memory
    cleanupSemaphore(semSetID_);
    cleanupShm(shmID_, shmBuffer_);
    semSetID_ = -1;
    shmID_ = -1;
    shmBuffer_ = 0;

    if (imgConvertCtx_)
    {
        sws_freeContext(imgConvertCtx_);
        imgConvertCtx_ = 0;
    }

    // free the scaled frame
    if (scaledPicture_)
    {
        av_free(scaledPicture_);
        scaledPicture_ = 0;
    }
    // free the YUV frame
    if (rawFrame_)
    {
        av_free(rawFrame_);
        rawFrame_ = 0;
    }

    // doesn't need to be freed, we didn't use avcodec_alloc_context
    if (decoderCtx_)
    {
        avcodec_close(decoderCtx_);
        decoderCtx_ = 0;
    }

    // close the video file
    if (inputCtx_)
    {
        av_close_input_file(inputCtx_);
        inputCtx_ = 0;
    }
    _debug("Finished %s", __PRETTY_FUNCTION__);
}

void VideoReceiveThread::createScalingContext()
{
    // Create scaling context, no scaling done here
    imgConvertCtx_ = sws_getCachedContext(imgConvertCtx_, decoderCtx_->width,
            decoderCtx_->height, decoderCtx_->pix_fmt, dstWidth_,
            dstHeight_, (enum PixelFormat) format_, SWS_BICUBIC,
            NULL, NULL, NULL);
    if (imgConvertCtx_ == 0)
    {
        _error("Cannot init the conversion context!");
        exit();
    }
}

VideoReceiveThread::VideoReceiveThread(const std::map<std::string, std::string> &args) : args_(args),
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
    imgConvertCtx_(0),
    dstWidth_(-1),
    dstHeight_(-1)
{
    test_source_ = (args_["input"] == "SFLTEST");
    setCancel(cancelImmediate);
}

void VideoReceiveThread::run()
{
    setup();
    AVPacket inpacket;
    int frameFinished;
    enum PixelFormat fmt = (enum PixelFormat) format_;

    if (!test_source_)
        createScalingContext();

    for (;;)
    {
        if (!test_source_)
        {
            errno = av_read_frame(inputCtx_, &inpacket);
            if (errno < 0) {
                _error("Couldn't read frame : %m\n");
                break;
            }

            // is this a packet from the video stream?
            if (inpacket.stream_index != videoStreamIndex_)
                goto next_packet;

            // decode video frame from camera
            avcodec_decode_video2(decoderCtx_, rawFrame_, &frameFinished, &inpacket);
            if (!frameFinished)
                goto next_packet;

            avpicture_fill(reinterpret_cast<AVPicture *>(scaledPicture_),
                    reinterpret_cast<uint8_t*>(shmBuffer_), fmt, dstWidth_, dstHeight_);

            sws_scale(imgConvertCtx_, rawFrame_->data, rawFrame_->linesize,
                    0, decoderCtx_->height, scaledPicture_->data,
                    scaledPicture_->linesize);
        }
        else
        {
            // assign appropriate parts of buffer to image planes in scaledPicture
            avpicture_fill(reinterpret_cast<AVPicture *>(scaledPicture_),
                    reinterpret_cast<uint8_t*>(shmBuffer_), fmt, dstWidth_, dstHeight_);
            const AVPixFmtDescriptor *pixdesc = &av_pix_fmt_descriptors[format_];
            int components = pixdesc->nb_components;
            int planes = 0;
            for (int i = 0; i < components; i++)
                if (pixdesc->comp[i].plane > planes)
                    planes = pixdesc->comp[i].plane;
            planes++;

            int i = frameNumber_++;
            const unsigned pitch = scaledPicture_->linesize[0];

            for (int y = 0; y < dstHeight_; y++)
                for (unsigned x=0; x < pitch; x++)
                    scaledPicture_->data[0][y * pitch + x] = x + y + i * planes;
        }

        /* signal the semaphore that a new frame is ready */ 
        sem_signal(semSetID_);

        if (test_source_)
        {
            yield();
            continue;
        }

        // free the packet that was allocated by av_read_frame
next_packet:
        av_free_packet(&inpacket);
        yield();
    }
}

VideoReceiveThread::~VideoReceiveThread()
{
    // free resources, exit thread
    DBusManager::instance().getCallManager()->stoppedReceivingVideoEvent(shmKey_,
            semKey_);
    ost::Thread::terminate();
    cleanup();
}

} // end namespace sfl_video

