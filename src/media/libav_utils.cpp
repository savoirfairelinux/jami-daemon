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

#include "libav_deps.h" // MUST BE INCLUDED FIRST

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "video/video_base.h"
#include "logger.h"

#include <vector>
#include <algorithm>
#include <string>
#include <iostream>
#include <thread>
#include <mutex>
#include <exception>
#include <ciso646> // fix windows compiler bug

extern "C" {
#if LIBAVUTIL_VERSION_MAJOR < 56
AVFrameSideData*
av_frame_new_side_data_from_buf(AVFrame* frame, enum AVFrameSideDataType type, AVBufferRef* buf)
{
    auto side_data = av_frame_new_side_data(frame, type, 0);
    av_buffer_unref(&side_data->buf);
    side_data->buf = buf;
    side_data->data = side_data->buf->data;
    side_data->size = side_data->buf->size;
    return side_data;
}
#endif
}

namespace jami {
namespace libav_utils {

AVSampleFormat
choose_sample_fmt(const AVCodec* codec,
                  const AVSampleFormat* preferred_formats,
                  int preferred_formats_count)
{
    if (codec->sample_fmts)
        for (int i = 0; i < preferred_formats_count; ++i) {
            for (auto it = codec->sample_fmts; *it != -1; ++it) {
                if (*it == preferred_formats[i])
                    return preferred_formats[i];
            }
        }
    return AV_SAMPLE_FMT_NONE;
}

AVSampleFormat
choose_sample_fmt_default(const AVCodec* codec, AVSampleFormat defaultFormat)
{
    // List of supported formats, current default first
    const AVSampleFormat preferred_formats[] = {defaultFormat,
                                                AV_SAMPLE_FMT_FLTP,
                                                AV_SAMPLE_FMT_FLT,
                                                AV_SAMPLE_FMT_S16P,
                                                AV_SAMPLE_FMT_S16,
                                                AV_SAMPLE_FMT_DBLP,
                                                AV_SAMPLE_FMT_DBL,
                                                AV_SAMPLE_FMT_S32P,
                                                AV_SAMPLE_FMT_S32};
    return choose_sample_fmt(codec,
                             preferred_formats,
                             sizeof(preferred_formats) / sizeof(preferred_formats[0]));
}

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
// protect libav/ffmpeg access
static int
avcodecManageMutex(void** data, enum AVLockOp op)
{
    auto mutex = reinterpret_cast<std::mutex**>(data);
    int ret = 0;
    switch (op) {
    case AV_LOCK_CREATE:
        try {
            *mutex = new std::mutex;
        } catch (const std::bad_alloc& e) {
            return AVERROR(ENOMEM);
        }
        break;
    case AV_LOCK_OBTAIN:
        (*mutex)->lock();
        break;
    case AV_LOCK_RELEASE:
        (*mutex)->unlock();
        break;
    case AV_LOCK_DESTROY:
        delete *mutex;
        *mutex = nullptr;
        break;
    default:
#ifdef AVERROR_BUG
        return AVERROR_BUG;
#else
        break;
#endif
    }
    return AVERROR(ret);
}
#endif

static constexpr const char* AVLOGLEVEL = "AVLOGLEVEL";

static void
setAvLogLevel()
{
    char* envvar = getenv(AVLOGLEVEL);
    signed level = AV_LOG_WARNING;

    if (envvar != nullptr) {
        level = to_int<int>(envvar, AV_LOG_ERROR);
        level = std::max(AV_LOG_QUIET, std::min(level, AV_LOG_DEBUG));
    }
    av_log_set_level(level);
}

#ifdef __ANDROID__
static void
androidAvLogCb(void* ptr, int level, const char* fmt, va_list vl)
{
    if (level > av_log_get_level())
        return;

    char line[1024];
    int print_prefix = 1;
    int android_level;
    va_list vl2;
    va_copy(vl2, vl);
    av_log_format_line(ptr, level, fmt, vl2, line, sizeof(line), &print_prefix);
    va_end(vl2);

    // replace unprintable characters with '?'
    int idx = 0;
    while (line[idx]) {
        if (line[idx] < 0x08 || (line[idx] > 0x0D && line[idx] < 0x20))
            line[idx] = '?';
        ++idx;
    }

    switch (level) {
    case AV_LOG_QUIET:
        android_level = ANDROID_LOG_SILENT;
        break;
    case AV_LOG_PANIC:
        android_level = ANDROID_LOG_FATAL;
        break;
    case AV_LOG_FATAL:
        android_level = ANDROID_LOG_FATAL;
        break;
    case AV_LOG_ERROR:
        android_level = ANDROID_LOG_ERROR;
        break;
    case AV_LOG_WARNING:
        android_level = ANDROID_LOG_WARN;
        break;
    case AV_LOG_INFO:
        android_level = ANDROID_LOG_INFO;
        break;
    case AV_LOG_VERBOSE:
        android_level = ANDROID_LOG_INFO;
        break;
    case AV_LOG_DEBUG:
        android_level = ANDROID_LOG_DEBUG;
        break;
    case AV_LOG_TRACE:
        android_level = ANDROID_LOG_VERBOSE;
        break;
    default:
        android_level = ANDROID_LOG_DEFAULT;
        break;
    }
    __android_log_print(android_level, "FFmpeg", "%s", line);
}
#endif

static void
init_once()
{
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(58, 9, 100)
    av_register_all();
#endif
    avdevice_register_all();
    avformat_network_init();
#if LIBAVFILTER_VERSION_INT < AV_VERSION_INT(7, 13, 100)
    avfilter_register_all();
#endif

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
    av_lockmgr_register(avcodecManageMutex);
#endif

    setAvLogLevel();

#ifdef __ANDROID__
    // android doesn't like stdout and stderr :(
    av_log_set_callback(androidAvLogCb);
#endif
}

static std::once_flag already_called;

void
av_init()
{
    std::call_once(already_called, init_once);
}

bool
is_yuv_planar(const AVPixFmtDescriptor& desc)
{
    if (not(desc.flags & AV_PIX_FMT_FLAG_PLANAR) or desc.flags & AV_PIX_FMT_FLAG_RGB)
        return false;

    /* handle formats that do not use all planes */
    unsigned used_bit_mask = (1u << desc.nb_components) - 1;
    for (unsigned i = 0; i < desc.nb_components; ++i)
        used_bit_mask &= ~(1u << desc.comp[i].plane);

    return not used_bit_mask;
}

std::string
getError(int err)
{
    std::string ret(AV_ERROR_MAX_STRING_SIZE, '\0');
    av_strerror(err, (char*) ret.data(), ret.size());
    return ret;
}

const char*
getDictValue(const AVDictionary* d, const std::string& key, int flags)
{
    auto kv = av_dict_get(d, key.c_str(), nullptr, flags);
    if (kv)
        return kv->value;
    else
        return "";
}

void
setDictValue(AVDictionary** d, const std::string& key, const std::string& value, int flags)
{
    av_dict_set(d, key.c_str(), value.c_str(), flags);
}

void
fillWithBlack(AVFrame* frame)
{
    const AVPixelFormat format = static_cast<AVPixelFormat>(frame->format);
    const int planes = av_pix_fmt_count_planes(format);
    // workaround for casting pointers to different sizes
    // on 64 bit machines: sizeof(ptrdiff_t) != sizeof(int)
    ptrdiff_t linesizes[4];
    for (int i = 0; i < planes; ++i)
        linesizes[i] = frame->linesize[i];
    int ret = av_image_fill_black(frame->data,
                                  linesizes,
                                  format,
                                  frame->color_range,
                                  frame->width,
                                  frame->height);
    if (ret < 0) {
        JAMI_ERR() << "Failed to blacken frame";
    }
}

void
fillWithSilence(AVFrame* frame)
{
    int ret = av_samples_set_silence(frame->extended_data,
                                     0,
                                     frame->nb_samples,
                                     frame->ch_layout.nb_channels,
                                     (AVSampleFormat) frame->format);
    if (ret < 0)
        JAMI_ERR() << "Failed to fill frame with silence";
}

} // namespace libav_utils
} // namespace jami
