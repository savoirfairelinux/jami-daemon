# location of installation directories
locale_installdir=$(DESTDIR)$(datadir)/locale
DOMAIN=sflphone-client-gnome


POFILES=\
	 fr/fr.po \
	 es/es.po \
	 de/de.po \
     zh_CN/zh_CN.po \
     zh_HK/zh_HK.po \
	 ru/ru.po \
	 it/it.po \
	 pt_BR/pt_BR.po \
	 da/da.po

MOFILES=\
	 fr/fr.mo \
	 es/es.mo \
	 de/de.mo \
     zh_CN/zh_CN.mo \
     zh_HK/zh_HK.mo \
	 ru/ru.mo \
	 it/it.mo \
	 pt_BR/pt_BR.mo \
	 da/da.mo

MSGFMT=msgfmt

# necessary files
EXTRA_DIST= $(POFILES) $(MOFILES)

all:	$(MOFILES)	

# tells automake that toto.po makes toto.mo
SUFFIXES=.po .mo

.po.mo:
	$(MSGFMT) -o $@ $<

install-data-local: $(MOFILES)
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
	rm -f $(MOFILES)
