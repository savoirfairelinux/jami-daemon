# Global variables
src=$(top_srcdir)
sflcodecdir=$(libdir)/sflphone/codecs

# Preprocessor flags
AM_CPPFLAGS = \
	-I$(src)/libs \
	-I$(src)/src \
	-I$(src)/test \
	-DPREFIX=\"$(prefix)\" \
	-DPROGSHAREDIR=\"${datadir}/sflphone\" \
	$(ZEROCONFFLAGS) \
	$(IAX_FLAGS) \
	@SIP_CFLAGS@ \
	@DBUSCPP_CFLAGS@ \
	-DCODECS_DIR=\""$(sflcodecdir)"\" \
	-DENABLE_TRACE

