LOCAL_PATH:= $(call my-dir)

# FIXME
MY_COMMONCPP=commoncpp2-1.8.1-android
MY_CCRTP=ccrtp-1.8.0-android
MY_LIBSAMPLE=libsamplerate-0.1.8

include $(CLEAR_VARS)

LOCAL_SRC_FILES := opensllayer.cpp

# FIXME
LOCAL_C_INCLUDES += $(LOCAL_PATH)/.. \
			$(LOCAL_PATH)/../.. \
			$(APP_PROJECT_PATH)/jni/$(MY_COMMONCPP)/inc \
			$(APP_PROJECT_PATH)/jni/$(MY_CCRTP)/src \
			$(APP_PROJECT_PATH)/jni/$(MY_LIBSAMPLE)/src



LOCAL_MODULE := libopensl
LOCAL_CPPFLAGS += $(NETWORKMANAGER) \
				  -DCCPP_PREFIX \
				  -DPREFIX=\"$(MY_PREFIX)\" \
				  -DPROGSHAREDIR=\"${MY_DATADIR}/sflphone\" \
				  -DHAVE_CONFIG_H \
				  -std=gnu++0x -frtti -fpermissive \
				  -DAPP_NAME=\"openSL\"

LOCAL_SHARED_LIBRARIES += libOpenSLES

include $(BUILD_STATIC_LIBRARY)
