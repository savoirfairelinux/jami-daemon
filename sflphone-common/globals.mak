# Global variables
src=$(top_srcdir)
sflcodecdir=$(libdir)/sflphone/codecs
sflplugindir=$(libdir)/sflphone/plugins

PJSIP_LIBS = -lpjnath-sfl -lpjsua-sfl -lpjsip-sfl -lpjmedia-sfl -lpjsip-simple-sfl -lpjsip-ua-sfl -lpjmedia-codec-sfl -lpjlib-util-sfl -lpj-sfl 

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
	-I$(src)/src \
	-I$(src)/test \
	-DPREFIX=\"$(prefix)\" \
	-DPROGSHAREDIR=\"${datadir}/sflphone\" \
	$(ZEROCONFFLAGS) \
	$(IAX_FLAGS) \
	@SIP_CFLAGS@ \
	$(DBUSCPP_CFLAGS) \
	-DCODECS_DIR=\""$(sflcodecdir)"\" \
	-DPLUGINS_DIR=\""$(sflplugindir)"\" \
	-DENABLE_TRACE \
	-DSFLDEBUG \
         $(SPEEXCODEC) \
         $(GSMCODEC)
