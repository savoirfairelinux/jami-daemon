/*
 *  Copyright (C) 2011-2015 Savoir-Faire Linux Inc.
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

#ifndef __VIDEO_SENDER_H__
#define __VIDEO_SENDER_H__

#include "noncopyable.h"
#include "media_encoder.h"
#include "media_io_handle.h"
#include "video_mixer.h"

#include <map>
#include <string>
#include <memory>
#include <atomic>

#define PACKET_LOSS_THRESHOLD 1.0
#define PACKET_LOSS_CHECKING_FREQUENCY 100
// Forward declarations
namespace ring {
class SocketPair;
class AccountVideoCodecInfo;
}

namespace ring { namespace video {

class VideoSender : public VideoFramePassiveReader
{
public:
    VideoSender(const std::string& dest,
                const DeviceParams& dev,
                const MediaDescription& args,
                SocketPair& socketPair,
                const uint16_t seqVal);

    ~VideoSender();

    std::string getSDP() const { return sdp_; }
    void forceKeyFrame();

    // as VideoFramePassiveReader
    void update(Observable<std::shared_ptr<VideoFrame> >* obs,
                std::shared_ptr<VideoFrame> &);

    void setMuted(bool isMuted);
    uint16_t getLastSeqValue();

    bool useCodec(const AccountVideoCodecInfo* codec) const;

private:
    NON_COPYABLE(VideoSender);

    void encodeAndSendVideo(VideoFrame&);
    bool checkPeerPacketLoss();

    // encoder MUST be deleted before muxContext
    std::unique_ptr<MediaIOHandle> muxContext_ = nullptr;
    std::unique_ptr<MediaEncoder> videoEncoder_ = nullptr;

    std::atomic<int> forceKeyFrame_ = { 0 };
    int64_t frameNumber_ = 0;
    std::string sdp_ = "";
    SocketPair& socketPair_;
    unsigned cptBitrateChecking_ = 1;
};

}} // namespace ring::video

#endif // __VIDEO_SENDER_H__
