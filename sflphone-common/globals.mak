# Global variables
src=$(top_srcdir)
sflcodecdir=$(DESTDIR)$(libdir)/sflphone/codecs
sflplugindir=$(DESTDIR)$(libdir)/sflphone/plugins

# for pjsip
export TARGET_NAME := @target@

PJSIP_LIBS= \
			-L$(src)/libs/pjproject-1.0.1/pjnath/ \
			-L$(src)/libs/pjproject-1.0.1/pjsip/ \
			-L$(src)/libs/pjproject-1.0.1/pjlib/ \
			-L$(src)/libs/pjproject-1.0.1/pjlib-util/ \
			-L$(src)/libs/pjproject-1.0.1/pjmedia/ \
			-lpjnath-sfl-$(TARGET_NAME) -lpjsua-sfl-$(TARGET_NAME) -lpjsip-sfl-$(TARGET_NAME) -lpjmedia-sfl-$(TARGET_NAME) -lpjsip-simple-sfl-$(TARGET_NAME) -lpjsip-ua-sfl-$(TARGET_NAME) -lpjmedia-codec-sfl-$(TARGET_NAME) -lpjlib-util-sfl-$(TARGET_NAME) -lpj-sfl-$(TARGET_NAME)

SIP_CFLAGS=-I$(src)/libs/pjproject-1.0.1/pjsip/include \
		   -I$(src)/libs/pjproject-1.0.1/pjlib/include \
		   -I$(src)/libs/pjproject-1.0.1/pjlib-util/include \
		   -I$(src)/libs/pjproject-1.0.1/pjmedia/include \
		   -I$(src)/libs/pjproject-1.0.1/pjnath/include

DBUSCPP_CFLAGS=$(top_srcdir)/libs/dbus-c++/include/dbus-c++

if BUILD_SPEEX
SPEEXCODEC=-DHAVE_SPEEX_CODEC
else
SPEEXCODEC= 
endif

if BUILD_GSM
GSMCODEC=-DHAVE_GSM_CODEC
else
GSMCODEC= 
endif

# Preprocessor flags
AM_CPPFLAGS = \
	-I$(src)/libs \
	-I$(src)/libs/dbus-c++/include \
	-I$(src)/libs/iax2 \
	-I$(src)/libs/pjproject-1.0.1 \
	-I$(src)/src \
	-I$(src)/test \
	-DPREFIX=\"$(prefix)\" \
	-DPROGSHAREDIR=\"${datadir}/sflphone\" \
	$(ZEROCONFFLAGS) \
	$(IAX_FLAGS) \
	$(SIP_CFLAGS) \
	$(DBUSCPP_CFLAGS) \
	-DCODECS_DIR=\""$(sflcodecdir)"\" \
	-DPLUGINS_DIR=\""$(sflplugindir)"\" \
	-DENABLE_TRACE \
	-DSFLDEBUG \
         $(SPEEXCODEC) \
         $(GSMCODEC)
