# gsettings_SCHEMAS is a list of all the schemas you want to install
gsettings_SCHEMAS = org.sflphone.SFLphone.gschema.xml
# include the appropriate makefile rules for schema handling
@GSETTINGS_RULES@

EXTRA_DIST = $(gsettings_SCHEMAS)

MAINTAINERCLEANFILES = $(gsettings_SCHEMAS:.xml=.valid)

#FIXME: is there a way to avoid calling this every time we do make?
all-local: gschemas.compiled

gschemas.compiled: $(gsettings_SCHEMAS)
	$(GLIB_COMPILE_SCHEMAS) $(top_srcdir)/data --targetdir $(top_builddir)/data

clean-local:
	rm -rf $(top_builddir)/data/gschemas.compiled

-include $(top_srcdir)/git.mk
