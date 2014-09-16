#IAX

IAX_URL = https://gitlab.savoirfairelinux.com/sflphone/libiax2/repository/archive.tar.gz

PKGS += iax

$(TARBALLS)/libiax2.tar.gz:
	$(call download,$(IAX_URL))

.sum-iax: libiax2.tar.gz
	$(warning $@ not implemented)
	touch $@

iax: libiax2.tar.gz .sum-iax
	$(UNPACK)
	mv libiax2.git libiax2
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/iax/iax-win32.patch
	$(APPLY) $(SRC)/iax/iax-client.patch
endif
	mv libiax2 $@
	touch $@



.iax: iax
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
