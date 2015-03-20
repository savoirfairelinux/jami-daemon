if ENABLE_DOXYGEN

html_parent_dir = .
HTML_HEADER =
HTML_FOOTER =

all: doxygen-trac
	
.PHONY: doxygen-trac

doxygen-trac: clean core-doc-trac gtk-gui-doc-trac

%-doc-trac : %-doc
	ln -sf ${html_parent_dir}/$< ${html_parent_dir}/$</html

doc: clean core-doc gtk-gui-doc

%-doc : %-doc.cfg
	rm -rf $@/ refman.pdf
	$(DOXYGEN) $<
#	$(MAKE) -C latex/
#	mv latex/refman.pdf ./refman.pdf


clean-local:
	echo "clean-local: " && pwd
	rm -rf latex/
	rm -rf ${html_parent_dir}/*-doc/
	rm -f *~
	rm -f doxygen.log
	rm -f doxygen.cfg

maintainer-clean-local: clean-local
	echo "maintainer-clean-local: " && pwd
	rm -rf html refman.pdf

## We borrow guile's convention and use @-...-@ as the substitution
## brackets here, instead of the usual @...@.  This prevents autoconf
## from substituting the values directly into the left-hand sides of
## the sed substitutions.
%.cfg : %.cfg.in
	rm -f $@.tmp
	sed < $< > $@.tmp \
            -e 's:@-top_srcdir-@:${top_srcdir}:g'
	sed < $@.tmp > $@.tmp2 \
            -e 's:@-html_dir-@:${html_parent_dir}/$*:g'
	rm $@.tmp
	sed < $@.tmp2 > $@.tmp3 \
            -e 's:@-html_header-@:${HTML_HEADER}:g'
	rm $@.tmp2
	sed < $@.tmp3 > $@.tmp4 \
            -e 's:@-html_footer-@:${HTML_FOOTER}:g'
	rm $@.tmp3
	mv $@.tmp4 $@

endif # ENABLE_DOXYGEN
