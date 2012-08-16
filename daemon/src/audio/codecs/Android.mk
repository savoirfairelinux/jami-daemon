LOCAL_PATH:= $(call my-dir)

MY_COMMONCPP=commoncpp2-1.8.1-android

include $(CLEAR_VARS)

LOCAL_SRC_FILES := ulaw.cpp \
		audiocodec.cpp

# FIXME
LOCAL_C_INCLUDES += $(LOCAL_PATH)/.. \
			$(LOCAL_PATH)/../.. \
			$(APP_PROJECT_PATH)/jni/$(MY_COMMONCPP)/inc 

LOCAL_MODULE := libcodec_ulaw

LOCAL_CPPFLAGS += $(NETWORKMANAGER) \
				  -DCCPP_PREFIX \
				  -DPREFIX=\"$(MY_PREFIX)\" \
				  -DPROGSHAREDIR=\"${MY_DATADIR}/sflphone\" \
				  -std=gnu++0x -frtti -fpermissive -fexceptions \
				  -DAPP_NAME=\"codec_ulaw\"

include $(BUILD_SHARED_LIBRARY)


include $(CLEAR_VARS)

LOCAL_SRC_FILES := alaw.cpp \
		audiocodec.cpp

LOCAL_C_INCLUDES += $(LOCAL_PATH)/.. \
			$(LOCAL_PATH)/../.. \
			$(APP_PROJECT_PATH)/jni/$(MY_COMMONCPP)/inc \

LOCAL_MODULE := libcodec_alaw
LOCAL_CPPFLAGS += $(NETWORKMANAGER) \
				  -DCCPP_PREFIX \
				  -DPREFIX=\"$(MY_PREFIX)\" \
				  -DPROGSHAREDIR=\"${MY_DATADIR}/sflphone\" \
				  -std=gnu++0x -frtti -fpermissive -fexceptions \
				  -DAPP_NAME=\"codec_alaw\"

include $(BUILD_SHARED_LIBRARY)


include $(CLEAR_VARS)

LOCAL_SRC_FILES := audiocodecfactory.cpp

LOCAL_C_INCLUDES += $(LOCAL_PATH)/.. \
			$(LOCAL_PATH)/../.. \
			$(LOCAL_PATH)/../../.. \
			$(APP_PROJECT_PATH)/jni/$(MY_COMMONCPP)/inc 

LOCAL_MODULE := libcodecfactory
LOCAL_CPPFLAGS += $(NETWORKMANAGER) \
				  -DCCPP_PREFIX \
				  -DCODECS_DIR=\"/usr/lib/sflphone/audio/codec\" \
				  -DPREFIX=\"$(MY_PREFIX)\" \
				  -DPROGSHAREDIR=\"${MY_DATADIR}/sflphone\" \
				  -DHAVE_COFIG_H \
				  -std=gnu++0x -frtti -fpermissive -fexceptions \
				  -DAPP_NAME=\"codecfactory\"

include $(BUILD_STATIC_LIBRARY)
