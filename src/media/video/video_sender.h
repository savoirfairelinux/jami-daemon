/*
 *  Copyright (C) 2011-2019 Savoir-faire Linux Inc.
 *
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
 */

#ifndef __VIDEO_SENDER_H__
#define __VIDEO_SENDER_H__

#include "noncopyable.h"
#include "media_encoder.h"
#include "media_io_handle.h"

#include <map>
#include <string>
#include <memory>
#include <atomic>

// Forward declarations
namespace ring {
class SocketPair;
struct AccountVideoCodecInfo;
}

namespace ring { namespace video {

class VideoSender : public VideoFramePassiveReader
{
public:
    VideoSender(const std::string& dest,
                const DeviceParams& dev,
                const MediaDescription& args,
                SocketPair& socketPair,
                const uint16_t seqVal,
                uint16_t mtu);

    ~VideoSender();

    void forceKeyFrame();

    // as VideoFramePassiveReader
    void update(Observable<std::shared_ptr<MediaFrame>>* obs,
                const std::shared_ptr<MediaFrame>& frame_p) override;

    uint16_t getLastSeqValue();

    bool useCodec(const AccountVideoCodecInfo* codec) const;

    void setChangeOrientationCallback(std::function<void(int)> cb);

private:
    static constexpr int KEYFRAMES_AT_START {4}; // Number of keyframes to enforce at stream startup
    static constexpr unsigned KEY_FRAME_PERIOD {0}; // seconds before forcing a keyframe

    NON_COPYABLE(VideoSender);

    void encodeAndSendVideo(VideoFrame&);

    // encoder MUST be deleted before muxContext
    std::unique_ptr<MediaIOHandle> muxContext_ = nullptr;
    std::unique_ptr<MediaEncoder> videoEncoder_ = nullptr;

    // XXX forceKeyFrame_ is always at -1, incremented to 0 when a keyframe is requested (still works though)
    std::atomic<int> forceKeyFrame_ {KEYFRAMES_AT_START};
    int keyFrameFreq_ {0}; // Set keyframe rate, 0 to disable auto-keyframe. Computed in constructor
    int64_t frameNumber_ = 0;

    int rotation_ = 0;
    std::function<void(int)> changeOrientationCallback_;
};
}} // namespace ring::video

#endif // __VIDEO_SENDER_H__
