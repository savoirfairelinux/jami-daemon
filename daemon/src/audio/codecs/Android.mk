LOCAL_PATH:= $(call my-dir)

MY_COMMONCPP=commoncpp2-1.8.1-android
MY_CCRTP=ccrtp-1.8.0-android

############# ulaw ###############

include $(CLEAR_VARS)

LOCAL_SRC_FILES := ulaw.cpp \
		audiocodec.cpp

# FIXME
LOCAL_C_INCLUDES += $(LOCAL_PATH)/.. \
			$(LOCAL_PATH)/../.. \
			$(APP_PROJECT_PATH)/jni/$(MY_CCRTP)/src \
			$(APP_PROJECT_PATH)/jni/$(MY_COMMONCPP)/inc 

LOCAL_MODULE := libcodec_ulaw

LOCAL_CPPFLAGS += $(NETWORKMANAGER) \
				  -DCCPP_PREFIX \
				  -DPREFIX=\"$(MY_PREFIX)\" \
				  -DPROGSHAREDIR=\"${MY_DATADIR}/sflphone\" \
				  -std=gnu++0x -frtti -fpermissive -fexceptions \
				  -DAPP_NAME=\"codec_ulaw\"

include $(BUILD_SHARED_LIBRARY)



############# alaw ###############

include $(CLEAR_VARS)

LOCAL_SRC_FILES := alaw.cpp \
		audiocodec.cpp

LOCAL_C_INCLUDES += $(LOCAL_PATH)/.. \
			$(LOCAL_PATH)/../.. \
			$(APP_PROJECT_PATH)/jni/$(MY_CCRTP)/src \
			$(APP_PROJECT_PATH)/jni/$(MY_COMMONCPP)/inc \

LOCAL_MODULE := libcodec_alaw
LOCAL_CPPFLAGS += $(NETWORKMANAGER) \
				  -DCCPP_PREFIX \
				  -DPREFIX=\"$(MY_PREFIX)\" \
				  -DPROGSHAREDIR=\"${MY_DATADIR}/sflphone\" \
				  -std=gnu++0x -frtti -fpermissive -fexceptions \
				  -DAPP_NAME=\"codec_alaw\"

LOCAL_LDFLAGS += -Wl,--export-dynamic

include $(BUILD_SHARED_LIBRARY)


############# g722 ###############

include $(CLEAR_VARS)

LOCAL_SRC_FILES := g722.cpp \
		audiocodec.cpp

LOCAL_C_INCLUDES += $(LOCAL_PATH)/.. \
			$(LOCAL_PATH)/../.. \
			$(LOCAL_PATH)/../../.. \
			$(APP_PROJECT_PATH)/jni/$(MY_CCRTP)/src \
			$(APP_PROJECT_PATH)/jni/$(MY_COMMONCPP)/inc 

LOCAL_MODULE := libcodec_g722
LOCAL_CPPFLAGS += $(NETWORKMANAGER) \
				  -DCCPP_PREFIX \
				  -DCODECS_DIR=\"/usr/lib/sflphone/audio/codec\" \
				  -DPREFIX=\"$(MY_PREFIX)\" \
				  -DPROGSHAREDIR=\"${MY_DATADIR}/sflphone\" \
				  -DHAVE_COFIG_H \
				  -std=gnu++0x -frtti -fpermissive -fexceptions \
				  -DAPP_NAME=\"codecfactory\"

include $(BUILD_SHARED_LIBRARY)



############# opus ###############

include $(CLEAR_VARS)

LOCAL_SRC_FILES := opus.cpp \
		audiocodec.cpp

LOCAL_C_INCLUDES += $(LOCAL_PATH)/.. \
			$(LOCAL_PATH)/../.. \
			$(LOCAL_PATH)/../../.. \
			$(APP_PROJECT_PATH)/jni/$(MY_CCRTP)/src \
			$(APP_PROJECT_PATH)/jni/$(MY_COMMONCPP)/inc 

LOCAL_MODULE := libcodec_opus
LOCAL_CPPFLAGS += $(NETWORKMANAGER) \
				  -DCCPP_PREFIX \
				  -DCODECS_DIR=\"/usr/lib/sflphone/audio/codec\" \
				  -DPREFIX=\"$(MY_PREFIX)\" \
				  -DPROGSHAREDIR=\"${MY_DATADIR}/sflphone\" \
				  -DHAVE_COFIG_H \
				  -std=gnu++0x -frtti -fpermissive -fexceptions \
				  -DAPP_NAME=\"codecfactory\"

include $(BUILD_SHARED_LIBRARY)




############# speex_nb ###############

include $(CLEAR_VARS)

LOCAL_SRC_FILES := speexcodec_nb.cpp \
		audiocodec.cpp

LOCAL_C_INCLUDES += $(LOCAL_PATH)/.. \
			$(LOCAL_PATH)/../.. \
			$(LOCAL_PATH)/../../.. \
			$(APP_PROJECT_PATH)/jni/$(MY_CCRTP)/src \
			$(APP_PROJECT_PATH)/jni/$(MY_COMMONCPP)/inc 

LOCAL_MODULE := libcodec_speex_nb
LOCAL_CPPFLAGS += $(NETWORKMANAGER) \
				  -DCCPP_PREFIX \
				  -DCODECS_DIR=\"/usr/lib/sflphone/audio/codec\" \
				  -DPREFIX=\"$(MY_PREFIX)\" \
				  -DPROGSHAREDIR=\"${MY_DATADIR}/sflphone\" \
				  -DHAVE_COFIG_H \
				  -std=gnu++0x -frtti -fpermissive -fexceptions \
				  -DAPP_NAME=\"codecfactory\"

include $(BUILD_SHARED_LIBRARY)



############# speex_ub ###############

include $(CLEAR_VARS)

LOCAL_SRC_FILES := speexcodec_ub.cpp \
		audiocodec.cpp

LOCAL_C_INCLUDES += $(LOCAL_PATH)/.. \
			$(LOCAL_PATH)/../.. \
			$(LOCAL_PATH)/../../.. \
			$(APP_PROJECT_PATH)/jni/$(MY_CCRTP)/src \
			$(APP_PROJECT_PATH)/jni/$(MY_COMMONCPP)/inc 

LOCAL_MODULE := libcodec_speex_ub
LOCAL_CPPFLAGS += $(NETWORKMANAGER) \
				  -DCCPP_PREFIX \
				  -DCODECS_DIR=\"/usr/lib/sflphone/audio/codec\" \
				  -DPREFIX=\"$(MY_PREFIX)\" \
				  -DPROGSHAREDIR=\"${MY_DATADIR}/sflphone\" \
				  -DHAVE_COFIG_H \
				  -std=gnu++0x -frtti -fpermissive -fexceptions \
				  -DAPP_NAME=\"codecfactory\"

include $(BUILD_SHARED_LIBRARY)

############# speex_wb ###############

include $(CLEAR_VARS)

LOCAL_SRC_FILES := speexcodec_wb.cpp \
		audiocodec.cpp

LOCAL_C_INCLUDES += $(LOCAL_PATH)/.. \
			$(LOCAL_PATH)/../.. \
			$(LOCAL_PATH)/../../.. \
			$(APP_PROJECT_PATH)/jni/$(MY_CCRTP)/src \
			$(APP_PROJECT_PATH)/jni/$(MY_COMMONCPP)/inc 

LOCAL_MODULE := libcodec_speex_wb
LOCAL_CPPFLAGS += $(NETWORKMANAGER) \
				  -DCCPP_PREFIX \
				  -DCODECS_DIR=\"/usr/lib/sflphone/audio/codec\" \
				  -DPREFIX=\"$(MY_PREFIX)\" \
				  -DPROGSHAREDIR=\"${MY_DATADIR}/sflphone\" \
				  -DHAVE_COFIG_H \
				  -std=gnu++0x -frtti -fpermissive -fexceptions \
				  -DAPP_NAME=\"codecfactory\"

include $(BUILD_SHARED_LIBRARY)


############# audiocodecfactory ###############

include $(CLEAR_VARS)

LOCAL_SRC_FILES := audiocodecfactory.cpp

LOCAL_C_INCLUDES += $(LOCAL_PATH)/.. \
			$(LOCAL_PATH)/../.. \
			$(LOCAL_PATH)/../../.. \
			$(APP_PROJECT_PATH)/jni/$(MY_CCRTP)/src \
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
