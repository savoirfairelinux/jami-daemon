/*
 *  Copyright (C) 2016 Savoir-faire Linux Inc.
 *
 *  Author: Edric Ladent-Milaret <edric.ladent-milaret@savoirfairelinux.com>
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
 */

#pragma once

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8)
#define RING_THREADLOCAL(type, name) thread_local type* name
#elif !TARGET_OS_IPHONE
#define RING_THREADLOCAL(type, name) __thread type* name
#else
#include <pthread.h>

#define RING_THREADLOCAL(type, name) ThreadLocal<type> name

template <typename T, int NUM=0>
class ThreadLocal {
public:
    void setup() {
        static pthread_once_t once = PTHREAD_ONCE_INIT;
        pthread_once(&once, createKey);
    }

    ThreadLocal& operator=(decltype(nullptr)) {
        setup();
        pthread_setspecific(key, 0);
        return *this;
    }

    ThreadLocal& operator=(T* val) {
        setup();
        pthread_setspecific(key, val);
        return *this;
    }

    T* operator*() {
        setup();
        return (T*)pthread_getspecific(key);
    }

    operator T*() {
        return operator*();
    }
private:
    static pthread_key_t key;

    static void createKey() {
        pthread_key_create(&key, 0);
    }
};

template <typename T, int NUM>
pthread_key_t ThreadLocal<T, NUM>::key;

#endif
