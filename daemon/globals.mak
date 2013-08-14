# Global variables

src=$(abs_top_srcdir)
sfllibdir=$(DESTDIR)$(libdir)/sflphone
sflcodecdir=$(sfllibdir)/codecs
sflplugindir=$(sfllibdir)/plugins

ASTYLERC="$(top_srcdir)/../astylerc"
indent="/usr/bin/astyle"

# for pjsip
include $(src)/libs/pjproject-2.1.0/build.mak
PJSIP_LIBS=$(APP_LDFLAGS) $(APP_LDLIBS)

SIP_CFLAGS=-I$(src)/libs/pjproject-2.1.0/pjsip/include \
		   -I$(src)/libs/pjproject-2.1.0/pjlib/include \
		   -I$(src)/libs/pjproject-2.1.0/pjlib-util/include \
		   -I$(src)/libs/pjproject-2.1.0/pjmedia/include \
		   -I$(src)/libs/pjproject-2.1.0/pjnath/include \
		   -DPJ_AUTOCONF=1

if BUILD_SPEEX
SPEEXCODEC=-DHAVE_SPEEX_CODEC
else
SPEEXCODEC=
endif

if BUILD_OPUS
OPUSCODEC=-DHAVE_OPUS
else
OPUSCODEC=
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
	-I$(src)/libs/pjproject-2.1.0 \
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
	$(GSMCODEC) \
	$(OPUSCODEC)


indent:
	@echo "Indenting code:"
	if [ -f $(ASTYLERC) ] ; then \
                find $(top_srcdir)/src/ -name \*.cpp -o -name \*.h | xargs $(indent) --options=$(ASTYLERC) ; \
	fi
