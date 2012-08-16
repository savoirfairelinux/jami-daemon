LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := urlhook.cpp

LOCAL_MODULE := libhooks
LOCAL_CPPFLAGS += $(NETWORKMANAGER) \
				  -DCCPP_PREFIX \
				  -DPREFIX=\"$(MY_PREFIX)\" \
				  -DPROGSHAREDIR=\"${MY_DATADIR}/sflphone\" \
				  -DHAVE_CONFIG_H \
				  -std=gnu++0x -frtti -fpermissive -fexceptions \
				  -DAPP_NAME=\"hooks\"


include $(BUILD_STATIC_LIBRARY)
