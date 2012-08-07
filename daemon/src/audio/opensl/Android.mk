LOCAL_PATH:= $(call my-dir)

# FIXME
MY_PREFIX=/sdcard
MY_DATADIR=
MY_PJPROJECT="pjproject/android"
MY_PJDIR=
MY_COMMONCPP=commoncpp2-1.8.1-android
MY_CCRTP=ccrtp-1.8.0-android
MY_LIBSAMPLE=libsamplerate-0.1.8

include $(CLEAR_VARS)

LOCAL_SRC_FILES := opensllayer.cpp

# FIXME
LOCAL_C_INCLUDES += $(LOCAL_PATH)/.. \
					$(LOCAL_PATH)/../.. \
					$(LOCAL_PATH)/../sip \
					$(LOCAL_PATH)/../config \
					$(LOCAL_PATH)/../history \
					$(APP_PROJECT_PATH)/jni/$(MY_COMMONCPP)/inc \
					$(APP_PROJECT_PATH)/jni/$(MY_CCRTP)/src \
					$(APP_PROJECT_PATH)/jni/$(MY_LIBSAMPLE)/src \
					$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/third_party/speex/include \
					$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/third_party/build/speex \
					$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjsip/include \
					$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjlib/include \
					$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjlib-util/include \
					$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjmedia/include \
					$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjnath/include \

#LOCAL_CPP_EXTENSION := .cpp .h

LOCAL_MODULE := libopensl
LOCAL_CPPFLAGS += $(NETWORKMANAGER) \
				  -DCCPP_PREFIX \
				  -DPREFIX=\"$(MY_PREFIX)\" \
				  -DPROGSHAREDIR=\"${MY_DATADIR}/sflphone\" \
				  -DHAVE_CONFIG_H \
				  -std=gnu++0x -frtti -fpermissive

LOCAL_SHARED_LIBRARIES += libOpenSLES

include $(BUILD_STATIC_LIBRARY)
