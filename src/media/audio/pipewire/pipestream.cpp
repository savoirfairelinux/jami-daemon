#include "pipestream.h"

PipeWireStream::PipeWireStream(pw_core* core, const char* name, AudioDeviceType type, const PwDeviceInfo& dev_info)
    : type_(type)
{
    pw_properties* props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio",
                                             PW_KEY_MEDIA_CATEGORY, type == AudioDeviceType::CAPTURE ? "Capture" : "Playback",
                                             PW_KEY_APP_NAME, name,
                                             nullptr);

    stream_ = pw_stream_new(core, name, props);
    pw_stream_add_listener(stream_, &streamListener_, &stream_events_, this);

    // Configure and connect the stream based on dev_info
}

void PipeWireStream::start() 
{
    pw_stream_set_active(stream_, true);
}

void PipeWireStream::stop() 
{
    pw_stream_set_active(stream_, false);
}

void PipeWireStream::streamProcess(void* data) 
{
    PipeWireStream* self = static_cast<PipeWireStream*>(data);
    // Handle audio data processing here
}