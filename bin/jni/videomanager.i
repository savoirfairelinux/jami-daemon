/*
 *  Copyright (C) 2015-2019 Savoir-faire Linux Inc.
 *
 *  Authors: Damien Riegel <damien.riegel@savoirfairelinux.com>
 *           Adrien Béraud <adrien.beraud@savoirfairelinux.com>
 *           Ciro Santilli <ciro.santilli@savoirfairelinux.com>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

%include <std_shared_ptr.i>
%header %{
#include <functional>
#include <list>
#include <mutex>
#include <utility>

#include "dring/dring.h"
#include "dring/videomanager_interface.h"
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/log.h>

extern "C" {
#include <libavutil/pixdesc.h>
#include <libavutil/imgutils.h>
#include <libavcodec/avcodec.h>
}

class VideoCallback {
public:
    virtual ~VideoCallback(){}
    virtual void getCameraInfo(const std::string& device, std::vector<int> *formats, std::vector<unsigned> *sizes, std::vector<unsigned> *rates) {}
    virtual void setParameters(const std::string, const int format, const int width, const int height, const int rate) {}
    virtual void requestKeyFrame(){}
    virtual void startCapture(const std::string& camid) {}
    virtual void stopCapture() {}
    virtual void decodingStarted(const std::string& id, const std::string& shm_path, int w, int h, bool is_mixer) {}
    virtual void decodingStopped(const std::string& id, const std::string& shm_path, bool is_mixer) {}
    virtual std::string startLocalRecorder(const bool& audioOnly, const std::string& filepath) {}
    virtual void stopLocalRecorder(const std::string& filepath) {}
    virtual bool getDecodingAccelerated() {}
    virtual void setDecodingAccelerated(bool state) {}
    virtual bool getEncodingAccelerated() {}
    virtual void setEncodingAccelerated(bool state) {}
};
%}

%feature("director") VideoCallback;

%{

std::map<ANativeWindow*, std::unique_ptr<DRing::FrameBuffer>> windows {};
std::mutex windows_mutex;

std::vector<uint8_t> workspace;
extern JavaVM *gJavaVM;

void rotateNV21(uint8_t* yinput, uint8_t* uvinput, unsigned ystride, unsigned uvstride, unsigned width, unsigned height, int rotation, uint8_t* youtput, uint8_t* uvoutput)
{
    if (rotation == 0) {
        std::copy_n(yinput, ystride * height, youtput);
        std::copy_n(uvinput, uvstride * height, uvoutput);
        return;
    }
    if (rotation % 90 != 0 || rotation < 0 || rotation > 270) {
        __android_log_print(ANDROID_LOG_ERROR, "videomanager.i", "%u %u %d", width, height, rotation);
        return;
    }
    bool swap      = rotation % 180 != 0;
    bool xflip     = rotation % 270 != 0;
    bool yflip     = rotation >= 180;
    unsigned wOut  = swap ? height : width;
    unsigned hOut  = swap ? width  : height;

    for (unsigned j = 0; j < height; j++) {
        for (unsigned i = 0; i < width; i++) {
            unsigned yIn = j * ystride + i;
            unsigned uIn = (j >> 1) * uvstride + (i & ~1);
            unsigned vIn = uIn + 1;
            unsigned iSwapped = swap ? j : i;
            unsigned jSwapped = swap ? i : j;
            unsigned iOut     = xflip ? wOut - iSwapped - 1 : iSwapped;
            unsigned jOut     = yflip ? hOut - jSwapped - 1 : jSwapped;
            unsigned yOut = jOut * wOut + iOut;
            unsigned uOut = (jOut >> 1) * wOut + (iOut & ~1);
            unsigned vOut = uOut + 1;
            youtput[yOut] = yinput[yIn];
            uvoutput[uOut] = uvinput[uIn];
            uvoutput[vOut] = uvinput[vIn];
        }
    }
    return;
}

JNIEXPORT void JNICALL Java_cx_ring_daemon_RingserviceJNI_setVideoFrame(JNIEnv *jenv, jclass jcls, jbyteArray frame, int frame_size, jlong target, int w, int h, int rotation)
{
    uint8_t* f_target = (uint8_t*) ((intptr_t) target);
    if (rotation == 0)
         jenv->GetByteArrayRegion(frame, 0, frame_size, (jbyte*)f_target);
    else {
        workspace.resize(frame_size);
        jenv->GetByteArrayRegion(frame, 0, frame_size, (jbyte*)workspace.data());
        auto planeSize = w*h;
        rotateNV21(workspace.data(), workspace.data() + planeSize, w, w, w, h, rotation, f_target, f_target + planeSize);
    }
}

int AndroidFormatToAVFormat(int androidformat) {
    switch (androidformat) {
    case 17: // ImageFormat.NV21
        return AV_PIX_FMT_NV21;
    case 35: // ImageFormat.YUV_420_888
        return AV_PIX_FMT_YUV420P;
    case 39: // ImageFormat.YUV_422_888
        return AV_PIX_FMT_YUV422P;
    case 41: // ImageFormat.FLEX_RGB_888
        return AV_PIX_FMT_GBRP;
    case 42: // ImageFormat.FLEX_RGBA_8888
        return AV_PIX_FMT_GBRAP;
    default:
        return AV_PIX_FMT_NONE;
    }
}

JNIEXPORT void JNICALL Java_cx_ring_daemon_RingserviceJNI_captureVideoPacket(JNIEnv *jenv, jclass jcls, jobject buffer, jint size, jint offset, jboolean keyframe, jlong timestamp)
{
    auto frame = DRing::getNewFrame();
    if (not frame)
        return;
    auto packet = std::unique_ptr<AVPacket, void(*)(AVPacket*)>(new AVPacket, [](AVPacket* pkt){
        if (pkt) {
            av_packet_unref(pkt);
            delete pkt;
        }
    });
    av_init_packet(packet.get());
    if (keyframe)
        packet->flags = AV_PKT_FLAG_KEY;
    auto data = (uint8_t*)jenv->GetDirectBufferAddress(buffer);
    packet->data = data + offset;
    packet->size = size;
    packet->pts = timestamp;
    frame->setPacket(std::move(packet));
    DRing::publishFrame();
}

JNIEXPORT void JNICALL Java_cx_ring_daemon_RingserviceJNI_captureVideoFrame(JNIEnv *jenv, jclass jcls, jobject image, jint rotation)
{
    jclass imageClass = jenv->GetObjectClass(image);
    auto frame = DRing::getNewFrame();
    if (not frame) {
        jenv->CallVoidMethod(image, jenv->GetMethodID(imageClass, "close", "()V"));
        return;
    }
    auto avframe = frame->pointer();

    avframe->format = AndroidFormatToAVFormat(jenv->CallIntMethod(image, jenv->GetMethodID(imageClass, "getFormat", "()I")));
    avframe->width = jenv->CallIntMethod(image, jenv->GetMethodID(imageClass, "getWidth", "()I"));
    avframe->height = jenv->CallIntMethod(image, jenv->GetMethodID(imageClass, "getHeight", "()I"));
    jobject crop = jenv->CallObjectMethod(image, jenv->GetMethodID(imageClass, "getCropRect", "()Landroid/graphics/Rect;"));
    if (crop) {
        jclass rectClass = jenv->GetObjectClass(crop);
        avframe->crop_top = jenv->GetIntField(crop, jenv->GetFieldID(rectClass, "top", "I"));
        avframe->crop_left = jenv->GetIntField(crop, jenv->GetFieldID(rectClass, "left", "I"));
        avframe->crop_bottom = avframe->height - jenv->GetIntField(crop, jenv->GetFieldID(rectClass, "bottom", "I"));
        avframe->crop_right = avframe->width - jenv->GetIntField(crop, jenv->GetFieldID(rectClass, "right", "I"));
    }

    bool directPointer = true;
    jobjectArray planes = (jobjectArray)jenv->CallObjectMethod(image, jenv->GetMethodID(imageClass, "getPlanes", "()[Landroid/media/Image$Plane;"));
    jsize planeCount = jenv->GetArrayLength(planes);
    if (avframe->format == AV_PIX_FMT_YUV420P) {
        jobject yplane = jenv->GetObjectArrayElement(planes, 0);
        jobject uplane = jenv->GetObjectArrayElement(planes, 1);
        jobject vplane = jenv->GetObjectArrayElement(planes, 2);
        jclass planeClass = jenv->GetObjectClass(yplane);
        jmethodID getBuffer = jenv->GetMethodID(planeClass, "getBuffer", "()Ljava/nio/ByteBuffer;");
        jmethodID getRowStride = jenv->GetMethodID(planeClass, "getRowStride", "()I");
        jmethodID getPixelStride = jenv->GetMethodID(planeClass, "getPixelStride", "()I");
        auto ydata = (uint8_t*)jenv->GetDirectBufferAddress(jenv->CallObjectMethod(yplane, getBuffer));
        auto udata = (uint8_t*)jenv->GetDirectBufferAddress(jenv->CallObjectMethod(uplane, getBuffer));
        auto vdata = (uint8_t*)jenv->GetDirectBufferAddress(jenv->CallObjectMethod(vplane, getBuffer));
        auto ystride = jenv->CallIntMethod(yplane, getRowStride);
        auto uvstride = jenv->CallIntMethod(uplane, getRowStride);
        auto uvpixstride = jenv->CallIntMethod(uplane, getPixelStride);

        if (uvpixstride == 1) {
            avframe->data[0] = ydata;
            avframe->linesize[0] = ystride;
            avframe->data[1] = udata;
            avframe->linesize[1] = uvstride;
            avframe->data[2] = vdata;
            avframe->linesize[2] = uvstride;
        } else if (uvpixstride == 2) {
            // False YUV422, actually NV12 or NV21
            auto uvdata = std::min(udata, vdata);
            avframe->format = uvdata == udata ? AV_PIX_FMT_NV12 : AV_PIX_FMT_NV21;
            if (rotation == 0) {
                avframe->data[0] = ydata;
                avframe->linesize[0] = ystride;
                avframe->data[1] = uvdata;
                avframe->linesize[1] = uvstride;
            } else {
                directPointer = false;
                bool swap = rotation != 0 && rotation != 180;
                auto ow = avframe->width;
                auto oh = avframe->height;
                avframe->width = swap ? oh : ow;
                avframe->height = swap ? ow : oh;
                av_frame_get_buffer(avframe, 1);
                rotateNV21(ydata, uvdata, ystride, uvstride, ow, oh, rotation, avframe->data[0], avframe->data[1]);
                jenv->CallVoidMethod(image, jenv->GetMethodID(imageClass, "close", "()V"));
            }
        }
    } else {
        for (int i=0; i<planeCount; i++) {
            jobject plane = jenv->GetObjectArrayElement(planes, i);
            jclass planeClass = jenv->GetObjectClass(plane);
            jint stride = jenv->CallIntMethod(plane, jenv->GetMethodID(planeClass, "getRowStride", "()I"));
            jint pxStride = jenv->CallIntMethod(plane, jenv->GetMethodID(planeClass, "getPixelStride", "()I"));
            jobject buffer = jenv->CallObjectMethod(plane, jenv->GetMethodID(planeClass, "getBuffer", "()Ljava/nio/ByteBuffer;"));
            avframe->data[i] = (uint8_t *)jenv->GetDirectBufferAddress(buffer);
            avframe->linesize[i] = stride;
        }
    }

    if (directPointer) {
        image = jenv->NewGlobalRef(image);
        imageClass = (jclass)jenv->NewGlobalRef(imageClass);
        frame->setReleaseCb([jenv, image, imageClass](uint8_t *) mutable {
            bool justAttached = false;
            int envStat = gJavaVM->GetEnv((void**)&jenv, JNI_VERSION_1_6);
            if (envStat == JNI_EDETACHED) {
                justAttached = true;
                if (gJavaVM->AttachCurrentThread(&jenv, nullptr) != 0)
                    return;
            } else if (envStat == JNI_EVERSION) {
                return;
            }
            jenv->CallVoidMethod(image, jenv->GetMethodID(imageClass, "close", "()V"));
            jenv->DeleteGlobalRef(image);
            jenv->DeleteGlobalRef(imageClass);
            if (justAttached)
                gJavaVM->DetachCurrentThread();
        });
    }
    DRing::publishFrame();
}

JNIEXPORT jlong JNICALL Java_cx_ring_daemon_RingserviceJNI_acquireNativeWindow(JNIEnv *jenv, jclass jcls, jobject javaSurface)
{
    return (jlong)(intptr_t)ANativeWindow_fromSurface(jenv, javaSurface);
}

JNIEXPORT void JNICALL Java_cx_ring_daemon_RingserviceJNI_releaseNativeWindow(JNIEnv *jenv, jclass jcls, jlong window_)
{
    std::lock_guard<std::mutex> guard(windows_mutex);
    ANativeWindow *window = (ANativeWindow*)((intptr_t) window_);
    ANativeWindow_release(window);
}

JNIEXPORT void JNICALL Java_cx_ring_daemon_RingserviceJNI_setNativeWindowGeometry(JNIEnv *jenv, jclass jcls, jlong window_, int width, int height)
{
    ANativeWindow *window = (ANativeWindow*)((intptr_t) window_);
    ANativeWindow_setBuffersGeometry(window, width, height, WINDOW_FORMAT_RGBA_8888);
}

void AndroidDisplayCb(ANativeWindow *window, std::unique_ptr<DRing::FrameBuffer> frame)
{
    std::lock_guard<std::mutex> guard(windows_mutex);
    try {
        auto& i = windows.at(window);
        ANativeWindow_Buffer buffer;
        if (ANativeWindow_lock(window, &buffer, NULL) == 0) {
            if (buffer.bits && frame && frame->ptr) {
                if (buffer.stride == frame->width)
                    memcpy(buffer.bits, frame->ptr, frame->width * frame->height * 4);
                else {
                    size_t line_size_in = frame->width * 4;
                    size_t line_size_out = buffer.stride * 4;
                    for (size_t i=0, n=frame->height; i<n; i++)
                        memcpy((uint8_t*)buffer.bits + line_size_out * i, frame->ptr + line_size_in * i, line_size_in);
                }
            }
            else
                __android_log_print(ANDROID_LOG_WARN, "videomanager.i", "Can't copy surface");
            ANativeWindow_unlockAndPost(window);
        } else {
            __android_log_print(ANDROID_LOG_WARN, "videomanager.i", "Can't lock surface");
        }
        i = std::move(frame);
    } catch (...) {
        __android_log_print(ANDROID_LOG_WARN, "videomanager.i", "Can't copy frame: no window");
    }
}

std::unique_ptr<DRing::FrameBuffer> sinkTargetPullCallback(ANativeWindow *window, std::size_t bytes)
{
    try {
        std::unique_ptr<DRing::FrameBuffer> ret;
        {
            std::lock_guard<std::mutex> guard(windows_mutex);
            ret = std::move(windows.at(window));
        }
        if (not ret) {
            __android_log_print(ANDROID_LOG_WARN, "videomanager.i", "Creating new video buffer of %zu kib", bytes/1024);
            ret.reset(new DRing::FrameBuffer());
        }
        ret->storage.resize(bytes);
        ret->ptr = ret->storage.data();
        ret->ptrSize = bytes;
        return ret;
    } catch (...) {
        return {};
    }
}

JNIEXPORT void JNICALL Java_cx_ring_daemon_RingserviceJNI_registerVideoCallback(JNIEnv *jenv, jclass jcls, jstring sinkId, jlong window)
{
    if(!sinkId) {
        SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null string");
        return;
    }
    const char *arg1_pstr = (const char *)jenv->GetStringUTFChars(sinkId, 0);
    if (!arg1_pstr)
        return;
    const std::string sink(arg1_pstr);
    jenv->ReleaseStringUTFChars(sinkId, arg1_pstr);

    ANativeWindow* nativeWindow = (ANativeWindow*)((intptr_t) window);
    auto f_display_cb = std::bind(&AndroidDisplayCb, nativeWindow, std::placeholders::_1);
    auto p_display_cb = std::bind(&sinkTargetPullCallback, nativeWindow, std::placeholders::_1);

    std::lock_guard<std::mutex> guard(windows_mutex);
    windows.emplace(nativeWindow, nullptr);
    DRing::registerSinkTarget(sink, DRing::SinkTarget {.pull=p_display_cb, .push=f_display_cb});
}

JNIEXPORT void JNICALL Java_cx_ring_daemon_RingserviceJNI_unregisterVideoCallback(JNIEnv *jenv, jclass jcls, jstring sinkId, jlong window)
{
    if(!sinkId) {
        SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null string");
        return;
    }
    const char *arg1_pstr = (const char *)jenv->GetStringUTFChars(sinkId, 0);
    if (!arg1_pstr)
        return;
    const std::string sink(arg1_pstr);
    jenv->ReleaseStringUTFChars(sinkId, arg1_pstr);

    std::lock_guard<std::mutex> guard(windows_mutex);
    DRing::registerSinkTarget(sink, DRing::SinkTarget {});
    ANativeWindow* nativeWindow = (ANativeWindow*)((intptr_t) window);
    windows.erase(nativeWindow);
}

%}
%native(setVideoFrame) void setVideoFrame(void*, int, jlong, int, int, int);
%native(acquireNativeWindow) jlong acquireNativeWindow(jobject);
%native(releaseNativeWindow) void releaseNativeWindow(jlong);
%native(setNativeWindowGeometry) void setNativeWindowGeometry(jlong, int, int);
%native(registerVideoCallback) void registerVideoCallback(jstring, jlong);
%native(unregisterVideoCallback) void unregisterVideoCallback(jstring, jlong);

%native(captureVideoFrame) void captureVideoFrame(jobject, jint);
%native(captureVideoPacket) void captureVideoPacket(jobject, jint, jint, jboolean, jlong);

namespace DRing {

void setDefaultDevice(const std::string& name);
std::string getDefaultDevice();

void startCamera();
void stopCamera();
bool hasCameraStarted();
bool switchInput(const std::string& resource);
bool switchToCamera();
std::map<std::string, std::string> getSettings(const std::string& name);
void applySettings(const std::string& name, const std::map<std::string, std::string>& settings);

void addVideoDevice(const std::string &node);
void removeVideoDevice(const std::string &node);
uint8_t* obtainFrame(int length);
void releaseFrame(uint8_t* frame);
void registerSinkTarget(const std::string& sinkId, const DRing::SinkTarget& target);
bool getDecodingAccelerated();
void setDecodingAccelerated(bool state);
bool getEncodingAccelerated();
void setEncodingAccelerated(bool state);
}

class VideoCallback {
public:
    virtual ~VideoCallback(){}
    virtual void getCameraInfo(const std::string& device, std::vector<int> *formats, std::vector<unsigned> *sizes, std::vector<unsigned> *rates){}
    virtual void setParameters(const std::string, const int format, const int width, const int height, const int rate) {}
    virtual void requestKeyFrame(){}
    virtual void startCapture(const std::string& camid) {}
    virtual void stopCapture() {}
    virtual void decodingStarted(const std::string& id, const std::string& shm_path, int w, int h, bool is_mixer) {}
    virtual void decodingStopped(const std::string& id, const std::string& shm_path, bool is_mixer) {}
};
