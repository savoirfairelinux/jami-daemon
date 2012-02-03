# Global variables

src=$(abs_top_srcdir)
sfllibdir=$(DESTDIR)$(libdir)/sflphone
sflcodecdir=$(sfllibdir)/codecs
sflplugindir=$(sfllibdir)/plugins

ASTYLERC="$(top_srcdir)/../astylerc"
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
			-lpjlib-util-$(target) \
			-lpj-$(target)

SIP_CFLAGS=-I$(src)/libs/pjproject/pjsip/include \
		   -I$(src)/libs/pjproject/pjlib/include \
		   -I$(src)/libs/pjproject/pjlib-util/include \
		   -I$(src)/libs/pjproject/pjmedia/include \
		   -I$(src)/libs/pjproject/pjnath/include

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
	-I$(src)/libs/iax2 \
	-I$(src)/libs/pjproject \
	-I$(src)/src \
	-I$(src)/src/config \
	-I$(src)/test \
	$(SIP_CFLAGS) \
	@DBUSCPP_CFLAGS@ \
	-DPREFIX=\"$(prefix)\" \
	-DPROGSHAREDIR=\"${datadir}/sflphone\" \
	-DCODECS_DIR=\""$(sflcodecdir)"\" \
	-DPLUGINS_DIR=\""$(sflplugindir)"\" \
	-DENABLE_TRACE \
         $(SPEEXCODEC) \
         $(GSMCODEC)


indent:
	@echo "Indenting code:"
	if [ -f $(ASTYLERC) ] ; then \
                find $(top_srcdir)/src/ -name \*.cpp -o -name \*.h | xargs $(indent) --options=$(ASTYLERC) ; \
	fi
