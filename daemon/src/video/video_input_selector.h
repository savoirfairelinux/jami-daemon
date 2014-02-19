/*
 *  Copyright (C) 2011-2014 Savoir-Faire Linux Inc.
 *  Author: Vivien Didelot <vivien.didelot@savoirfairelinux.com>
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

#ifndef __VIDEO_INPUT_SELECTOR_H__
#define __VIDEO_INPUT_SELECTOR_H__

#include "noncopyable.h"
#include "video_input.h"

#include <string>
#include <map>

namespace sfl_video {

class VideoInputSelector :
    public VideoFramePassiveReader,
    public VideoFrameActiveWriter
{
public:
    VideoInputSelector(const std::string& device);
    ~VideoInputSelector();

    /* as of VideoFrameActiveReader (Observer) */
    void update(Observable<std::shared_ptr<sfl_video::VideoFrame>>*, std::shared_ptr<VideoFrame>&);

    void switchInput(const std::string& device);

private:
    NON_COPYABLE(VideoInputSelector);

    void openInput(const std::string& device);
    void closeInput(void);

    VideoInput *currentInput_;
};

}

#endif // __VIDEO_INPUT_SELECTOR_H__
