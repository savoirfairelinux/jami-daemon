LOCAL_PATH:= $(call my-dir)

# FIXME
prefix=/sdcard
datadir=

# FIXME
ifneq ($(SFL_VIDEO),)
video_SOURCES += video_controls.cpp
video_controls-glue.h: video_controls-introspec.xml Makefile.am
	dbusxx-xml2cpp $< --adaptor=$@
endif

ifneq ($(USE_NETWORKMANAGER),)
network_SOURCES += networkmanager.cpp
NETWORKMANAGER = -DUSE_NETWORKMANAGER
endif

include $(CLEAR_VARS)

# FIXME
# Rule to generate the binding headers
%-glue.h: %-introspec.xml Android.mk
	dbusxx-xml2cpp $< --adaptor=$@

LOCAL_SRC_FILES := $(video_SOURCES) $(network_SOURCES) \
	callmanager.cpp \
    configurationmanager.cpp  \
    instance.cpp  \
    dbusmanager.cpp
#    callmanager-glue.h              \
#    configurationmanager-glue.h     \
#    instance-glue.h

# FIXME
LOCAL_C_INCLUDES += $(LOCAL_PATH)/.. \
					$(LOCAL_PATH)/../.. \
					$(LOCAL_PATH)/../sip \
					$(LOCAL_PATH)/../config \
					$(LOCAL_PATH)/../history \
					$(APP_PROJECT_PATH)/jni/commoncpp2-1.8.1-android/inc \
					$(APP_PROJECT_PATH)/jni/ccrtp-1.8.0-android/src \
					$(APP_PROJECT_PATH)/jni/libsamplerate-0.1.8/src \
					$(LOCAL_PATH)/../../libs/pjproject/third_party/speex/include \
					$(LOCAL_PATH)/../../libs/pjproject/pjlib/include \
					$(LOCAL_PATH)/../../libs/pjproject/pjsip/include \
					$(LOCAL_PATH)/../../libs/pjproject/pjlib-util/include \
					$(LOCAL_PATH)/../../libs/pjproject/third_party/build/speex \
					$(LOCAL_PATH)/../../libs/pjproject/pjmedia/include \
					$(LOCAL_PATH)/../../libs/pjproject/pjnath/include \

#LOCAL_CPP_EXTENSION := .cpp .h

LOCAL_MODULE := libdbus-glue
LOCAL_CPPFLAGS += $(NETWORKMANAGER) \
				  -DPREFIX=\"$(prefix)\" \
				  -DPROGSHAREDIR=\"${datadir}/sflphone\" \
				  -DHAVE_CONFIG_H
LOCAL_SHARED_LIBRARIES += libdbus-c++-1

include $(BUILD_STATIC_LIBRARY)

