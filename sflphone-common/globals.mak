# Global variables
src=$(top_srcdir)
sfllibdir=$(DESTDIR)$(libdir)/sflphone
sflcodecdir=$(sfllibdir)/codecs
sflplugindir=$(sfllibdir)/plugins

ASTYLERC="../astylerc"
indent="/usr/bin/astyle"

# for pjsip
PJSIP_LIBS= \
			-L$(src)/libs/pjproject/pjnath/lib/ \
			-L$(src)/libs/pjproject/pjsip/lib/ \
			-L$(src)/libs/pjproject/pjlib/lib/ \
			-L$(src)/libs/pjproject/pjlib-util/lib/ \
			-L$(src)/libs/pjproject/pjmedia/lib/ \
			-lpjnath-$(target) \
			-lpjsua-$(target) \
			-lpjsip-$(target) \
			-lpjmedia-$(target) \
			-lpjsip-simple-$(target) \
			-lpjsip-ua-$(target) \
			-lpjmedia-codec-$(target) \
			-lpjlib-util-$(target) \
			-lpj-$(target) 

SIP_CFLAGS=-I$(src)/libs/pjproject/pjsip/include \
		   -I$(src)/libs/pjproject/pjlib/include \
		   -I$(src)/libs/pjproject/pjlib-util/include \
		   -I$(src)/libs/pjproject/pjmedia/include \
		   -I$(src)/libs/pjproject/pjnath/include

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
	-I$(src)/libs/pjproject \
	-I$(src)/src \
	-I$(src)/src/config \
	-I$(src)/test \
	-DPREFIX=\"$(prefix)\" \
	-DPROGSHAREDIR=\"${datadir}/sflphone\" \
	$(ZEROCONFFLAGS) \
	$(GSTREAMER_CFLAGS) \
	$(LIBAVCODEC_CFLAGS) \
	$(LIBSWSCALE_CFLAGS) \
	$(IAX_FLAGS) \
	$(SIP_CFLAGS) \
	-DCODECS_DIR=\""$(sflcodecdir)"\" \
	-DPLUGINS_DIR=\""$(sflplugindir)"\" \
	-DENABLE_TRACE \
         $(SPEEXCODEC) \
         $(GSMCODEC)
