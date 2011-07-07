/*
 *  Copyright (C) 2011 Savoir-Faire Linux Inc.
 *  Author: Rafaël Carré <rafael.carre@savoirfairelinux.com>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

#ifndef __VIDEO_PICTURE_H__
#define __VIDEO_PICTURE_H__

namespace sfl_video {

class VideoPicture {
    public:
        VideoPicture(const unsigned bpp, const unsigned width, const unsigned height, const uint64_t pts) : bpp(bpp), width(width), height(height), pts(pts) {
            // FIXME : stride must be equal to width * bpp
            data = malloc(Size());
        }

        void* data; /** raw image data  */

        const unsigned bpp;     /** bits per pixel  */
        const unsigned width;   /** image width     */
        const unsigned height;  /** image height    */
        const uint64_t pts;     /** presentation timestamp */

        size_t Size() const { return (bpp >> 3) * width * height; }
};
}

#endif // __VIDEO_PICTURE_H__
