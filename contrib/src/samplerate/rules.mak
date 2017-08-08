# SAMPLERATE
SAMPLERATE_VERSION := 0.1.8
SAMPLERATE_URL := http://www.mega-nerd.com/SRC/libsamplerate-$(SAMPLERATE_VERSION).tar.gz

PKGS += samplerate
ifeq ($(call need_pkg,"samplerate"),)
PKGS_FOUND += samplerate
endif

$(TARBALLS)/libsamplerate-$(SAMPLERATE_VERSION).tar.gz:
	$(call download,$(SAMPLERATE_URL))

.sum-samplerate: libsamplerate-$(SAMPLERATE_VERSION).tar.gz

samplerate: libsamplerate-$(SAMPLERATE_VERSION).tar.gz .sum-samplerate
	$(UNPACK)
	$(APPLY) $(SRC)/samplerate/disable_tests.patch
	$(APPLY) $(SRC)/samplerate/soundcard.patch
	$(APPLY) $(SRC)/samplerate/carbon.patch
ifdef HAVE_IOS
ifeq ($(IOS_TARGET_PLATFORM),iPhoneSimulator)
#warning assembler double / int conversion disabled
	$(APPLY) $(SRC)/samplerate/disable_assembler.patch
endif
endif
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR) && mv config.guess config.sub Cfg
	$(MOVE)

.samplerate: samplerate
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
