LOCAL_PATH:= $(call my-dir)

MY_LIBEXPAT=libexpat
MY_LIBYAML=libyaml

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	sfl_config.cpp \
	yamlemitter.cpp \
	yamlparser.cpp \
	yamlnode.cpp

# FIXME
LOCAL_C_INCLUDES += $(LOCAL_PATH)/.. \
					$(LOCAL_PATH)/../.. \
					$(APP_PROJECT_PATH)/jni/$(MY_LIBYAML)/inc
LOCAL_MODULE := libconfig
LOCAL_CPPFLAGS += -std=gnu++0x -fexceptions -frtti
LOCAL_LDFLAGS += -L$(APP_PROJECT_PATH)/obj/local/armeabi \
				 -lyaml
LOCAL_SHARED_LIBRARIES += libyaml

include $(BUILD_STATIC_LIBRARY)
