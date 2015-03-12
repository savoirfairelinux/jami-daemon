include $(top_srcdir)/globals.mak

noinst_LTLIBRARIES = libcodecdescriptor.la

libcodecdescriptor_la_SOURCES = audiocodecfactory.cpp audiocodec.cpp audiocodec.h audiocodecfactory.h
libcodecdescriptor_la_CXXFLAGS = -fPIC

if BUILD_GSM
GSM_LIB = libcodec_gsm.so
libcodec_gsm_so_SOURCES = gsmcodec.cpp
libcodec_gsm_so_CXXFLAGS = -fPIC -g -Wall
libcodec_gsm_so_LDFLAGS = --shared -lc -lgsm
libcodec_gsm_so_LDADD = libcodecdescriptor.la
INSTALL_GSM_RULE = install-libcodec_gsm_so
endif

if BUILD_SPEEX
SPEEX_LIB = libcodec_speex.so
libcodec_speex_so_SOURCES = speexcodec.cpp speexcodec.h
libcodec_speex_so_CXXFLAGS = -fPIC -g -Wall @SPEEX_CFLAGS@
libcodec_speex_so_LDFLAGS = --shared -lc @SPEEX_LIBS@
libcodec_speex_so_LDADD = libcodecdescriptor.la
INSTALL_SPEEX_RULE = install-libcodec_speex_so
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
libcodecdescriptor_la_CXXFLAGS += -DBUILD_ILBC
libcodec_ilbc_so_SOURCES = ilbc.cpp
libcodec_ilbc_so_CXXFLAGS = -fPIC -g -Wall $(libilbc_CFLAGS)
libcodec_ilbc_so_LDFLAGS = --shared -lc $(libilbc_LIBS)
libcodec_ilbc_so_LDADD = libcodecdescriptor.la
INSTALL_ILBC_RULE = install-libcodec_ilbc_so
endif

noinst_PROGRAMS = libcodec_ulaw.so libcodec_alaw.so libcodec_g722.so $(CODEC_g729) $(OPUS_LIB) $(GSM_LIB) $(SPEEX_LIB) $(ILBC_LIB)

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

#Temporary ldl mingw port exist
if !HAVE_WIN32
CODEC_g729 = libcodec_g729.so
libcodec_g729_so_SOURCES = g729.cpp g729.h
libcodec_g729_so_CXXFLAGS = -fPIC -g -Wall
libcodec_g729_so_LDADD = libcodecdescriptor.la
libcodec_g729_so_LDFLAGS = --shared -lc -lstdc++
INSTALL_G729_RULE = install-libcodec_g729_so
endif

install-exec-local:  install-libcodec_ulaw_so install-libcodec_alaw_so install-libcodec_g722_so $(INSTALL_G729_RULE) $(INSTALL_OPUS_RULE) $(INSTALL_GSM_RULE) $(INSTALL_SPEEX_NB_RULE) $(INSTALL_SPEEX_WB_RULE) $(INSTALL_SPEEX_UB_RULE) $(INSTALL_ILBC_RULE)
uninstall-local:  uninstall-libcodec_ulaw_so uninstall-libcodec_alaw_so uninstall-libcodec_g722_so uninstall-libcodec_opus_so uninstall-libcodec_gsm_so uninstall-libcodec_speex_nb_so uninstall-libcodec_speex_wb_so uninstall-libcodec_speex_ub_so uninstall-libcodec_ilbc_so

install-libcodec_ulaw_so: libcodec_ulaw.so$(EXEEXT)
	mkdir -p $(sflcodecdir)
	$(INSTALL_PROGRAM)  libcodec_ulaw.so$(EXEEXT) $(sflcodecdir)
install-libcodec_alaw_so: libcodec_alaw.so$(EXEEXT)
	mkdir -p $(sflcodecdir)
	$(INSTALL_PROGRAM)  libcodec_alaw.so$(EXEEXT) $(sflcodecdir)
install-libcodec_g722_so: libcodec_g722.so$(EXEEXT)
	mkdir -p $(sflcodecdir)
	$(INSTALL_PROGRAM)  libcodec_g722.so$(EXEEXT) $(sflcodecdir)
install-libcodec_opus_so: libcodec_opus.so$(EXEEXT)
	mkdir -p $(sflcodecdir)
	$(INSTALL_PROGRAM)  libcodec_opus.so$(EXEEXT) $(sflcodecdir)
install-libcodec_gsm_so: libcodec_gsm.so$(EXEEXT)
	mkdir -p $(sflcodecdir)
	$(INSTALL_PROGRAM)  libcodec_gsm.so$(EXEEXT) $(sflcodecdir)
install-libcodec_speex_nb_so: libcodec_speex_nb.so$(EXEEXT)
	mkdir -p $(sflcodecdir)
	$(INSTALL_PROGRAM)  libcodec_speex_nb.so$(EXEEXT) $(sflcodecdir)
install-libcodec_speex_wb_so: libcodec_speex_wb.so$(EXEEXT)
	mkdir -p $(sflcodecdir)
	$(INSTALL_PROGRAM)  libcodec_speex_wb.so$(EXEEXT) $(sflcodecdir)
install-libcodec_speex_ub_so: libcodec_speex_ub.so$(EXEEXT)
	mkdir -p $(sflcodecdir)
	$(INSTALL_PROGRAM)  libcodec_speex_ub.so $(sflcodecdir)
install-libcodec_ilbc_so: libcodec_ilbc.so$(EXEEXT)
	mkdir -p $(sflcodecdir)
	$(INSTALL_PROGRAM)  libcodec_ilbc.so$(EXEEXT) $(sflcodecdir)

uninstall-libcodec_ulaw_so:
	rm -f $(sflcodecdir)/libcodec_ulaw.so$(EXEEXT)
uninstall-libcodec_alaw_so:
	rm -f $(sflcodecdir)/libcodec_alaw.so$(EXEEXT)
uninstall-libcodec_g722_so:
	rm -f $(sflcodecdir)/libcodec_g722.so$(EXEEXT)
uninstall-libcodec_opus_so:
	rm -f $(sflcodecdir)/libcodec_opus.so$(EXEEXT)
uninstall-libcodec_gsm_so:
	rm -f $(sflcodecdir)/libcodec_gsm.so$(EXEEXT)
uninstall-libcodec_speex_nb_so:
	rm -f $(sflcodecdir)/libcodec_speex_nb.so$(EXEEXT)
uninstall-libcodec_speex_wb_so:
	rm -f $(sflcodecdir)/libcodec_speex_wb.so$(EXEEXT)
uninstall-libcodec_speex_ub_so:
	rm -f $(sflcodecdir)/libcodec_speex_ub.so$(EXEEXT)
uninstall-libcodec_ilbc_so:
	rm -f $(sflcodecdir)/libcodec_ilbc.so$(EXEEXT)
