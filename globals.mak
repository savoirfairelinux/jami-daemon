# Global variables
src=$(top_srcdir)
sflcodecdir=$(libdir)/sflphone/codecs

PJSIP_LIBS = -lpjnath -lpjsua -lpjsip -lpjmedia -lpjsip-simple -lpjsip-ua -lpjmedia-codec -lpjlib-util -lpj 

# Preprocessor flags
AM_CPPFLAGS = \
	-I$(src)/libs \
	-I$(src)/src \
	-DPREFIX=\"$(prefix)\" \
	-DPROGSHAREDIR=\"${datadir}/sflphone\" \
	$(ZEROCONFFLAGS) \
	$(IAX_FLAGS) \
	@SIP_CFLAGS@ \
	@DBUSCPP_CFLAGS@ \
	-DCODECS_DIR=\""$(sflcodecdir)"\" \
	-DENABLE_TRACE

