src = $(top_srcdir)
AM_CPPFLAGS = -I$(src)/libs -I$(src)/src  -DPREFIX=\"$(prefix)\" -DPROGSHAREDIR=\"${datadir}/sflphone\" $(ZEROCONFFLAGS) $(IAX_FLAGS) $(SFLPHONE_CFLAGS) $(SIP_CFLAGS)
