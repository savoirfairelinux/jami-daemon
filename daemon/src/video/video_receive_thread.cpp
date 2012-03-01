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
#include "packet_handle.h"

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
#include "dbus/video_controls.h"
#include "fileutils.h"

static const enum PixelFormat video_rgb_format = PIX_FMT_BGRA;

namespace sfl_video {

using std::map;
using std::string;

namespace { // anonymous namespace

#if _SEM_SEMUN_UNDEFINED
union semun {
 int val;				    /* value for SETVAL */
 struct semid_ds *buf;		/* buffer for IPC_STAT & IPC_SET */
 unsigned short int *array;	/* array for GETALL & SETALL */
 struct seminfo *__buf;		/* buffer for IPC_INFO */
};
#endif

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
    /* connect to and possibly create a segment with 644 permissions
       (rw-r--r--) */

    srand(time(NULL));
    int proj_id = rand();
    key_t key = ftok(fileutils::get_program_dir(), proj_id);
    *shmKey = key;
    int shm_id = shmget(key, numBytes, 0644 | IPC_CREAT);

    if (shm_id == -1)
        ERROR("%s:shmget:%m", __PRETTY_FUNCTION__);

    return shm_id;
}

/* attach a shared memory segment */
uint8_t *attachShm(int shm_id)
{
    /* attach to the segment and get a pointer to it */
    uint8_t *data = reinterpret_cast<uint8_t*>(shmat(shm_id, (void *) 0, 0));
    if (data == reinterpret_cast<uint8_t *>(-1)) {
        ERROR("%s:shmat:%m", __PRETTY_FUNCTION__);
        data = NULL;
    }

    return data;
}

void detachShm(uint8_t *data)
{
    /* detach from the segment: */
    if (data and shmdt(data) == -1)
        ERROR("%s:shmdt:%m", __PRETTY_FUNCTION__);
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


string openTemp(string path, std::ofstream& f)
{
    path += "/XXXXXX";
    std::vector<char> dst_path(path.begin(), path.end());
    dst_path.push_back('\0');

    int fd = -1;
    while (fd == -1) {
        fd = mkstemp(&dst_path[0]);
        if (fd != -1) {
            path.assign(dst_path.begin(), dst_path.end() - 1);
            f.open(path.c_str(), std::ios_base::trunc | std::ios_base::out);
            close(fd);
        }
    }
    return path;
}
} // end anonymous namespace

int VideoReceiveThread::createSemSet(int shmKey, int *semKey)
{
    key_t key;
    do
		key = ftok(fileutils::get_program_dir(), rand());
    while (key == shmKey);

    *semKey = key;

    /* first we create a semaphore set with a single semaphore,
       whose counter is initialized to '0'. */
    int sem_set_id = semget(key, 1, 0600 | IPC_CREAT);
    if (sem_set_id == -1) {
        ERROR("%s:semget:%m", __PRETTY_FUNCTION__);
        ost::Thread::exit();
    }

    /* semaphore value, for semctl(). */
    union semun sem_val;
    sem_val.val = 0;
    semctl(sem_set_id, 0, SETVAL, sem_val);
    return sem_set_id;
}


void VideoReceiveThread::loadSDP()
{
    assert(not args_["receiving_sdp"].empty());
    // this memory will be released on next call to tmpnam
    std::ofstream os;
    sdpFilename_ = openTemp("/tmp", os);

    os << args_["receiving_sdp"];
    DEBUG("%s:loaded SDP %s", __PRETTY_FUNCTION__,
          args_["receiving_sdp"].c_str());

    os.close();
}

void VideoReceiveThread::setup()
{
    dstWidth_ = atoi(args_["width"].c_str());
    dstHeight_ = atoi(args_["height"].c_str());

    AVInputFormat *file_iformat = 0;

    if (args_["input"].empty()) {
        loadSDP();
        args_["input"] = sdpFilename_;
        file_iformat = av_find_input_format("sdp");
        if (!file_iformat) {
            ERROR("%s:Could not find format \"sdp\"", __PRETTY_FUNCTION__);
            ost::Thread::exit();
        }
    } else if (args_["input"].substr(0, strlen("/dev/video")) == "/dev/video") {
        // it's a v4l device if starting with /dev/video
        // FIXME: This is not the most robust way of checking if we mean to use a
        // v4l device
        DEBUG("Using v4l2 format");
        file_iformat = av_find_input_format("video4linux2");
        if (!file_iformat) {
            ERROR("%s:Could not find format!", __PRETTY_FUNCTION__);
            ost::Thread::exit();
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
    if (avformat_open_input(&inputCtx_, args_["input"].c_str(), file_iformat,
                            &options) != 0) {
        ERROR("%s:Could not open input file \"%s\"", __PRETTY_FUNCTION__,
              args_["input"].c_str());
        ost::Thread::exit();
    }

    // retrieve stream information
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(53, 8, 0)
    if (av_find_stream_info(inputCtx_) < 0) {
#else
    if (avformat_find_stream_info(inputCtx_, NULL) < 0) {
#endif
        ERROR("%s:Could not find stream info!", __PRETTY_FUNCTION__);
        ost::Thread::exit();
    }

    // find the first video stream from the input
    for (unsigned i = 0; i < inputCtx_->nb_streams; i++) {
        if (inputCtx_->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex_ = i;
            break;
        }
    }

    if (videoStreamIndex_ == -1) {
        ERROR("%s:Could not find video stream!", __PRETTY_FUNCTION__);
        ost::Thread::exit();
    }

    // Get a pointer to the codec context for the video stream
    decoderCtx_ = inputCtx_->streams[videoStreamIndex_]->codec;

    // find the decoder for the video stream
    AVCodec *inputDecoder = avcodec_find_decoder(decoderCtx_->codec_id);
    if (inputDecoder == NULL) {
        ERROR("%s:Unsupported codec!", __PRETTY_FUNCTION__);
        ost::Thread::exit();
    }

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(53, 6, 0)
    if (avcodec_open(decoderCtx_, inputDecoder) < 0) {
#else
    if (avcodec_open2(decoderCtx_, inputDecoder, NULL) < 0) {
#endif
        ERROR("%s:Could not open codec!", __PRETTY_FUNCTION__);
        ost::Thread::exit();
    }

    scaledPicture_ = avcodec_alloc_frame();
    if (scaledPicture_ == 0) {
        ERROR("%s:Could not allocated output frame!", __PRETTY_FUNCTION__);
        ost::Thread::exit();
    }

    if (dstWidth_ == 0 and dstHeight_ == 0) {
        dstWidth_ = decoderCtx_->width;
        dstHeight_ = decoderCtx_->height;
    }

    // determine required buffer size and allocate buffer
    videoBufferSize_ = bufferSize(dstWidth_, dstHeight_, video_rgb_format);

    // create shared memory segment and attach to it
    shmID_ = createShm(videoBufferSize_, &shmKey_);
    shmBuffer_  = attachShm(shmID_);
    semSetID_ = createSemSet(shmKey_, &semKey_);
    shmReady_.signal();

    // allocate video frame
    rawFrame_ = avcodec_alloc_frame();

    // we're receiving RTP
    if (args_["input"] == sdpFilename_) {
        // publish our new video stream's existence
        DEBUG("Publishing shm: %d sem: %d size: %d", shmKey_, semKey_,
              videoBufferSize_);
        // Fri Jul 15 12:15:59 EDT 2011:tmatth:FIXME: access to call manager
        // from this thread may not be thread-safe
        Manager::instance().getDbusManager()->getVideoControls()->receivingEvent(shmKey_,
                semKey_, videoBufferSize_, dstWidth_, dstHeight_);
    }
}

// NOT called from this (the run() ) thread
void VideoReceiveThread::waitForShm()
{
    shmReady_.wait();
}

void VideoReceiveThread::createScalingContext()
{
    // Create scaling context, no scaling done here
    imgConvertCtx_ = sws_getCachedContext(imgConvertCtx_, decoderCtx_->width,
                                          decoderCtx_->height,
                                          decoderCtx_->pix_fmt, dstWidth_,
                                          dstHeight_, video_rgb_format,
                                          SWS_BICUBIC, NULL, NULL, NULL);
    if (imgConvertCtx_ == 0) {
        ERROR("Cannot init the conversion context!");
        ost::Thread::exit();
    }
}

VideoReceiveThread::VideoReceiveThread(const map<string, string> &args) :
    args_(args),
    frameNumber_(0),
    shmBuffer_(0),
    shmID_(-1),
    semSetID_(-1),
    shmKey_(-1),
    semKey_(-1),
    videoBufferSize_(0),
    decoderCtx_(0),
    rawFrame_(0),
    scaledPicture_(0),
    videoStreamIndex_(-1),
    inputCtx_(0),
    imgConvertCtx_(0),
    dstWidth_(-1),
    dstHeight_(-1),
    shmReady_(),
    sdpFilename_()
{
    setCancel(cancelDeferred);
}

void VideoReceiveThread::run()
{
    setup();

    createScalingContext();
    while (not testCancel()) {
        AVPacket inpacket;
        errno = av_read_frame(inputCtx_, &inpacket);
        if (errno < 0) {
            ERROR("Couldn't read frame : %m\n");
            break;
        }
        PacketHandle inpacket_handle(inpacket);

        // is this a packet from the video stream?
        if (inpacket.stream_index == videoStreamIndex_) {
            int frameFinished;
            avcodec_decode_video2(decoderCtx_, rawFrame_, &frameFinished, &inpacket);
            if (frameFinished) {
                avpicture_fill(reinterpret_cast<AVPicture *>(scaledPicture_),
                               shmBuffer_, video_rgb_format, dstWidth_,
                               dstHeight_);

                sws_scale(imgConvertCtx_, rawFrame_->data, rawFrame_->linesize,
                          0, decoderCtx_->height, scaledPicture_->data,
                          scaledPicture_->linesize);

                // signal the semaphore that a new frame is ready
                sem_signal(semSetID_);
            }
        }
    }
}

VideoReceiveThread::~VideoReceiveThread()
{
    // free resources, exit thread
	Manager::instance().getDbusManager()->getVideoControls()->stoppedReceivingEvent(shmKey_, semKey_);
    ost::Thread::terminate();

    // make sure no one is waiting for the SHM event which will never come if we've error'd out
    shmReady_.signal();

    cleanupSemaphore(semSetID_);
    cleanupShm(shmID_, shmBuffer_);

    if (imgConvertCtx_)
        sws_freeContext(imgConvertCtx_);

    if (scaledPicture_)
        av_free(scaledPicture_);

    if (rawFrame_)
        av_free(rawFrame_);

    if (decoderCtx_)
        avcodec_close(decoderCtx_);

    if (inputCtx_)
        av_close_input_file(inputCtx_);
}
} // end namespace sfl_video
