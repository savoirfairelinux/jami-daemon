
/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#include "pipelayer.h"

#include <pipewire/pipewire.h>
#include <pipewire/stream.h>

namespace jami {

struct PwDeviceInfo 
{
    uint32_t id;
    std::string name;
    std::string description;
    uint32_t channels;
    uint32_t rate;
    pw_stream_flags flags;
};

class PipeWireStream 
{
public:
    PipeWireStream(
        pw_thread_loop* loop,
        pw_core* core,
        const char* name,
        AudioDeviceType type,
        const PwDeviceInfo& dev_info,
        std::function<void(pw_buffer* buf)>&& onData,
        std::function<void(const AudioFormat&)>&& onFormatChange,
        std::function<void()>&& onReady);
    ~PipeWireStream();

    void start();
    void stop();

    pw_stream* stream() { return stream_; }

    bool isReady() const;

    inline size_t sampleSize() const { return av_get_bytes_per_sample(format_.sampleFormat) ; }
    inline size_t frameSize() const { return format_.getBytesPerFrame(); }

    inline uint8_t channels() const { return format_.nb_channels; }

    inline AudioFormat format() const
    {
        return format_;
    }


private:
    bool ready_ {false};
    pw_thread_loop* loop_;
    pw_stream* stream_;
    AudioDeviceType type_;
    pw_stream_events stream_events_;
    spa_hook streamListener_;
    std::function<void(pw_buffer* buf)> onData_;
    std::function<void(const AudioFormat&)> onFormatChange_;
    std::function<void()> onReady_;
    AudioFormat format_;

    static void streamStateChanged(void* data, enum pw_stream_state old, enum pw_stream_state state, const char* error);
    static void streamParamChanged(void* data, uint32_t id, const struct spa_pod* param);
    static void streamProcess(void* data);
};

} // namespace jami