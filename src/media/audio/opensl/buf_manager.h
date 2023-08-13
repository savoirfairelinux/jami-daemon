/*
 * Copyright 2015 The Android Open Source Project
 * Copyright 2015-2023 Savoir-faire Linux Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "logger.h"
#include "noncopyable.h"

#include <SLES/OpenSLES.h>
#include <sys/types.h>

#include <atomic>
#include <cassert>
#include <memory>
#include <limits>
#include <vector>

/*
 * ProducerConsumerQueue, borrowed from Ian NiLewis
 */
template<typename T>
class ProducerConsumerQueue
{
public:
    explicit ProducerConsumerQueue(size_t size)
        : buffer_(size)
    {
        // This is necessary because we depend on twos-complement wraparound
        // to take care of overflow conditions.
        assert(size < std::numeric_limits<int>::max());
    }

    void clear()
    {
        read_.store(0);
        write_.store(0);
    }

    bool push(const T& item)
    {
        return push([&](T* ptr) -> bool {
            *ptr = item;
            return true;
        });
    }

    // get() is idempotent between calls to commit().
    T* getWriteablePtr()
    {
        T* result = nullptr;

        bool check __attribute__((unused)); //= false;

        check = push([&](T* head) -> bool {
            result = head;
            return false; // don't increment
        });

        // if there's no space, result should not have been set, and vice versa
        assert(check == (result != nullptr));

        return result;
    }

    bool commitWriteablePtr(T* ptr)
    {
        bool result = push([&](T* head) -> bool {
            // this writer func does nothing, because we assume that the caller
            // has already written to *ptr after acquiring it from a call to get().
            // So just double-check that ptr is actually at the write head, and
            // return true to indicate that it's safe to advance.

            // if this isn't the same pointer we got from a call to get(), then
            // something has gone terribly wrong. Either there was an intervening
            // call to push() or commit(), or the pointer is spurious.
            assert(ptr == head);
            return true;
        });
        return result;
    }

    // writer() can return false, which indicates that the caller
    // of push() changed its mind while writing (e.g. ran out of bytes)
    template<typename F>
    bool push(const F& writer)
    {
        bool result = false;
        int readptr = read_.load(std::memory_order_acquire);
        int writeptr = write_.load(std::memory_order_relaxed);

        // note that while readptr and writeptr will eventually
        // wrap around, taking their difference is still valid as
        // long as size_ < MAXINT.
        int space = buffer_.size() - (int) (writeptr - readptr);
        if (space >= 1) {
            result = true;

            // writer
            if (writer(buffer_.data() + (writeptr % buffer_.size()))) {
                ++writeptr;
                write_.store(writeptr, std::memory_order_release);
            }
        }
        return result;
    }
    // front out the queue, but not pop-out
    bool front(T* out_item)
    {
        return front([&](T* ptr) -> bool {
            *out_item = *ptr;
            return true;
        });
    }

    void pop(void)
    {
        int readptr = read_.load(std::memory_order_relaxed);
        ++readptr;
        read_.store(readptr, std::memory_order_release);
    }

    template<typename F>
    bool front(const F& reader)
    {
        bool result = false;

        int writeptr = write_.load(std::memory_order_acquire);
        int readptr = read_.load(std::memory_order_relaxed);

        // As above, wraparound is ok
        int available = (int) (writeptr - readptr);
        if (available >= 1) {
            result = true;
            reader(buffer_.data() + (readptr % buffer_.size()));
        }

        return result;
    }
    uint32_t size(void)
    {
        int writeptr = write_.load(std::memory_order_acquire);
        int readptr = read_.load(std::memory_order_relaxed);

        return (uint32_t) (writeptr - readptr);
    }

private:
    NON_COPYABLE(ProducerConsumerQueue);
    std::vector<T> buffer_;
    std::atomic<int> read_ {0};
    std::atomic<int> write_ {0};
};

struct sample_buf
{
    uint8_t* buf_ {nullptr}; // audio sample container
    size_t cap_ {0};         // buffer capacity in byte
    size_t size_ {0};        // audio sample size (n buf) in byte
    sample_buf() {}
    sample_buf(size_t alloc, size_t size)
        : buf_(new uint8_t[alloc])
        , cap_(size)
    {}
    sample_buf(size_t alloc)
        : buf_(new uint8_t[alloc])
        , cap_(alloc)
    {}
    sample_buf(sample_buf&& o)
        : buf_(o.buf_)
        , cap_(o.cap_)
        , size_(o.size_)
    {
        o.buf_ = nullptr;
        o.cap_ = 0;
        o.size_ = 0;
    }
    sample_buf& operator=(sample_buf&& o)
    {
        buf_ = o.buf_;
        cap_ = o.cap_;
        size_ = o.size_;
        o.buf_ = nullptr;
        o.cap_ = 0;
        o.size_ = 0;
        return *this;
    }

    ~sample_buf()
    {
        if (buf_)
            delete[] buf_;
    }
    NON_COPYABLE(sample_buf);
};

using AudioQueue = ProducerConsumerQueue<sample_buf*>;
