# SNDFILE
SNDFILE_VERSION := 1.0.25
SNDFILE_URL := http://www.mega-nerd.com/libsndfile/files/libsndfile-$(SNDFILE_VERSION).tar.gz

PKGS += sndfile
ifeq ($(call need_pkg,"sndfile"),)
PKGS_FOUND += sndfile
endif

DEPS_sndfile = ogg vorbis flac

$(TARBALLS)/libsndfile-$(SNDFILE_VERSION).tar.gz:
	$(call download,$(SNDFILE_URL))

.sum-sndfile: libsndfile-$(SNDFILE_VERSION).tar.gz

sndfile: libsndfile-$(SNDFILE_VERSION).tar.gz .sum-sndfile
	$(UNPACK)
	$(APPLY) $(SRC)/sndfile/soundcard.patch
	$(APPLY) $(SRC)/sndfile/carbon.patch
	$(APPLY) $(SRC)/sndfile/autotools.patch
	$(APPLY) $(SRC)/sndfile/disable_programs.patch
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR) && mv config.guess config.sub Cfg && autoreconf -fi
ifdef HAVE_IOS
	rm -Rf examples
endif
	$(MOVE)

.sndfile: sndfile
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
