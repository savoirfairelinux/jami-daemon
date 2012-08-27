LOCAL_PATH:= $(call my-dir)

# FIXME
MY_PREFIX=/sdcard
MY_DATADIR=
MY_PJPROJECT="pjproject-android/android"
MY_PJDIR=
MY_COMMONCPP=commoncpp2-1.8.1-android
MY_CCRTP=ccrtp-1.8.0-android
MY_LIBSAMPLE=libsamplerate-0.1.8
MY_YAML=libyaml

# FIXME
ifneq ($(BUILD_SDES),)
libsiplink_la_SOURCES += sdes_negotiator.cpp \
						 pattern.cpp

libsiplink_la_CXXFLAGS = \
		@PCRE_LIBS@
endif

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
		sdp.cpp \
		sipaccount.cpp \
		sipcall.cpp \
		sipvoiplink.cpp \
		siptransport.cpp \
		sip_utils.cpp

# FIXME
LOCAL_C_INCLUDES += $(LOCAL_PATH)/.. \
					$(LOCAL_PATH)/../.. \
					$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/third_party/build/speex \
					$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/third_party/speex/include \
					$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjmedia/include \
					$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjlib/include \
					$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjsip/include \
					$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjlib-util/include \
					$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjnath/include \
					$(APP_PROJECT_PATH)/jni/$(MY_COMMONCPP)/inc \
					$(APP_PROJECT_PATH)/jni/$(MY_LIBSAMPLE)/src \
					$(APP_PROJECT_PATH)/jni/$(MY_CCRTP)/src \
					$(APP_PROJECT_PATH)/jni/$(MY_YAML)/inc \
					$(LOCAL_PATH)/../../libs/iax2 \

#LOCAL_CPP_EXTENSION := .cpp .h

LOCAL_MODULE := libsiplink
LOCAL_CPPFLAGS += $(NETWORKMANAGER) \
				  -DPREFIX=\"$(MY_PREFIX)\" \
				  -DPROGSHAREDIR=\"${MY_DATADIR}/sflphone\" \
				  -DHAVE_CONFIG_H \
				  -std=gnu++0x -frtti -fpermissive \
				  -DAPP_NAME=\"sip\"

LOCAL_SHARED_LIBRARIES += libdbus-c++-1

include $(BUILD_STATIC_LIBRARY)
