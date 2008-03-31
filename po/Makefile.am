# location of installation directories
locale_installdir=$(datadir)/locale
DOMAIN=sflphone

POFILES=\
	 fr.po \
	 es.po \
	 de.po

MOFILES=\
	 fr.mo \
	 es.mo \
	 de.mo

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
		lang=`echo $$i | sed 's/\.mo//'`; \
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
