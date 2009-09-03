# build.mak.  Generated from build.mak.in by configure.
export MACHINE_NAME := auto
export OS_NAME := auto
export HOST_NAME := unix
export CC_NAME := gcc
export TARGET_NAME := x86_64-unknown-linux-gnu
export CROSS_COMPILE := 
export LINUX_POLL := select 

LIB_SUFFIX = $(TARGET_NAME).a

# Determine which party libraries to use
export APP_THIRD_PARTY_LIBS := -lresample-$(TARGET_NAME) -lmilenage-$(TARGET_NAME) -lsrtp-$(TARGET_NAME)
export APP_THIRD_PARTY_LIB_FILES = $(PJ_DIR)/third_party/lib/libresample-$(LIB_SUFFIX) $(PJ_DIR)/third_party/lib/libmilenage-$(LIB_SUFFIX) $(PJ_DIR)/third_party/lib/libsrtp-$(LIB_SUFFIX)

ifneq (,1)
APP_THIRD_PARTY_LIBS += -lgsmcodec-$(TARGET_NAME)
APP_THIRD_PARTY_LIB_FILES += $(PJ_DIR)/third_party/lib/libgsmcodec-$(LIB_SUFFIX)
endif

ifneq (,1)
APP_THIRD_PARTY_LIBS += -lspeex-$(TARGET_NAME)
APP_THIRD_PARTY_LIB_FILES += $(PJ_DIR)/third_party/lib/libspeex-$(LIB_SUFFIX)
endif

ifneq (,1)
APP_THIRD_PARTY_LIBS += -lilbccodec-$(TARGET_NAME)
APP_THIRD_PARTY_LIB_FILES += $(PJ_DIR)/third_party/lib/libilbccodec-$(LIB_SUFFIX)
endif

ifneq (,1)
APP_THIRD_PARTY_LIBS += -lg7221codec-$(TARGET_NAME)
APP_THIRD_PARTY_LIB_FILES += $(PJ_DIR)/third_party/lib/libg7221codec-$(LIB_SUFFIX)
endif

ifneq ($(findstring pa,pa_unix),)
APP_THIRD_PARTY_LIBS += -lportaudio-$(TARGET_NAME)
APP_THIRD_PARTY_LIB_FILES += $(PJ_DIR)/third_party/lib/libportaudio-$(LIB_SUFFIX)
endif

# Additional flags


# CFLAGS, LDFLAGS, and LIBS to be used by applications
export PJDIR := /home/emilou/git-repos/sflphone/sflphone-common/libs/pjproject
export APP_CC := $(CROSS_COMPILE)$(CC_NAME)
export APP_CFLAGS := -DPJ_AUTOCONF=1\
	-O2\
	-I$(PJDIR)/pjlib/include\
	-I$(PJDIR)/pjlib-util/include\
	-I$(PJDIR)/pjnath/include\
	-I$(PJDIR)/pjmedia/include\
	-I$(PJDIR)/pjsip/include
export APP_CXXFLAGS := $(APP_CFLAGS)
export APP_LDFLAGS := -L$(PJDIR)/pjlib/lib\
	-L$(PJDIR)/pjlib-util/lib\
	-L$(PJDIR)/pjnath/lib\
	-L$(PJDIR)/pjmedia/lib\
	-L$(PJDIR)/pjsip/lib\
	-L$(PJDIR)/third_party/lib\
	
export APP_LDLIBS := -lpjsua-sfl-$(TARGET_NAME)\
	-lpjsip-ua-sfl-$(TARGET_NAME)\
	-lpjsip-simple-sfl-$(TARGET_NAME)\
	-lpjsip-sfl-$(TARGET_NAME)\
	-lpjmedia-codec-sfl-$(TARGET_NAME)\
	-lpjmedia-sfl-$(TARGET_NAME)\
	-lpjmedia-audiodev-sfl-$(TARGET_NAME)\
	-lpjnath-sfl-$(TARGET_NAME)\
	-lpjlib-util-sfl-$(TARGET_NAME)\
	$(APP_THIRD_PARTY_LIBS)\
	-lpj-sfl-$(TARGET_NAME)\
	-lm -luuid -lnsl -lrt -lpthread  -lasound -lssl -lcrypto
export APP_LIB_FILES = $(PJ_DIR)/pjsip/lib/libpjsua-sfl-$(LIB_SUFFIX) \
	$(PJ_DIR)/pjsip/lib/libpjsip-ua-sfl-$(LIB_SUFFIX) \
	$(PJ_DIR)/pjsip/lib/libpjsip-simple-sfl-$(LIB_SUFFIX) \
	$(PJ_DIR)/pjsip/lib/libpjsip-sfl-$(LIB_SUFFIX) \
	$(PJ_DIR)/pjmedia/lib/libpjmedia-codec-sfl-$(LIB_SUFFIX) \
	$(PJ_DIR)/pjmedia/lib/libpjmedia-sfl-$(LIB_SUFFIX) \
	$(PJ_DIR)/pjmedia/lib/libpjmedia-audiodev-sfl-$(LIB_SUFFIX) \
	$(PJ_DIR)/pjnath/lib/libpjnath-sfl-$(LIB_SUFFIX) \
	$(PJ_DIR)/pjlib-util/lib/libpjlib-util-sfl-$(LIB_SUFFIX) \
	$(APP_THIRD_PARTY_LIB_FILES) \
	$(PJ_DIR)/pjlib/lib/libpj-sfl-$(LIB_SUFFIX)

export PJ_DIR := $(PJDIR)
export PJ_CC := $(APP_CC)
export PJ_CFLAGS := $(APP_CFLAGS)
export PJ_CXXFLAGS := $(APP_CXXFLAGS)
export PJ_LDFLAGS := $(APP_LDFLAGS)
export PJ_LDLIBS := $(APP_LDLIBS)
export PJ_LIB_FILES := $(APP_LIB_FILES)

