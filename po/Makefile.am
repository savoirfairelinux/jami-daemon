# location of installation directories
locale_installdir=$(datadir)/locale
DOMAIN=sflphone

POTFILE=sflphone.pot

POFILES=\
	 fr.po \
	 es.po \
	 de.po \
     zh_CN.po \
     zh_HK.po \
	 ru.po

MOFILES=\
	 fr.mo \
	 es.mo \
	 de.mo \
     zh_CN.mo \
     zh_HK.mo \
	 ru.mo

MSGFMT=msgfmt
MSGMERGE=msgmerge

# necessary files
EXTRA_DIST= $(POFILES)

all:	$(MOFILES)	

# tells automake that toto.po makes toto.mo
SUFFIXES=.po .mo

.po.mo:
	$(MSGMERGE) --update $< $(POTFILE) 
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
