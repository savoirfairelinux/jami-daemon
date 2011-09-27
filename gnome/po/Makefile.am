# location of installation directories
locale_installdir=$(DESTDIR)$(datadir)/locale
DOMAIN=sflphone-client-gnome


POFILES=\
	 fr.po \
	 es.po \
	 de.po \
     zh_CN.po \
     zh_HK.po \
	 ru.po \
	 it.po \
	 pt_BR.po \
	 da.po

MOFILES=\
	 fr.mo \
	 es.mo \
	 de.mo \
     zh_CN.mo \
     zh_HK.mo \
	 ru.mo \
	 it.mo \
	 pt_BR.mo \
	 da.mo

MSGFMT=msgfmt

# necessary files
EXTRA_DIST= $(POFILES) $(MOFILES)

all:	$(MOFILES)	

# tells automake that toto.po makes toto.mo
SUFFIXES=.po .mo

.po.mo:
	$(MSGFMT) -o $@ $<

install-data-local: $(MOFILES)
	catalogs='$(MOFILES)'; \
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
	rm -f $(MOFILES)
