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
#include <libavutil/display.h>
#include <libavcodec/avcodec.h>
}

class VideoCallback {
public:
    virtual ~VideoCallback(){}
    virtual void getCameraInfo(const std::string& device, std::vector<int> *formats, std::vector<unsigned> *sizes, std::vector<unsigned> *rates) {}
    virtual void setParameters(const std::string, const int format, const int width, const int height, const int rate) {}
    virtual void setBitrate(const std::string, const int bitrate) {}
    virtual void requestKeyFrame(){}
    virtual void startCapture(const std::string& camid) {}
    virtual void stopCapture() {}
    virtual void decodingStarted(const std::string& id, const std::string& shm_path, int w, int h, bool is_mixer) {}
    virtual void decodingStopped(const std::string& id, const std::string& shm_path, bool is_mixer) {}
};
%}

%feature("director") VideoCallback;

%{

std::map<ANativeWindow*, std::unique_ptr<DRing::FrameBuffer>> windows {};
std::mutex windows_mutex;

std::vector<uint8_t> workspace;
int rotAngle = 0;
AVBufferRef* rotMatrix = nullptr;

extern JavaVM *gJavaVM;

void setRotation(int angle)
{
    if (angle == rotAngle)
        return;
    AVBufferRef* localFrameDataBuffer = angle == 0 ? nullptr : av_buffer_alloc(sizeof(int32_t) * 9);
    if (localFrameDataBuffer)
        av_display_rotation_set(reinterpret_cast<int32_t*>(localFrameDataBuffer->data), angle);

    std::swap(rotMatrix, localFrameDataBuffer);
    rotAngle = angle;

    av_buffer_unref(&localFrameDataBuffer);
}

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

JNIEXPORT void JNICALL Java_cx_ring_daemon_RingserviceJNI_captureVideoPacket(JNIEnv *jenv, jclass jcls, jobject buffer, jint size, jint offset, jboolean keyframe, jlong timestamp, jint rotation)
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
    setRotation(rotation);
    if (rotMatrix) {
        auto buf = av_packet_new_side_data(packet.get(), AV_PKT_DATA_DISPLAYMATRIX, rotMatrix->size);
        std::copy_n(rotMatrix->data, rotMatrix->size, buf);
    }
    auto data = (uint8_t*)jenv->GetDirectBufferAddress(buffer);
    packet->data = data + offset;
    packet->size = size;
    packet->pts = timestamp;
    frame->setPacket(std::move(packet));
    DRing::publishFrame();
}

JNIEXPORT void JNICALL Java_cx_ring_daemon_RingserviceJNI_captureVideoFrame(JNIEnv *jenv, jclass jcls, jobject image, jint rotation)
{
    static jclass imageClass = jenv->GetObjectClass(image);
    static jmethodID imageGetFormat = jenv->GetMethodID(imageClass, "getFormat", "()I");
    static jmethodID imageGetWidth = jenv->GetMethodID(imageClass, "getWidth", "()I");
    static jmethodID imageGetHeight = jenv->GetMethodID(imageClass, "getHeight", "()I");
    static jmethodID imageGetCropRect = jenv->GetMethodID(imageClass, "getCropRect", "()Landroid/graphics/Rect;");
    static jmethodID imageGetPlanes = jenv->GetMethodID(imageClass, "getPlanes", "()[Landroid/media/Image$Plane;");
    static jmethodID imageClose = jenv->GetMethodID(imageClass, "close", "()V");

    auto frame = DRing::getNewFrame();
    if (not frame) {
        jenv->CallVoidMethod(image, imageClose);
        return;
    }
    auto avframe = frame->pointer();

    avframe->format = AndroidFormatToAVFormat(jenv->CallIntMethod(image, imageGetFormat));
    avframe->width = jenv->CallIntMethod(image, imageGetWidth);
    avframe->height = jenv->CallIntMethod(image, imageGetHeight);
    jobject crop = jenv->CallObjectMethod(image, imageGetCropRect);
    if (crop) {
        static jclass rectClass = jenv->GetObjectClass(crop);
        static jfieldID rectTopField = jenv->GetFieldID(rectClass, "top", "I");
        static jfieldID rectLeftField = jenv->GetFieldID(rectClass, "left", "I");
        static jfieldID rectBottomField = jenv->GetFieldID(rectClass, "bottom", "I");
        static jfieldID rectRightField = jenv->GetFieldID(rectClass, "right", "I");
        avframe->crop_top = jenv->GetIntField(crop, rectTopField);
        avframe->crop_left = jenv->GetIntField(crop, rectLeftField);
        avframe->crop_bottom = avframe->height - jenv->GetIntField(crop, rectBottomField);
        avframe->crop_right = avframe->width - jenv->GetIntField(crop, rectRightField);
    }

    jobjectArray planes = (jobjectArray)jenv->CallObjectMethod(image, imageGetPlanes);
    static jclass planeClass = jenv->GetObjectClass(jenv->GetObjectArrayElement(planes, 0));
    static jmethodID planeGetBuffer = jenv->GetMethodID(planeClass, "getBuffer", "()Ljava/nio/ByteBuffer;");
    static jmethodID planeGetRowStride = jenv->GetMethodID(planeClass, "getRowStride", "()I");
    static jmethodID planeGetPixelStride = jenv->GetMethodID(planeClass, "getPixelStride", "()I");

    jsize planeCount = jenv->GetArrayLength(planes);
    if (avframe->format == AV_PIX_FMT_YUV420P) {
        jobject yplane = jenv->GetObjectArrayElement(planes, 0);
        jobject uplane = jenv->GetObjectArrayElement(planes, 1);
        jobject vplane = jenv->GetObjectArrayElement(planes, 2);
        auto ydata = (uint8_t*)jenv->GetDirectBufferAddress(jenv->CallObjectMethod(yplane, planeGetBuffer));
        auto udata = (uint8_t*)jenv->GetDirectBufferAddress(jenv->CallObjectMethod(uplane, planeGetBuffer));
        auto vdata = (uint8_t*)jenv->GetDirectBufferAddress(jenv->CallObjectMethod(vplane, planeGetBuffer));
        auto ystride = jenv->CallIntMethod(yplane, planeGetRowStride);
        auto uvstride = jenv->CallIntMethod(uplane, planeGetRowStride);
        auto uvpixstride = jenv->CallIntMethod(uplane, planeGetPixelStride);

        if (uvpixstride == 1) {
            avframe->data[0] = ydata;
            avframe->linesize[0] = ystride;
            avframe->data[1] = udata;
            avframe->linesize[1] = uvstride;
            avframe->data[2] = vdata;
            avframe->linesize[2] = uvstride;
        } else if (uvpixstride == 2) {
            // False YUV420, actually NV12 or NV21
            auto uvdata = std::min(udata, vdata);
            avframe->format = uvdata == udata ? AV_PIX_FMT_NV12 : AV_PIX_FMT_NV21;
            avframe->data[0] = ydata;
            avframe->linesize[0] = ystride;
            avframe->data[1] = uvdata;
            avframe->linesize[1] = uvstride;
        }
    } else {
        for (int i=0; i<planeCount; i++) {
            jobject plane = jenv->GetObjectArrayElement(planes, i);
            //jint pxStride = jenv->CallIntMethod(plane, planeGetPixelStride);
            avframe->data[i] = (uint8_t *)jenv->GetDirectBufferAddress(jenv->CallObjectMethod(plane, planeGetBuffer));
            avframe->linesize[i] = jenv->CallIntMethod(plane, planeGetRowStride);
        }
    }

    setRotation(rotation);
    if (rotMatrix)
        av_frame_new_side_data_from_buf(avframe, AV_FRAME_DATA_DISPLAYMATRIX, av_buffer_ref(rotMatrix));

    image = jenv->NewGlobalRef(image);
    frame->setReleaseCb([jenv, image](uint8_t *) mutable {
        bool justAttached = false;
        int envStat = gJavaVM->GetEnv((void**)&jenv, JNI_VERSION_1_6);
        if (envStat == JNI_EDETACHED) {
            justAttached = true;
            if (gJavaVM->AttachCurrentThread(&jenv, nullptr) != 0)
                return;
        } else if (envStat == JNI_EVERSION) {
            return;
        }
        jenv->CallVoidMethod(image, imageClose);
        jenv->DeleteGlobalRef(image);
        if (justAttached)
            gJavaVM->DetachCurrentThread();
    });
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
%native(captureVideoPacket) void captureVideoPacket(jobject, jint, jint, jboolean, jlong, jint);

namespace DRing {

void setDefaultDevice(const std::string& name);
std::string getDefaultDevice();

void startCamera();
void stopCamera();
bool hasCameraStarted();
void startAudioDevice();
void stopAudioDevice();
bool switchInput(const std::string& resource);
bool switchToCamera();
std::map<std::string, std::string> getSettings(const std::string& name);
void applySettings(const std::string& name, const std::map<std::string, std::string>& settings);

void addVideoDevice(const std::string &node);
void removeVideoDevice(const std::string &node);
void setDeviceOrientation(const std::string& name, int angle);
uint8_t* obtainFrame(int length);
void releaseFrame(uint8_t* frame);
void registerSinkTarget(const std::string& sinkId, const DRing::SinkTarget& target);
std::string startLocalRecorder(const bool& audioOnly, const std::string& filepath);
void stopLocalRecorder(const std::string& filepath);
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
    virtual void setBitrate(const std::string, const int bitrate) {}
    virtual void requestKeyFrame(){}
    virtual void startCapture(const std::string& camid) {}
    virtual void stopCapture() {}
    virtual void decodingStarted(const std::string& id, const std::string& shm_path, int w, int h, bool is_mixer) {}
    virtual void decodingStopped(const std::string& id, const std::string& shm_path, bool is_mixer) {}
};
