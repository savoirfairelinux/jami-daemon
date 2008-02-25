SUBDIRS =

docdir = ${prefix}/share/doc/wifidog-@VERSION@
html_parent_dir = .

all:

doc: core-doc gtk-gui-doc
%-doc : %-doc.cfg
	rm -rf $@-html/ refman.pdf
	$(DOXYGEN) $<
#	$(MAKE) -C latex/ 
#	mv latex/refman.pdf ./refman.pdf


clean-local:
	echo "clean-local: " && pwd
	rm -rf latex/
	rm -rf ${html_parent_dir}/*-html/
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
            -e 's:@-htmldir-@:${html_parent_dir}/$*-html:g'
	rm $@.tmp
	mv $@.tmp2 $@