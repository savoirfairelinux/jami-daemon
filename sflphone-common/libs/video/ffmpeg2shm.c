#include <assert.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>     /* semaphore functions and structs.    */
#include <sys/shm.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>

#if _SEM_SEMUN_UNDEFINED
union semun
{
 int val;				    /* value for SETVAL */
 struct semid_ds *buf;		/* buffer for IPC_STAT & IPC_SET */
 unsigned short int *array;	/* array for GETALL & SETALL */
 struct seminfo *__buf;		/* buffer for IPC_INFO */
};
#endif

static volatile int interrupted = 0;

void signal_handler(int sig) { (void)sig; interrupted = 1; }
void attach_signal_handlers() { signal(SIGINT, signal_handler); }


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

int create_sem_set()
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
char *attachShm(int shm_id)
{
    char *data = NULL;

    /* attach to the segment and get a pointer to it */
    data = shmat(shm_id, (void *)0, 0);
    if (data == (char *)(-1)) {
        perror("shmat");
        data = NULL;
    }

    return data;
}

void detachShm(char *data)
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

void cleanupShm(int shm_id, char *data)
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

int main(int argc, char *argv[])
{
    attach_signal_handlers();
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    av_register_all();
    avdevice_register_all();
    AVFormatContext *formatCtx;

    AVInputFormat *file_iformat = NULL;
    // it's a v4l device if starting with /dev/video0
    if (strncmp(argv[1], "/dev/video", strlen("/dev/video")) == 0) {
        printf("Using v4l2 format\n");
        file_iformat = av_find_input_format("video4linux2");
        if (!file_iformat) {
            fprintf(stderr, "Could not find format!\n");
            return 1;
        }
    }

    // Open video file
    if (av_open_input_file(&formatCtx, argv[1], file_iformat, 0, NULL) != 0) {
        fprintf(stderr, "Could not open input file!\n");
        return 1; // couldn't open file
    }

    // retrieve stream information
    if (av_find_stream_info(formatCtx) < 0) {
        fprintf(stderr, "Could not find stream info!\n");
        return 1; // couldn't find stream info
    }

    int i;
    AVCodecContext *camDecoderCtx;

    // find the first video stream
    int videoStream = -1;
    for (i = 0; i < formatCtx->nb_streams; i++) {
        if (formatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStream = i;
            break;
        }
    }
    if (videoStream == -1) {
        fprintf(stderr, "Could not find video stream!\n");
        return 1; // didn't find a video stream
    }

    // Get a pointer to the codec context for the video stream
    camDecoderCtx = formatCtx->streams[videoStream]->codec;

    // find the decoder for the video stream
    AVCodec *camDecoder = avcodec_find_decoder(camDecoderCtx->codec_id);
    if (camDecoder == NULL) {
        fprintf(stderr, "Unsupported codec!\n");
        return 1; // codec not found
    }

    // open codec
    if (avcodec_open(camDecoderCtx, camDecoder) < 0) {
        fprintf(stderr, "Could not open codec!\n");
        return 1; // could not open codec
    }

    AVFrame *frame;

    // allocate video frame
    frame = avcodec_alloc_frame();

    // allocate an AVFrame structure
    AVFrame *frameRGB = avcodec_alloc_frame();
    if (frameRGB == NULL)
        return 1;

    unsigned numBytes;
    // determine required buffer size and allocate buffer
    numBytes = bufferSizeRGB24(camDecoderCtx->width, camDecoderCtx->height);
    /*printf("%u bytes\n", numBytes);*/
    postFrameSize(camDecoderCtx->width, camDecoderCtx->height, numBytes);

    int frameFinished;
    AVPacket packet;

    struct SwsContext *img_convert_ctx = NULL;

    int shm_id = createShm(numBytes);
    char *shm_buffer  = attachShm(shm_id);
    int sem_set_id = create_sem_set();

    // assign appropriate parts of buffer to image planes in frameRGB
    // Note that frameRGB is an AVFrame, but AVFrame is a superset
    // of AVPicture
    avpicture_fill((AVPicture *)frameRGB, (uint8_t *)shm_buffer,
            PIX_FMT_RGB24, camDecoderCtx->width, camDecoderCtx->height);


    /* Main loop, frames are read and saved here */
    i = 0;
    while (!interrupted && av_read_frame(formatCtx, &packet) >= 0) {
        // is this a packet from the video stream?
        if (packet.stream_index == videoStream) {
            // decode video frame from camera
            avcodec_decode_video2(camDecoderCtx, frame, &frameFinished, &packet);

            // did we get a complete frame?
            if (frameFinished)  {
                int w = camDecoderCtx->width;
                int h = camDecoderCtx->height;
                /* FIXME: should the colorspace conversion/scaling be done with
                 * libavfilter instead? It is in ffmpeg.c */
                img_convert_ctx = sws_getContext(w, h, camDecoderCtx->pix_fmt, w,
                        h, PIX_FMT_RGB24, SWS_BICUBIC,
                        NULL, NULL, NULL);
                if (img_convert_ctx == NULL) {
                    fprintf(stderr, "Cannot init the conversion context!\n");
                    exit(1);
                }

                /* here we're not actually scaling, just converting from YUV to
                 * RGB */
                sws_scale(img_convert_ctx, (void*)frame->data, frame->linesize,
                        0, camDecoderCtx->height, frameRGB->data,
                        frameRGB->linesize);
                /* signal the semaphore that a new frame is ready */ 
                sem_signal(sem_set_id);
            }
        }
        // free the packet that was allocated by av_read_frame
        av_free_packet(&packet);
    }

    /* free shared memory */
    cleanupSemaphore(sem_set_id);
    cleanupShm(shm_id, shm_buffer);

    // free the rgb image
    av_free(frameRGB);

    // free the YUV frame
    av_free(frame);

    /* doesn't need to be freed, we didn't use avcodec_alloc_context */
    avcodec_close(camDecoderCtx);

    // close the video file
    av_close_input_file(formatCtx);

    printf("Exitting...\n");
    return 0;
}
