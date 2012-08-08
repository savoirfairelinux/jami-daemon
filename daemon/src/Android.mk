# /!\ absolutely necessary when including submakefiles
# and defining targets in the "same Android.mk"
MYSRC_LOCAL_PATH := $(call my-dir)
include $(call all-subdir-makefiles)

LOCAL_PATH := $(MYSRC_LOCAL_PATH)

# FIXME
MY_PREFIX=/sdcard
MY_DATADIR=
MY_PJPROJECT="pjproject-android/android"
MY_PJDIR=
MY_COMMONCPP=commoncpp2-1.8.1-android
MY_CCRTP=ccrtp-1.8.0-android
MY_LIBSAMPLE=libsamplerate-0.1.8
MY_YAML=libyaml


include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
		conference.cpp \
		voiplink.cpp \
		preferences.cpp \
		managerimpl.cpp \
		manager.cpp \
		managerimpl_registration.cpp \
		eventthread.cpp \
		call.cpp \
		account.cpp \
		logger.cpp \
		numbercleaner.cpp \
		fileutils.cpp \

# FIXME
LOCAL_C_INCLUDES += $(LOCAL_PATH)/.. \
					$(APP_PROJECT_PATH)/jni/$(MY_COMMONCPP)/inc \
					$(APP_PROJECT_PATH)/jni/$(MY_CCRTP)/src \
					$(APP_PROJECT_PATH)/jni/$(MY_LIBSAMPLE)/src \
					$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjsip/include \
					$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjlib/include \
					$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjlib-util/include \
					$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjmedia/include \
					$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjnath/include \
					$(APP_PROJECT_PATH)/jni/$(MY_LIBYAML)/inc \


LOCAL_MODULE := sflphoned

LOCAL_CPPFLAGS += $(NETWORKMANAGER) \
				  -DPREFIX=\"$(MY_PREFIX)\" \
				  -DPROGSHAREDIR=\"${MY_DATADIR}/sflphone\" \
				  -DHAVE_CONFIG_H \
				  -std=gnu++0x -frtti -fexceptions -fpermissive

LOCAL_LDFLAGS += -L$(APP_PROJECT_PATH)/obj/local/armeabi \
				 -lccgnu2 \
				 -lccrtp1 \
				 -lsamplerate \
				 -lspeex \
				 -lspeexresampler \
				 -lsiplink \
				 -laudio \
				 -ldbus-glue \
				 -lconfig \
				 -lhooks \
				 -lhistory \
				 -lopensl \
				 -lsound \
				 -lulaw \
				 -lalaw \
				 -lcodecfactory \
				 -lrtp

LOCAL_STATIC_LIBRARIES += libsiplink \
						  libaudio \
						  libdbus-glue \
						  libconfig \
						  libhooks \
						  libhistory \
						  libopensl \
						  libsound \
						  libulaw \
						  libalaw \
						  libcodecfactory \
						  librtp

LOCAL_SHARED_LIBRARIES += libccgnu2 \
						  libccrtp1 \
						  libsamplerate \
						  libspeex \
						  libspeexresampler

include $(BUILD_EXECUTABLE)

