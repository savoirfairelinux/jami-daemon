include $(top_srcdir)/globals.mak

noinst_LTLIBRARIES = libcodecdescriptor.la

libcodecdescriptor_la_SOURCES = audiocodecfactory.cpp audiocodec.cpp audiocodec.h audiocodecfactory.h
libcodecdescriptor_la_LIBADD = $(CCRTP_LIBS)

if BUILD_GSM
GSM_LIB = libcodec_gsm.so
libcodec_gsm_so_SOURCES = gsmcodec.cpp
libcodec_gsm_so_CXXFLAGS = -fPIC -g -Wall
libcodec_gsm_so_LDFLAGS = --shared -lc -lgsm
libcodec_gsm_so_LDADD = libcodecdescriptor.la
INSTALL_GSM_RULE = install-libcodec_gsm_so
endif

if BUILD_SPEEX
SPEEX_NB_LIB = libcodec_speex_nb.so
libcodec_speex_nb_so_SOURCES = speexcodec_nb.cpp speexcodec.h
libcodec_speex_nb_so_CXXFLAGS = -fPIC -g -Wall $(SPEEXDSP)
libcodec_speex_nb_so_LDFLAGS = --shared -lc -lspeex
libcodec_speex_nb_so_LDADD = libcodecdescriptor.la
INSTALL_SPEEX_NB_RULE = install-libcodec_speex_nb_so

SPEEX_WB_LIB = libcodec_speex_wb.so
libcodec_speex_wb_so_SOURCES = speexcodec_wb.cpp speexcodec.h
libcodec_speex_wb_so_CXXFLAGS = -fPIC -g -Wall $(SPEEXDSP)
libcodec_speex_wb_so_LDFLAGS = --shared -lc -lspeex
libcodec_speex_wb_so_LDADD = libcodecdescriptor.la
INSTALL_SPEEX_WB_RULE = install-libcodec_speex_wb_so

SPEEX_UB_LIB = libcodec_speex_ub.so
libcodec_speex_ub_so_SOURCES = speexcodec_ub.cpp speexcodec.h
libcodec_speex_ub_so_CXXFLAGS = -fPIC -g -Wall $(SPEEXDSP)
libcodec_speex_ub_so_LDFLAGS = --shared -lc -lspeex
libcodec_speex_ub_so_LDADD = libcodecdescriptor.la
INSTALL_SPEEX_UB_RULE = install-libcodec_speex_ub_so
endif

if BUILD_OPUS
OPUS_LIB = libcodec_opus.so
libcodec_opus_so_SOURCES  = opuscodec.cpp opuscodec.h
libcodec_opus_so_CXXFLAGS = -fPIC -g -Wall $(opus_CFLAGS)
libcodec_opus_so_LDFLAGS  = --shared -lc -lstdc++ $(opus_LIBS)
libcodec_opus_so_LDADD    = libcodecdescriptor.la
INSTALL_OPUS_RULE = install-libcodec_opus_so
endif

if BUILD_ILBC
ILBC_LIB = libcodec_ilbc.so
libcodecdescriptor_la_CXXFLAGS = -DBUILD_ILBC
libcodec_ilbc_so_SOURCES = ilbc.cpp
libcodec_ilbc_so_CXXFLAGS = -fPIC -g -Wall $(libilbc_CFLAGS)
libcodec_ilbc_so_LDFLAGS = --shared -lc $(libilbc_LIBS)
libcodec_ilbc_so_LDADD = libcodecdescriptor.la
INSTALL_ILBC_RULE = install-libcodec_ilbc_so
endif

noinst_PROGRAMS = libcodec_ulaw.so libcodec_alaw.so libcodec_g722.so libcodec_g729.so $(OPUS_LIB) $(GSM_LIB) $(SPEEX_NB_LIB) $(SPEEX_WB_LIB) $(SPEEX_UB_LIB) $(ILBC_LIB)

libcodec_ulaw_so_SOURCES = ulaw.cpp g711.h
libcodec_ulaw_so_CXXFLAGS = -fPIC -g -Wall
libcodec_ulaw_so_LDADD = libcodecdescriptor.la
libcodec_ulaw_so_LDFLAGS = --shared -lc

libcodec_alaw_so_SOURCES = alaw.cpp g711.h
libcodec_alaw_so_CXXFLAGS = -fPIC -g -Wall
libcodec_alaw_so_LDADD = libcodecdescriptor.la
libcodec_alaw_so_LDFLAGS = --shared -lc

libcodec_g722_so_SOURCES = g722.cpp g722.h
libcodec_g722_so_CXXFLAGS = -fPIC -g -Wall
libcodec_g722_so_LDADD = libcodecdescriptor.la
libcodec_g722_so_LDFLAGS = --shared -lc

libcodec_g729_so_SOURCES = g729.cpp g729.h
libcodec_g729_so_CXXFLAGS = -fPIC -g -Wall
libcodec_g729_so_LDADD = libcodecdescriptor.la
libcodec_g729_so_LDFLAGS = --shared -lc -ldl -lstdc++

install-exec-local:  install-libcodec_ulaw_so install-libcodec_alaw_so install-libcodec_g722_so install-libcodec_g729_so $(INSTALL_OPUS_RULE) $(INSTALL_GSM_RULE) $(INSTALL_SPEEX_NB_RULE) $(INSTALL_SPEEX_WB_RULE) $(INSTALL_SPEEX_UB_RULE) $(INSTALL_ILBC_RULE)
uninstall-local:  uninstall-libcodec_ulaw_so uninstall-libcodec_alaw_so uninstall-libcodec_g722_so uninstall-libcodec_g729_so uninstall-libcodec_opus_so uninstall-libcodec_gsm_so uninstall-libcodec_speex_nb_so uninstall-libcodec_speex_wb_so uninstall-libcodec_speex_ub_so uninstall-libcodec_ilbc_so

install-libcodec_ulaw_so: libcodec_ulaw.so
	mkdir -p $(sflcodecdir)
	$(INSTALL_PROGRAM)  libcodec_ulaw.so $(sflcodecdir)
install-libcodec_alaw_so: libcodec_alaw.so
	mkdir -p $(sflcodecdir)
	$(INSTALL_PROGRAM)  libcodec_alaw.so $(sflcodecdir)
install-libcodec_g722_so: libcodec_g722.so
	mkdir -p $(sflcodecdir)
	$(INSTALL_PROGRAM)  libcodec_g722.so $(sflcodecdir)
install-libcodec_g729_so: libcodec_g729.so
	mkdir -p $(sflcodecdir)
	$(INSTALL_PROGRAM)  libcodec_g729.so $(sflcodecdir)
install-libcodec_opus_so: libcodec_opus.so
	mkdir -p $(sflcodecdir)
	$(INSTALL_PROGRAM)  libcodec_opus.so $(sflcodecdir)
install-libcodec_gsm_so: libcodec_gsm.so
	mkdir -p $(sflcodecdir)
	$(INSTALL_PROGRAM)  libcodec_gsm.so $(sflcodecdir)
install-libcodec_speex_nb_so: libcodec_speex_nb.so
	mkdir -p $(sflcodecdir)
	$(INSTALL_PROGRAM)  libcodec_speex_nb.so $(sflcodecdir)
install-libcodec_speex_wb_so: libcodec_speex_wb.so
	mkdir -p $(sflcodecdir)
	$(INSTALL_PROGRAM)  libcodec_speex_wb.so $(sflcodecdir)
install-libcodec_speex_ub_so: libcodec_speex_ub.so
	mkdir -p $(sflcodecdir)
	$(INSTALL_PROGRAM)  libcodec_speex_ub.so $(sflcodecdir)
install-libcodec_ilbc_so: libcodec_ilbc.so
	mkdir -p $(sflcodecdir)
	$(INSTALL_PROGRAM)  libcodec_ilbc.so $(sflcodecdir)

uninstall-libcodec_ulaw_so:
	rm -f $(sflcodecdir)/libcodec_ulaw.so
uninstall-libcodec_alaw_so:
	rm -f $(sflcodecdir)/libcodec_alaw.so
uninstall-libcodec_g722_so:
	rm -f $(sflcodecdir)/libcodec_g722.so
uninstall-libcodec_g729_so:
	rm -f $(sflcodecdir)/libcodec_g729.so
uninstall-libcodec_opus_so:
	rm -f $(sflcodecdir)/libcodec_opus.so
uninstall-libcodec_gsm_so:
	rm -f $(sflcodecdir)/libcodec_gsm.so
uninstall-libcodec_speex_nb_so:
	rm -f $(sflcodecdir)/libcodec_speex_nb.so
uninstall-libcodec_speex_wb_so:
	rm -f $(sflcodecdir)/libcodec_speex_wb.so
uninstall-libcodec_speex_ub_so:
	rm -f $(sflcodecdir)/libcodec_speex_ub.so
uninstall-libcodec_ilbc_so:
	rm -f $(sflcodecdir)/libcodec_ilbc.so

