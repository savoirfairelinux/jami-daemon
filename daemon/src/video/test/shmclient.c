#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>     /* semaphore functions and structs.    */
#include <sys/shm.h>

#include <clutter/clutter.h>
#define TEMPFILE "/tmp/frame.txt"

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

struct AppData {
    unsigned width;
    unsigned height;
    char *shm_buffer;
    int sem_set_id;
    ClutterActor *texture;
};

FrameInfo getFrameSize()
{
    FrameInfo info;

    /* get message out of the file */
    FILE *tmp = fopen(TEMPFILE, "r");
    fscanf(tmp, "%u\n%u\n%u\n", &info.size, &info.width, &info.height);
    printf("Size is %u\n", info.size);
    printf("Width is %u\n", info.width);
    printf("Height is %u\n", info.height);
    fclose(tmp);
    return info;
}

int get_sem_set()
{
    /* this variable will contain the semaphore set. */
    int sem_set_id;
    key_t key = ftok("/tmp", 'b');

    /* semaphore value, for semctl().                */
    union semun sem_val;

    /* first we get a semaphore set with a single semaphore, */
    /* whose counter is initialized to '0'.                     */
    sem_set_id = semget(key, 1, 0600);
    if (sem_set_id == -1) {
        perror("semget");
        exit(1);
    }
    sem_val.val = 0;
    semctl(sem_set_id, 0, SETVAL, sem_val);
    return sem_set_id;
}

/*
 * function: sem_wait. wait for frame from other process
 * input:    semaphore set ID.
 * output:   none.
 */
    void
sem_wait(int sem_set_id)
{
    /* structure for semaphore operations.   */
    struct sembuf sem_op;

    /* wait on the semaphore, unless it's value is non-negative. */
    sem_op.sem_num = 0;
    sem_op.sem_op = -1;
    sem_op.sem_flg = 0;
    semop(sem_set_id, &sem_op, 1);
}

/* join and/or create a shared memory segment */
int getShm(unsigned numBytes)
{
    key_t key;
    int shm_id;
    /* connect to a segment with 600 permissions
       (r--r--r--) */
    key = ftok("/tmp", 'c');
    shm_id = shmget(key, numBytes, 0644);

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

/* round integer value up to next multiple of 4 */
int round_up_4(int value)
{
    return (value + 3) &~ 3;
}

void readFrameFromShm(int width, int height, char *data, int sem_set_id,
        ClutterActor *texture)
{
    sem_wait(sem_set_id);
    clutter_texture_set_from_rgb_data (CLUTTER_TEXTURE(texture),
            (void*)data,
            FALSE,
            width,
            height,
            round_up_4(3 * width),
            3,
            0,
            NULL);
}

gboolean updateTexture(gpointer data)
{
    struct AppData *app = (struct AppData*) data;
    readFrameFromShm(app->width, app->height, app->shm_buffer, app->sem_set_id,
            app->texture);
    return TRUE;
}

int main(int argc, char *argv[])
{
    /* Initialize Clutter */
    if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
        return 1;
    FrameInfo info = getFrameSize();
    int shm_id = getShm(info.size);
    char *shm_buffer = attachShm(shm_id);
    if (shm_buffer == NULL)
        return 1;
    int sem_set_id = get_sem_set();

    ClutterActor *stage, *texture;

    /* Get the default stage */
    stage = clutter_stage_get_default ();
    clutter_actor_set_size(stage,
            info.width,
            info.height);

    texture = clutter_texture_new();

    clutter_stage_set_title(CLUTTER_STAGE (stage), "Client");
    /* Add ClutterTexture to the stage */
    clutter_container_add(CLUTTER_CONTAINER (stage), texture, NULL);

    struct AppData app = {info.width, info.height, shm_buffer, sem_set_id,
        texture};
    /* frames are read and saved here */
    g_idle_add(updateTexture, &app);

    clutter_actor_show_all(stage);

    /* main loop */
    clutter_main();

    detachShm(shm_buffer);

    return 0;
}
