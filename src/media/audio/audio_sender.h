/*
 *  Copyright (C) 2018 Savoir-faire Linux Inc.
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

#include "audiobuffer.h"
#include "media_buffer.h"
#include "media_codec.h"
#include "noncopyable.h"
#include "observer.h"
#include "socket_pair.h"

namespace ring {

class AudioInput;
class MediaEncoder;
class MediaIOHandle;
class Resampler;

class AudioSender : public Observer<std::shared_ptr<AudioFrame>> {
public:
    AudioSender(const std::string& id,
                const std::string& dest,
                const MediaDescription& args,
                SocketPair& socketPair,
                const uint16_t seqVal,
                bool muteState,
                const uint16_t mtu);
    ~AudioSender();

    void setMuted(bool isMuted);
    uint16_t getLastSeqValue();

    void update(Observable<std::shared_ptr<ring::AudioFrame>>*,
                const std::shared_ptr<ring::AudioFrame>&) override;

private:
    NON_COPYABLE(AudioSender);

    bool setup(SocketPair& socketPair);

    std::string id_;
    std::string dest_;
    MediaDescription args_;
    std::unique_ptr<MediaEncoder> audioEncoder_;
    std::unique_ptr<MediaIOHandle> muxContext_;
    std::unique_ptr<Resampler> resampler_;
    std::shared_ptr<AudioInput> audioInput_;

    uint64_t sent_samples = 0;

    AudioBuffer micData_;
    AudioBuffer resampledData_;
    const uint16_t seqVal_;
    bool muteState_ = false;
    uint16_t mtu_;
};

} // namespace ring
