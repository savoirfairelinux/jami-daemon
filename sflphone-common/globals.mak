# Global variables
src=$(top_srcdir)
sflcodecdir=$(DESTDIR)$(libdir)/sflphone/codecs
sflplugindir=$(DESTDIR)$(libdir)/sflphone/plugins

PJSIP_VERSION="1.0.2"

# for pjsip
PJSIP_LIBS= \
			-L$(src)/libs/pjproject-$(PJSIP_VERSION)/pjnath/lib/ \
			-L$(src)/libs/pjproject-$(PJSIP_VERSION)/pjsip/lib/ \
			-L$(src)/libs/pjproject-$(PJSIP_VERSION)/pjlib/lib/ \
			-L$(src)/libs/pjproject-$(PJSIP_VERSION)/pjlib-util/lib/ \
			-L$(src)/libs/pjproject-$(PJSIP_VERSION)/pjmedia/lib/ \
			-lpjnath-sfl-$(target) \
			-lpjsua-sfl-$(target) \
			-lpjsip-sfl-$(target) \
			-lpjmedia-sfl-$(target) \
			-lpjsip-simple-sfl-$(target) \
			-lpjsip-ua-sfl-$(target) \
			-lpjmedia-codec-sfl-$(target) \
			-lpjlib-util-sfl-$(target) \
			-lpj-sfl-$(target)

SIP_CFLAGS=-I$(src)/libs/pjproject-$(PJSIP_VERSION)/pjsip/include \
		   -I$(src)/libs/pjproject-$(PJSIP_VERSION)/pjlib/include \
		   -I$(src)/libs/pjproject-$(PJSIP_VERSION)/pjlib-util/include \
		   -I$(src)/libs/pjproject-$(PJSIP_VERSION)/pjmedia/include \
		   -I$(src)/libs/pjproject-$(PJSIP_VERSION)/pjnath/include

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
	-I$(src)/libs/pjproject-$(PJSIP_VERSION) \
	-I$(src)/src \
	-I$(src)/src/config \
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
