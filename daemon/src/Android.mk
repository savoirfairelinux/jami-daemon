# /!\ absolutely necessary when including submakefiles
# and defining targets in the "same Android.mk"
MYSRC_LOCAL_PATH := $(call my-dir)
#include $(call all-subdir-makefiles)
include $(MYSRC_LOCAL_PATH)/audio/codecs/Android.mk

LOCAL_PATH := $(MYSRC_LOCAL_PATH)

# FIXME
VERSION="1.1.0"
MY_PREFIX=/sdcard
MY_DATADIR=/data/data
TARGET_NAME=arm-unknown-linux-androideabi

MY_PJPROJECT=pjproject-android/android
MY_COMMONCPP=commoncpp2-1.8.1-android
MY_CCRTP=ccrtp-1.8.0-android
MY_LIBSAMPLE=libsamplerate-0.1.8
MY_DBUSCPP=libdbus-c++-0.9.0-android
MY_DBUS=libdbus-c++-0.9.0-android
MY_SPEEX=speex
MY_OPENSSL=openssl
MY_LIBYAML=libyaml
MY_JNI_WRAP := $(LOCAL_PATH)/dbus/callmanager_wrap.cpp

include $(CLEAR_VARS)

$(MY_JNI_WRAP): $(LOCAL_PATH)/dbus/callmanager.i $(LOCAL_PATH)/sflphoneservice.c.template
	@echo "in $(MY_JNI_WRAP) target"
	./make-swig.sh

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
		audio/audioloop.cpp \
		audio/ringbuffer.cpp \
		audio/mainbuffer.cpp \
		audio/audiorecord.cpp \
		audio/audiorecorder.cpp \
		audio/recordable.cpp \
		audio/audiolayer.cpp \
		audio/samplerateconverter.cpp \
		audio/delaydetection.cpp \
		audio/gaincontrol.cpp \
		audio/dcblocker.cpp \
		audio/opensl/opensllayer.cpp \
		audio/sound/audiofile.cpp \
		audio/sound/tone.cpp \
		audio/sound/tonelist.cpp \
		audio/sound/dtmf.cpp \
		audio/sound/dtmfgenerator.cpp \
		audio/codecs/audiocodecfactory.cpp \
		audio/audiortp/audio_rtp_session.cpp \
		audio/audiortp/audio_symmetric_rtp_session.cpp \
		audio/audiortp/audio_rtp_record_handler.cpp \
		audio/audiortp/audio_rtp_factory.cpp \
		audio/audiortp/audio_srtp_session.cpp \
		config/sfl_config.cpp \
		config/yamlemitter.cpp \
		config/yamlparser.cpp \
		config/yamlnode.cpp \
		dbus/callmanager.cpp \
    		dbus/configurationmanager.cpp  \
		dbus/callmanager_wrap.cpp \
    		dbus/instance.cpp  \
    		dbus/dbusmanager.cpp \
		android-jni/callmanagerJNI.cpp \
		history/historyitem.cpp \
		history/history.cpp \
		history/historynamecache.cpp \
		hooks/urlhook.cpp \
		sip/sdp.cpp \
		sip/sipaccount.cpp \
		sip/sipcall.cpp \
		sip/sipvoiplink.cpp \
		sip/siptransport.cpp \
		sip/sip_utils.cpp




# FIXME
LOCAL_C_INCLUDES += $(LOCAL_PATH) \
			$(LOCAL_PATH)/.. \
			$(LOCAL_PATH)/audio \
			$(LOCAL_PATH)/audio/opensl \
			$(LOCAL_PATH)/audio/sound \
			$(LOCAL_PATH)/audio/codecs \
			$(LOCAL_PATH)/audio/audiortp \
			$(LOCAL_PATH)/config \
			$(LOCAL_PATH)/dbus \
			$(LOCAL_PATH)/history \
			$(LOCAL_PATH)/hooks \
			$(LOCAL_PATH)/sip \
			$(APP_PROJECT_PATH)/jni/$(MY_SPEEX)/include \
			$(APP_PROJECT_PATH)/jni/$(MY_COMMONCPP)/inc \
			$(APP_PROJECT_PATH)/jni/$(MY_CCRTP)/src \
			$(APP_PROJECT_PATH)/jni/$(MY_DBUSCPP)/include \
			$(APP_PROJECT_PATH)/jni/$(MY_DBUS)/include \
			$(APP_PROJECT_PATH)/jni/$(MY_LIBSAMPLE)/src \
			$(APP_PROJECT_PATH)/jni/$(MY_OPENSSL)/include \
			$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjsip/include \
			$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjlib/include \
			$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjlib-util/include \
			$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjmedia/include \
			$(APP_PROJECT_PATH)/jni/$(MY_PJPROJECT)/pjnath/include \
			$(APP_PROJECT_PATH)/jni/$(MY_LIBYAML)/inc \
			$(APP_PROJECT_PATH)/jni/$(MY_SPEEX)/include

LOCAL_MODULE := libsflphone

LOCAL_CPPFLAGS += $(NETWORKMANAGER) \
				  -DCCPP_PREFIX \
				  -DCODECS_DIR=\"/usr/lib/sflphone/audio/codec\" \
				  -DPREFIX=\"$(MY_PREFIX)\" \
				  -DPROGSHAREDIR=\"${MY_DATADIR}/sflphone\" \
				  -DHAVE_CONFIG_H \
				  -std=gnu++0x -frtti -fexceptions -fpermissive \
				  -DAPP_NAME=\"sflphone\" \
				  -DSWIG_JAVA_ATTACH_CURRENT_THREAD_AS_DAEMON \
				  -DDEBUG_DIRECTOR_OWNED

#-L$(APP_PROJECT_PATH)/obj/local/armeabi \

LOCAL_LDLIBS  += -L$(APP_PROJECT_PATH)/obj/local/armeabi \
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
		 -lspeex \
		 -lspeexresampler \
		 -lsamplerate \
		 -lccrtp1 \
		 -lccgnu2 \
		 -lyaml \
		 -ldbus-c++-1 \
		 -ldbus \
		 -lexpat \
		 -lcrypto \
		 -lssl \
		 -lz \
		 -lcodec_ulaw \
		 -lcodec_alaw \
		 -llog \
		 -lOpenSLES \
		 -lgnustl_shared

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
						  libspeex \
						  libdbus-c++-1 \
						  libdbus \


LOCAL_SHARED_LIBRARIES += libccgnu2 \
						  libccrtp1 \
						  libexpat_shared \
						  libsamplerate \
						  libcodec_ulaw \
						  libcodec_alaw \
						  libspeexresampler \
						  libyaml

include $(BUILD_SHARED_LIBRARY)


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
		-std=gnu++0x -frtti -fexceptions -fpermissive \
		-DAPP_NAME=\"sflphoned\"

LOCAL_LDLIBS  += -L$(APP_PROJECT_PATH)/obj/local/armeabi \
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
				 -lspeex \
				 -lspeexresampler \
				 -lsamplerate \
				 -lccrtp1 \
				 -lccgnu2 \
				 -lyaml \
				 -ldbus-c++-1 \
				 -ldbus \
				 -lexpat \
				 -lsflphone \
				 -lcrypto \
				 -lssl \
				 -lz \
				 -lcodec_ulaw \
				 -lcodec_alaw \
				 -lOpenSLES \
				 -llog \
				 -lgnustl_shared
				 
# LOCAL_STATIC_LIBRARIES (NDK documentation)
#   The list of static libraries modules (built with BUILD_STATIC_LIBRARY)
#   that should be linked to this module. This only makes sense in
#   shared library modules.
LOCAL_SHARED_LIBRARIES := libsflphone

include $(BUILD_EXECUTABLE)

