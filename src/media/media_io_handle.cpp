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

#include "libav_deps.h" // MUST BE INCLUDED FIRST
#include "media_io_handle.h"

namespace jami {

MediaIOHandle::MediaIOHandle(std::size_t buffer_size,
                             bool writeable,
                             io_readcallback read_cb,
                             io_writecallback write_cb,
                             io_seekcallback seek_cb,
                             void* opaque)
    : ctx_(0)

{
    /* FFmpeg doesn't alloc the buffer for the first time, but it does free and
     * alloc it afterwards.
     * Don't directly use malloc because av_malloc is optimized for memory alignment.
     */
    auto buf = static_cast<uint8_t*>(av_malloc(buffer_size));
    ctx_ = avio_alloc_context(buf, buffer_size, writeable, opaque, read_cb, write_cb, seek_cb);
    ctx_->max_packet_size = buffer_size;
}

MediaIOHandle::~MediaIOHandle()
{
    av_free(ctx_->buffer);
    av_free(ctx_);
}

} // namespace jami
