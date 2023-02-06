/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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
 */

#ifndef __MEDIA_IO_HANDLE_H__
#define __MEDIA_IO_HANDLE_H__

#include "noncopyable.h"

#include <cstdlib>
#include <cstdint>
#include <vector>

#ifndef AVFORMAT_AVIO_H
struct AVIOContext;
#endif

typedef int (*io_readcallback)(void* opaque, uint8_t* buf, int buf_size);
typedef int (*io_writecallback)(void* opaque, uint8_t* buf, int buf_size);
typedef int64_t (*io_seekcallback)(void* opaque, int64_t offset, int whence);

namespace jami {

class MediaIOHandle
{
public:
    MediaIOHandle(std::size_t buffer_size,
                  bool writeable,
                  io_readcallback read_cb,
                  io_writecallback write_cb,
                  io_seekcallback seek_cb,
                  void* opaque);
    ~MediaIOHandle();

    AVIOContext* getContext() { return ctx_; }

private:
    NON_COPYABLE(MediaIOHandle);
    AVIOContext* ctx_;
};

} // namespace jami

#endif // __MEDIA_DECODER_H__
