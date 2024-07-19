
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

class PipeWireStream 
{
public:
    PipeWireStream(pw_core* core, const char* name, AudioDeviceType type, const PwDeviceInfo& dev_info, std::function<void(pw_buffer* buf)>&& onData, std::function<void()>&& onReady);
    ~PipeWireStream();

    void start();
    void stop();

    pw_stream* stream() { return stream_; }

    bool isReady() const;

private:
    pw_stream* stream_;
    AudioDeviceType type_;
    pw_stream_events stream_events_;
    pw_stream_listener streamListener_;
    std::function<void()> onData_;
    std::function<void()> onReady_;

    static void streamStateChanged(void* data, enum pw_stream_state old, enum pw_stream_state state, const char* error);
    static void streamProcess(void* data);
};

} // namespace jami