/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

#ifndef __LIBAV_UTILS_H__
#define __LIBAV_UTILS_H__

#include <vector>
#include <map>
#include <string>


namespace libav_utils {
    void sfl_avcodec_init();

    int libav_pixel_format(int fmt);
    int sfl_pixel_format(int fmt);

    std::map<std::string, std::string> encodersMap();

    std::vector<std::string> getVideoCodecList();

    std::vector<std::map <std::string, std::string> > getDefaultCodecs();

    const char *const DEFAULT_H264_PROFILE_LEVEL_ID = "profile-level-id=428014";
    const char *const MAX_H264_PROFILE_LEVEL_ID = "profile-level-id=640034";

    void sfl_url_split(const char *url,
                      char *hostname, size_t hostname_size, int *port,
                      char *path, size_t path_size);
}

#endif // __LIBAV_UTILS_H__
