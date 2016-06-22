#include "libav_deps.h" // MUST BE INCLUDED FIRST

#include "config.h"

#if defined(RING_VIDEO) && defined(USE_HWACCEL)

extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <va/va.h>
#ifdef HAVE_VAAPI_DRM
#   include <va/va_drm.h>
#endif
#ifdef HAVE_VAAPI_X11
#   include <va/va_x11.h>
#endif

#include <libavutil/avconfig.h>
#include <libavutil/buffer.h>
#include <libavutil/frame.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vaapi.h>

#include <libavcodec/vaapi.h>
}

#include "video/hwaccel.h"

namespace ring { namespace video {

class VaapiAccel : public HardwareAccel {
    public:
        VaapiAccel();
        ~VaapiAccel();

        void init(AVCodecContext* avctx) override;
        int getBuffer(AVCodecContext* avctx, AVFrame* frame, int flags) override;
        int retrieveData(AVCodecContext* avctx, AVFrame* input) override;

    private:
        void deviceInit(const char* device);
        void config(AVCodecContext* avctx);

        AVBufferRef       *device_ref;
        AVHWDeviceContext *device;
        AVBufferRef       *frames_ref;
        AVHWFramesContext *frames;

        VAProfile    va_profile;
        VAEntrypoint va_entrypoint;
        VAConfigID   va_config;
        VAContextID  va_context;

        enum AVPixelFormat decode_format;
        int decode_width;
        int decode_height;
        int decode_surfaces;

        enum AVPixelFormat output_format;

        struct vaapi_context decoder_vaapi_context;

        AVBufferRef *hw_device_ctx;
        AVBufferRef *hw_frames_ctx;

        // typedef struct VAAPIDecoderContext {
        //     const AVClass *av_class;
        //
        //     AVBufferRef       *device_ref;
        //     AVHWDeviceContext *device;
        //     AVBufferRef       *frames_ref;
        //     AVHWFramesContext *frames;
        //
        //     VAProfile    va_profile;
        //     VAEntrypoint va_entrypoint;
        //     VAConfigID   va_config;
        //     VAContextID  va_context;
        //
        //     enum AVPixelFormat decode_format;
        //     int decode_width;
        //     int decode_height;
        //     int decode_surfaces;
        //
        //     // The output need not have the same format, width and height as the
        //     // decoded frames - the copy for non-direct-mapped access is actually
        //     // a whole vpp instance which can do arbitrary scaling and format
        //     // conversion.
        //     enum AVPixelFormat output_format;
        //
        //     struct vaapi_context decoder_vaapi_context;
        // } VAAPIDecoderContext;
};

}} // namespace ring::video

#endif // defined(RING_VIDEO) && defined(USE_HWACCEL)
