/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
 *  Author: Philippe Gorley <philippe.gorley@savoirfairelinux.com>
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

#include <vector>
#include <map>
#include <string>

extern "C" {
struct AVDictionary;
struct AVFrame;
struct AVPixFmtDescriptor;
}

namespace ring { namespace libav_utils {

    void ring_avcodec_init();

    const char *const DEFAULT_H264_PROFILE_LEVEL_ID = "profile-level-id=428029";
    const char *const MAX_H264_PROFILE_LEVEL_ID = "profile-level-id=640034";

    void ring_url_split(const char *url,
                      char *hostname, size_t hostname_size, int *port,
                      char *path, size_t path_size);

    bool is_yuv_planar(const AVPixFmtDescriptor& desc);

    std::string getError(int err);

    const char* getDictValue(const AVDictionary* d, const std::string& key, int flags=0);

    void setDictValue(AVDictionary** d, const std::string& key, const std::string& value, int flags=0);

    void fillWithBlack(AVFrame* frame);

    void fillWithSilence(AVFrame* frame);

}} // namespace ring::libav_utils
