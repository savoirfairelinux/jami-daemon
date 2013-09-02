/*
 *  Copyright (C) 2011-2013 Savoir-Faire Linux Inc.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
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

#include <cstdio>
#include <cstdlib>
#include "shm_src.h"
#include "../shm_header.h"
#include "../../noncopyable.h"
#include <sys/mman.h>
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

        void render_to_texture()
        {
            if (shm_area_ == MAP_FAILED) {
                g_print("shm_area is MAP FAILED!\n");
                return;
            }

            shm_lock();

            while (buffer_gen_ == shm_area_->buffer_gen) {
                shm_unlock();
                sem_wait(&shm_area_->notification);

                shm_lock();
            }

            if (!resize_area()) {
                g_print("could not resize area\n");
                return;
            }

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
    src->render_to_texture();
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
    g_timeout_add_full(G_PRIORITY_DEFAULT_IDLE, 30, updateTexture, &src, NULL);

    clutter_actor_show_all(stage);

    /* main loop */
    clutter_main();
    src.stop();

    return 0;
}
