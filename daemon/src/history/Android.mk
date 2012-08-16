LOCAL_PATH := $(call my-dir)

MY_COMMONCPP=commoncpp2-1.8.1-android
MY_LIBSAMPLE=libsamplerate-0.1.8
MY_DBUS=libdbus-c++-0.9.0-android


include $(CLEAR_VARS)

LOCAL_SRC_FILES := historyitem.cpp \
		history.cpp \
		historynamecache.cpp

LOCAL_C_INCLUDES += $(LOCAL_PATH)/.. \
					$(LOCAL_PATH)/../.. \
					$(APP_PROJECT_PATH)/jni/$(MY_COMMONCPP)/inc \
					$(APP_PROJECT_PATH)/jni/$(MY_DBUS)/include \
					$(APP_PROJECT_PATH)/jni/$(MY_LIBSAMPLE)/src

LOCAL_MODULE := libhistory

LOCAL_CPPFLAGS += $(NETWORKMANAGER) \
				  -DCCPP_PREFIX \
				  -DPREFIX=\"$(MY_PREFIX)\" \
				  -DPROGSHAREDIR=\"${MY_DATADIR}/sflphone\" \
				  -DHAVE_CONFIG_H \
				  -std=gnu++0x -frtti -fpermissive -fexceptions \
				  -DAPP_NAME=\"history\"


include $(BUILD_STATIC_LIBRARY)
