LOCAL_PATH:= $(call my-dir)

MY_PJPROJECT="pjproject-android/android"
MY_EXPAT="libexpat"

$(warning Android.mk -> $(LOCAL_PATH))

include $(CLEAR_VARS)

LOCAL_SRC_FILES := instant_messaging.cpp

# FIXME
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include-all


LOCAL_MODULE := libim

include $(BUILD_STATIC_LIBRARY)