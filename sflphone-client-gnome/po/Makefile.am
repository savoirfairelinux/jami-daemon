# location of installation directories
locale_installdir=$(DESTDIR)$(datadir)/locale
DOMAIN=sflphone-client-gnome


POFILES=\
	 fr/sflphone-client-gnome.po \
	 es/sflphone-client-gnome.po \
	 de/sflphone-client-gnome.po \
     zh_CN/sflphone-client-gnome.po \
     zh_HK/sflphone-client-gnome.po \
	 ru/sflphone-client-gnome.po

MOFILES=\
	 fr/sflphone-client-gnome.mo \
	 es/sflphone-client-gnome.mo \
	 de/sflphone-client-gnome.mo \
     zh_CN/sflphone-client-gnome.mo \
     zh_HK/sflphone-client-gnome.mo \
	 ru/sflphone-client-gnome.mo

MSGFMT=msgfmt

# necessary files
EXTRA_DIST= $(POFILES)

all:	$(MOFILES)	

# tells automake that toto.po makes toto.mo
SUFFIXES=.po .mo

.po.mo:
	$(MSGFMT) -o $@ $<

install-data-local:
	@catalogs='$(MOFILES)'; \
	for i in $$catalogs; do \
		destdir=$(locale_installdir); \
		lang=`echo $$i | cut -d / -f1`; \
		$(mkinstalldirs) $(locale_installdir)/$$lang/LC_MESSAGES; \
		$(INSTALL_DATA) $$i $(locale_installdir)/$$lang/LC_MESSAGES/$(DOMAIN).mo; \
		echo "installing $$i as $(locale_installdir)/$$lang/LC_MESSAGES/$(DOMAIN).mo"; \
	done	

uninstall-local:
	@catalogs='$(MOFILES)'; \
	for i in $$catalogs; do \
		destdir=$(locale_installdir); \
		lang=`echo $$i | sed 's/\.mo//'`; \
		rm -f $(locale_installdir)/$$lang/LC_MESSAGES/$(DOMAIN).mo; \
		echo "removing $$i as $(locale_installdir)/$$lang/LC_MESSAGES/$(DOMAIN).mo"; \
	done

clean-local:
	rm -rf *.mo
