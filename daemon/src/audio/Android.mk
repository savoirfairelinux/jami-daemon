MYAUDIO_LOCAL_PATH := $(call my-dir)
include $(call all-subdir-makefiles)

LOCAL_PATH := $(MYAUDIO_LOCAL_PATH)

# FIXME
MY_COMMONCPP=commoncpp2-1.8.1-android
MY_LIBSAMPLE=libsamplerate-0.1.8
MY_DBUS=libdbus-c++-0.9.0-android


include $(CLEAR_VARS)

LOCAL_SRC_FILES := audioloop.cpp \
		ringbuffer.cpp \
		mainbuffer.cpp \
		audiorecord.cpp \
		audiorecorder.cpp \
		recordable.cpp \
		audiolayer.cpp \
		samplerateconverter.cpp \
		delaydetection.cpp \
		gaincontrol.cpp \
		dcblocker.cpp

LOCAL_C_INCLUDES += $(LOCAL_PATH)/.. \
			$(LOCAL_PATH)/../.. \
			$(APP_PROJECT_PATH)/jni/$(MY_COMMONCPP)/inc \
			$(APP_PROJECT_PATH)/jni/$(MY_DBUS)/include \
			$(APP_PROJECT_PATH)/jni/$(MY_LIBSAMPLE)/src

LOCAL_MODULE := libaudio

LOCAL_CPPFLAGS += $(NETWORKMANAGER) \
				  -DCCPP_PREFIX \
				  -DPREFIX=\"$(MY_PREFIX)\" \
				  -DPROGSHAREDIR=\"${MY_DATADIR}/sflphone\" \
				  -DHAVE_CONFIG_H \
				  -std=gnu++0x -frtti -fpermissive -fexceptions

include $(BUILD_STATIC_LIBRARY)
