# Global variables

src=$(abs_top_srcdir)
ringlibdir=$(DESTDIR)$(libdir)/ring

ASTYLERC="$(top_srcdir)/../astylerc"
indent="/usr/bin/astyle"

if BUILD_SPEEX
SPEEXCODEC=-DHAVE_SPEEX_CODEC
endif

if BUILD_OPUS
OPUSCODEC=-DHAVE_OPUS
endif

if BUILD_GSM
GSMCODEC=-DHAVE_GSM_CODEC
endif

# Preprocessor flags
AM_CPPFLAGS = \
	-I$(src)/src \
	-I$(src)/src/config \
	-I$(src)/src/media \
	-I$(src)/test \
	-I$(src)/src/dring \
	$(SIP_CFLAGS) \
	-DPREFIX=\"$(prefix)\" \
	-DPROGSHAREDIR=\"${datadir}/ring\" \
	-DENABLE_TRACE \
	$(SPEEXCODEC) \
	$(GSMCODEC) \
	$(OPUSCODEC)


indent:
	@echo "Indenting code:"
	if [ -f $(ASTYLERC) ] ; then \
                find $(top_srcdir)/src/ -name \*.cpp -o -name \*.h | xargs $(indent) --options=$(ASTYLERC) ; \
	fi
