#include <cstdio>
#include <cstdlib>
#include "shm_src.h"
#include "../shm_header.h"
#include "../../noncopyable.h"
#include <iostream>
#include <clutter/clutter.h>

class ClutterSHMSrc : public SHMSrc {
    public:
        ClutterSHMSrc(const std::string &name,
                unsigned width,
                unsigned height,
                ClutterActor *texture) :
            SHMSrc(name),
            width_(width),
            height_(height),
            texture_(texture)
            {
                printf("Creating source with name:%s width:%d height:%d texture:%p\n", name.c_str(), width, height, texture);
            }

        // override default memcpy implementation
        void render(char * /*data*/, size_t /*len*/)
        {
            shm_lock();

            while (buffer_gen_ == shm_area_->buffer_gen) {
                shm_unlock();
                sem_wait(&shm_area_->notification);

                shm_lock();
            }

            if (!resize_area())
                return;

            clutter_actor_set_size(texture_, width_, height_);
            const int BPP = 4;
            const int ROW_STRIDE = BPP * width_;
            /* update the clutter texture */
            clutter_texture_set_from_rgb_data(CLUTTER_TEXTURE(texture_),
                    reinterpret_cast<const unsigned char *>(shm_area_->data),
                    TRUE,
                    width_,
                    height_,
                    ROW_STRIDE,
                    BPP,
                    CLUTTER_TEXTURE_RGB_FLAG_BGR,
                    NULL);
            buffer_gen_ = shm_area_->buffer_gen;
            shm_unlock();
        }

    private:
        NON_COPYABLE(ClutterSHMSrc);
        unsigned width_;
        unsigned height_;
        ClutterActor *texture_;
};

gboolean updateTexture(gpointer data)
{
    ClutterSHMSrc *src = static_cast<ClutterSHMSrc *>(data);
    src->render(NULL, 0);
    return TRUE;
}

int main(int argc, char *argv[])
{
    if (argc < 4) {
        printf("Usage: ./shmclient <shm_filename> <width> <height>\n");
        return 1;
    }

    /* Initialize Clutter */
    if (clutter_init(NULL, NULL) != CLUTTER_INIT_SUCCESS)
        return 1;

    /* Get the default stage */
    ClutterActor *stage = clutter_stage_get_default();

    const int width = atoi(argv[2]);
    const int height = atoi(argv[3]);

    clutter_actor_set_size(stage, width, height);

    ClutterActor *texture = clutter_texture_new();

    clutter_stage_set_title(CLUTTER_STAGE(stage), "Client");
    /* Add ClutterTexture to the stage */
    clutter_container_add(CLUTTER_CONTAINER(stage), texture, NULL);

    ClutterSHMSrc src(argv[1], width, height, texture);
    if (not src.start()) {
        printf("Could not start SHM source\n");
        return 1;
    }
    /* frames are read and saved here */
    g_idle_add(updateTexture, &src);

    clutter_actor_show_all(stage);

    /* main loop */
    clutter_main();
    src.stop();

    return 0;
}
