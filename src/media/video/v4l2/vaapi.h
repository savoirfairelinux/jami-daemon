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

#include <stdexcept>

#include "video/hwaccel.h"

namespace ring { namespace video {

const struct {
    enum AVCodecID codecId;
    int codecProfile;
    VAProfile vaProfile;
} vaapiProfileMap[] = {
#define MAP(c, p, v) { AV_CODEC_ID_ ## c, FF_PROFILE_ ## p, VAProfile ## v }
    MAP(H263,        UNKNOWN,         H263Baseline),
    MAP(MPEG4,       MPEG4_SIMPLE,    MPEG4Simple ),
    MAP(MPEG4,       MPEG4_ADVANCED_SIMPLE,
                               MPEG4AdvancedSimple),
    MAP(MPEG4,       MPEG4_MAIN,      MPEG4Main   ),
    MAP(H264,        H264_CONSTRAINED_BASELINE,
                           H264ConstrainedBaseline),
    MAP(H264,        H264_BASELINE,   H264Baseline),
    MAP(H264,        H264_MAIN,       H264Main    ),
    MAP(H264,        H264_HIGH,       H264High    ),
#if VA_CHECK_VERSION(0, 37, 0)
    MAP(HEVC,        HEVC_MAIN,       HEVCMain    ),
#endif
#if VA_CHECK_VERSION(0, 35, 0)
    MAP(VP8,         UNKNOWN,       VP8Version0_3 ),
#endif
#undef MAP
};

class VaapiAccel : public HardwareAccel {
    public:
        VaapiAccel();
        ~VaapiAccel();

        void init(AVCodecContext* codecCtx) override;
        int getBuffer(AVFrame* frame, int flags) override;
        int retrieveData(AVFrame* frame) override;

    private:
        AVCodecContext* codecCtx_;
        AVBufferRef* deviceRef_;
        AVHWDeviceContext* device_;
        AVBufferRef* framesRef_;
        AVHWFramesContext* frames_;

        VAProfile vaProfile_;
        VAEntrypoint vaEntrypoint_;
        VAConfigID vaConfig_;
        VAContextID vaContext_;

        enum AVPixelFormat decodeFmt_;
        int decodeWidth_;
        int decodeHeight_;
        int decodeSurfaces_;

        enum AVPixelFormat outputFmt_;
        struct vaapi_context decoderVaapiContext_;

        AVBufferRef* deviceInit(const char *device, int *err);
        void buildDecoderConfig(int fallbackAllowed);
};

}} // namespace ring::video

#endif // defined(RING_VIDEO) && defined(USE_HWACCEL)
