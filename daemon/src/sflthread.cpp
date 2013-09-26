/*
 *  Copyright (C) 2013 Savoir-Faire Linux Inc.
 *
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
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

#include "sflthread.h"
#include "logger.h"

void* SFLThread::run_(void* data)
{
    SFLThread *obj = static_cast<SFLThread*>(data);
    obj->mainloop_();
    return nullptr;
}

void SFLThread::mainloop_()
{
    if (setup()) {
        while (running_)
            process();
        cleanup();
    } else
        ERROR("setup failed");
}

SFLThread::SFLThread() : thread_(), running_(false)
{}

SFLThread::~SFLThread()
{
    if (isRunning()) {
        stop();
        join();
    }
}

void SFLThread::start()
{
    if (!running_) {
        running_ = true;
        pthread_create(&thread_, NULL, &run_, this);
    }
}

void SFLThread::stop()
{
    running_ = false;
}

void SFLThread::join()
{
    if (thread_)
        pthread_join(thread_, NULL);
}

void SFLThread::exit()
{
    stop();
    pthread_exit(NULL);
}

bool SFLThread::isRunning()
{
    return running_;
}
