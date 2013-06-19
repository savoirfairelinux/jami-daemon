include $(top_srcdir)/globals.mak

XSLTPROC=xsltproc --xinclude --nonet
PYTHON=python

XMLS=	$(wildcard $(top_srcdir)/src/dbus/spec/*.xml)
TEMPLATES=	$(wildcard doc/templates/*)

GENERATED_FILES =   \
	doc/spec.html   \
	doc/spec/index.html

doc/spec.html: $(XMLS) tools/doc-generator.xsl
	@install -d tmp/doc
	$(XSLTPROC) tools/doc-generator.xsl spec/all.xml > tmp/$@
	mv tmp/$@ $@

doc/spec/index.html: $(XMLS) tools/doc-generator.py tools/specparser.py $(TEMPLATES)
	@install -d tmp/doc
	$(PYTHON) tools/doc-generator.py spec/all.xml doc/spec sflphone-spec org.sflphone.SFLphone

doc:	$(GENERATED_FILES)

clean-local:
	rm -rf $(GENERATED_FILES)
	rm -rf doc/spec
	rm -rf tmp

