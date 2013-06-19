LOCAL_PATH:= $(call my-dir)

$(warning Android.mk -> $(LOCAL_PATH))

# FIXME
MY_PREFIX=/sdcard
MY_DATADIR=
MY_PJPROJECT="pjproject-android/android"
MY_PJDIR=
MY_COMMONCPP=commoncpp2-1.8.1-android
MY_CCRTP=ccrtp-1.8.0-android
MY_LIBSAMPLE=libsamplerate-0.1.8

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
#    dbusmanager.cpp \
#    callmanager-glue.h              \
#    configurationmanager-glue.h     \
    instance-glue.h

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

LOCAL_MODULE := libdbus-glue
LOCAL_CPPFLAGS += $(NETWORKMANAGER) \
				  -DPREFIX=\"$(MY_PREFIX)\" \
				  -DPROGSHAREDIR=\"${MY_DATADIR}/sflphone\" \
				  -DHAVE_CONFIG_H \
				  -std=gnu++0x -frtti -fpermissive \
				  -DAPP_NAME=\"dbus-glue\"

LOCAL_SHARED_LIBRARIES += libdbus-c++-1

include $(BUILD_STATIC_LIBRARY)

