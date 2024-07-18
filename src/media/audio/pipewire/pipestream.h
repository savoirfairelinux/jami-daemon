
#include <pipewire/pipewire.h>
#include <pipewire/stream.h>

class PipeWireStream 
{
public:
    PipeWireStream(pw_core* core, const char* name, AudioDeviceType type, const PwDeviceInfo& dev_info);
    ~PipeWireStream();

    void start();
    void stop();

    pw_stream* stream() { return stream_; }

    bool isReady() const;

private:
    pw_stream* stream_;
    AudioDeviceType type_;
    pw_stream_events stream_events_;

    static void streamStateChanged(void* data, enum pw_stream_state old, enum pw_stream_state state, const char* error);
    static void streamProcess(void* data);
};
