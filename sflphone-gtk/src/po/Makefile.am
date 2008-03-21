# location of installation directories
locale_installdir=$(datadir)/locale
DOMAIN=sflphone

# tells automake that toto.po makes toto.mo
SUFFIXES=.po .mo

POFILES=\
	 fr.po

MSGFMT=msgfmt

# necessary files
EXTRA_DIST= $(PO_FILES)

#.po.mo:
#	$(MSGFMT) -o $@ $<

all:	fr.mo

fr.mo:	fr.po
	$(MSGFMT) -o $@ $<

install:
	$(mkinstalldirs) $(locale_installdir)/fr/LC_MESSAGES; \
	$(INSTALL_DATA) fr.mo $(locale_installdir)/fr/LC_MESSAGES/$(DOMAIN).mo; 

uninstall:
	rm -f $(locale_installdir)/fr/LC_MESSAGES/$(DOMAIN).mo; 

clean:
	rm -rf *.mo
