# /!\ absolutely necessary when including submakefiles
# and defining targets in the "same Android.mk"
MYSRC_LOCAL_PATH := $(call my-dir)
include $(call all-subdir-makefiles)

LOCAL_PATH := $(MYSRC_LOCAL_PATH)

# FIXME
VERSION="1.1.0"
MY_PREFIX=/sdcard
MY_DATADIR=/data/data
TARGET_NAME=arm-unknown-linux-androideabi

MY_PJPROJECT="pjproject-android/android"
MY_COMMONCPP=commoncpp2-1.8.1-android
MY_CCRTP=ccrtp-1.8.0-android
MY_LIBSAMPLE=libsamplerate-0.1.8
MY_YAML=libyaml
MY_DBUSCPP=libdbus-c++-0.9.0-android


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
		fileutils.cpp

# FIXME
LOCAL_C_INCLUDES += $(LOCAL_PATH)/.. \
					$(APP_PROJECT_PATH)/jni/$(MY_COMMONCPP)/inc \
					$(APP_PROJECT_PATH)/jni/$(MY_CCRTP)/src \
					$(APP_PROJECT_PATH)/jni/$(MY_DBUSCPP)/include \
					$(APP_PROJECT_PATH)/jni/$(MY_LIBSAMPLE)/src \
					$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjsip/include \
					$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjlib/include \
					$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjlib-util/include \
					$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjmedia/include \
					$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjnath/include \
					$(APP_PROJECT_PATH)/jni/$(MY_LIBYAML)/inc

LOCAL_MODULE := libsflphone

LOCAL_CPPFLAGS += $(NETWORKMANAGER) \
				  -DPREFIX=\"$(MY_PREFIX)\" \
				  -DPROGSHAREDIR=\"${MY_DATADIR}/sflphone\" \
				  -DHAVE_CONFIG_H \
				  -std=gnu++0x -frtti -fexceptions -fpermissive

LOCAL_LDFLAGS += -L$(APP_PROJECT_PATH)/obj/local/armeabi \
				 -L$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjsip/lib \
				 -L$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjlib/lib \
				 -L$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjlib-util/lib \
				 -L$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjmedia/lib \
				 -L$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjnath/lib

# LOCAL_STATIC_LIBRARIES (NDK documentation)
#   The list of static libraries modules (built with BUILD_STATIC_LIBRARY)
#   that should be linked to this module. This only makes sense in
#   shared library modules.
LOCAL_STATIC_LIBRARIES += libpjsua-$(TARGET_NAME) \
						  libpjsip-ua-$(TARGET_NAME) \
						  libpjsip-simple-$(TARGET_NAME) \
						  libpjsip-$(TARGET_NAME) \
						  libpjmedia-codec-$(TARGET_NAME) \
						  libpjmedia-$(TARGET_NAME) \
						  libpjnath-$(TARGET_NAME) \
						  libpjlib-util-$(TARGET_NAME) \
						  libpj-$(TARGET_NAME) \
						  libsiplink \
						  libaudio \
						  libdbus-glue \
						  libconfig \
						  libopensl \
						  libsound \
						  libcodecfactory \
						  librtp \
						  libhooks \
						  libhistory \
						  libspeex

LOCAL_SHARED_LIBRARIES += libccgnu2 \
						  libccrtp1 \
						  libsamplerate \
						  libulaw \
						  libalaw \
						  libspeexresampler

include $(BUILD_STATIC_LIBRARY)


include $(CLEAR_VARS)

# FIXME
LOCAL_C_INCLUDES += $(LOCAL_PATH)/.. \
					$(APP_PROJECT_PATH)/jni/$(MY_COMMONCPP)/inc \
					$(APP_PROJECT_PATH)/jni/$(MY_CCRTP)/src \
					$(APP_PROJECT_PATH)/jni/$(MY_DBUSCPP)/include \
					$(APP_PROJECT_PATH)/jni/$(MY_LIBSAMPLE)/src \
					$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjsip/include \
					$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjlib/include \
					$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjlib-util/include \
					$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjmedia/include \
					$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjnath/include \
					$(APP_PROJECT_PATH)/jni/$(MY_LIBYAML)/inc

LOCAL_SRC_FILES = main.cpp

LOCAL_MODULE := sflphoned

LOCAL_CPPFLAGS = \
		-DPREFIX=\"$(MY_PREFIX)\" \
		-DPROGSHAREDIR=\"${MY_DATADIR}/sflphone\" \
		$(IAX_CXXFLAG) $(NETWORKMANAGER) \
		-DVERSION=\"$(VERSION)\" \
		-DHAVE_CONFIG_H \
		-std=gnu++0x -frtti -fexceptions -fpermissive

LOCAL_LDFLAGS += -L$(APP_PROJECT_PATH)/obj/local/armeabi \
				 -L$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjsip/lib \
				 -L$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjlib/lib \
				 -L$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjlib-util/lib \
				 -L$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjmedia/lib \
				 -L$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjnath/lib \
				 -lpjsua-$(TARGET_NAME) \
				 -lpjsip-ua-$(TARGET_NAME) \
				 -lpjsip-simple-$(TARGET_NAME) \
				 -lpjsip-$(TARGET_NAME) \
				 -lpjmedia-codec-$(TARGET_NAME) \
				 -lpjmedia-$(TARGET_NAME) \
				 -lpjnath-$(TARGET_NAME) \
				 -lpjlib-util-$(TARGET_NAME) \
				 -lpj-$(TARGET_NAME) \
				 -lccgnu2 \
				 -lccrtp1 \
				 -lsamplerate \
				 -lconfig \
				 -lcodecfactory \
				 -lsound \
				 -lhistory \
				 -lrtp \
				 -lhooks \
				 -lopensl \
				 -lsiplink \
				 -laudio \
				 -lspeex \
				 -lspeexresampler \
				 -lsamplerate \
				 -lccrtp1 \
				 -lccgnu2 \
				 -lyaml \
				 -ldbus-c++-1 \
				 -ldbus \
				 -lalaw \
				 -lulaw \
				 -lexpat \
				 -lsflphone \
				 -lcrypto \
				 -lssl \
				 -lz

				 
# LOCAL_STATIC_LIBRARIES (NDK documentation)
#   The list of static libraries modules (built with BUILD_STATIC_LIBRARY)
#   that should be linked to this module. This only makes sense in
#   shared library modules.
LOCAL_STATIC_LIBRARIES := libsflphone

LOCAL_SHARED_LIBRARIES += libyaml

include $(BUILD_EXECUTABLE)

