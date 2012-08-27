# gsettings_SCHEMAS is a list of all the schemas you want to install
gsettings_SCHEMAS = org.sflphone.SFLphone.gschema.xml
# include the appropriate makefile rules for schema handling
@GSETTINGS_RULES@

EXTRA_DIST = $(gsettings_SCHEMAS)

MAINTAINERCLEANFILES = $(gsettings_SCHEMAS:.xml=.valid)


#FIXME: is there a way to avoid calling this every time we do make?
all-local: $(gsettings_SCHEMAS)
	$(MKDIR_P) $(top_builddir)/data/glib-2.0/schemas
	$(GLIB_COMPILE_SCHEMAS) $(top_srcdir)/data --targetdir $(top_builddir)/data/glib-2.0/schemas

clean-local:
	rm -rf $(top_builddir)/data/glib-2.0
