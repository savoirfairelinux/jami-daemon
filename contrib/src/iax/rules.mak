#IAX

IAX_URL = https://gitlab.savoirfairelinux.com/sflphone/libiax2/repository/archive.tar.gz

PKGS += iax

$(TARBALLS)/iax-git.tar.gz:
	$(call download,$(IAX_URL))

.sum-iax: iax-git.tar.gz
	$(warning $@ not implemented)
	touch $@

iax: iax-git.tar.gz .sum-iax
	$(UNPACK)
	mv libiax2.git $@
	touch $@


.iax: iax
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
