/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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

#include "video_sender.h"
#include "video_mixer.h"
#include "socket_pair.h"
#include "client/videomanager.h"
#include "logger.h"
#include "manager.h"

#include <map>
#include <unistd.h>


namespace sfl_video {

using std::string;

VideoSender::VideoSender(std::map<string, string> args,
                         SocketPair& socketPair) :
    muxContext_(socketPair.createIOContext()),
    videoEncoder_(new VideoEncoder)
{
    const char *enc_name = args["codec"].c_str();
    const char *dest = args["destination"].c_str();

    /* Encoder setup (may throw VideoEncoderException) */
    videoEncoder_->setOptions(args);
    videoEncoder_->openOutput(enc_name, "rtp", dest, NULL);
    videoEncoder_->setIOContext(muxContext_);
    videoEncoder_->startIO();

    videoEncoder_->print_sdp(sdp_);
}

void VideoSender::encodeAndSendVideo(VideoFrame& input_frame)
{
    bool is_keyframe = forceKeyFrame_ > 0;

    if (is_keyframe)
        --forceKeyFrame_;

    if (videoEncoder_->encode(input_frame, is_keyframe, frameNumber_++) < 0)
        ERROR("encoding failed");
}

void VideoSender::update(Observable<std::shared_ptr<VideoFrame> >* /*obs*/,
                         std::shared_ptr<VideoFrame> & frame_p)
{
    encodeAndSendVideo(*frame_p);
}

void VideoSender::forceKeyFrame()
{
    ++forceKeyFrame_;
}

} // end namespace sfl_video
